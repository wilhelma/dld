#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <boost/dynamic_bitset.hpp>

#include "pin.H"

// types
typedef std::set<ADDRINT> leaders_t;
typedef std::map<ADDRINT, ADDRINT> jumps_t;
typedef std::map<ADDRINT, std::string> dissmap_t;

dissmap_t dissmap;

/*
 * class BasicBlock
 */
class BasicBlock {
public:

    // types
    typedef size_t id_t;
    typedef std::vector<BasicBlock*> bbls_t;

    // attributes
    id_t id;						// id of the basic block (starting from 0)
    ADDRINT entry_ins;				// address of the first instruction
    ADDRINT exit_ins;				// address of the last instruction
    std::set<id_t> predecessors;	// set of id's of all predecessors
    std::set<id_t> dominators;		// set of id's of all dominators
    BasicBlock* successor;			// pointer to the successor
    BasicBlock* target;			// pointer to the target (if existing)
    BasicBlock* iDom;				// immediate dominator

    //--------------------------------------------------------------------------

    // the constructor
    explicit BasicBlock( ADDRINT entry_ins,
                         BasicBlock* predecessor )
            : id(gID++), entry_ins(entry_ins),
              successor(nullptr), target(nullptr), iDom(nullptr) {

        if (predecessor)
            predecessors.insert( predecessor->id );
        gBasicBlocks.push_back(this);
    }

    // resets the static information
    static void reset() {

        gID = 0;
        gBasicBlocks.clear();
    }

    // get pointer to the basic block with given id
    static BasicBlock* get(id_t id) {

        return gBasicBlocks[id];
    }

    // get current number of inserted basic blocks
    static size_t size() {

        return gBasicBlocks.size();
    }

    bool isPredecessor(id_t searchID) {

        for (id_t predID : predecessors) {
            if (predID > id)
                continue;
            if (predID == searchID)
                return true;
            if (BasicBlock::get(predID)->isPredecessor(searchID))
                return true;
        }

        return false;
    }

private:
    static BasicBlock::id_t gID;
    static bbls_t gBasicBlocks;
};
BasicBlock::id_t BasicBlock::gID = 0;
BasicBlock::bbls_t BasicBlock::gBasicBlocks;

struct Loop {
    typedef std::map<ADDRINT, Loop*> insLoopMap_t;

    BasicBlock::id_t head;
    BasicBlock::id_t tail;
    std::set<BasicBlock::id_t> nodes;
    std::set<BasicBlock::id_t> exits;

    static insLoopMap_t insLoopMap;
};
Loop::insLoopMap_t Loop::insLoopMap;


/*******************************************************************************
 * global functions
 ******************************************************************************/

/* getBBLFrontiers()
 *
 * iterates over all instruction in a given routine rtn to:
 * 	- identify the entry instruction of every static (compiler) BBL (=leaders)
 *  - identify all unconditional jumps (=jumps)
 */
void getBBLFrontiers( RTN rtn,
                    leaders_t& leaders,
                    jumps_t& jumps ) {

    bool isFirst = true;

    RTN_Open(rtn);
    for (INS in = RTN_InsHead(rtn); INS_Valid(in); in = INS_Next(in)) {

        dissmap[INS_Address(in)] = INS_Disassemble(in);

        if (isFirst) {
            leaders.insert( INS_Address(in) );
            isFirst = false;
        }


        if (INS_IsDirectBranchOrCall(in)) {
            ADDRINT target = INS_DirectBranchOrCallTargetAddress(in);
            leaders.insert( target );
            jumps[INS_Address(in)] = target;
            isFirst = true;
        }
    }
    RTN_Close(rtn);
}

/* getStaticBBLs()
 *
 * builds a cfg consisting of newly allocated bbl's and returning the root of
 * the cfg.
 * This is done by considering each leader instruction as the beginning of a
 * bbl, each transition as a successor relation, and each jump as a target
 * relation. The predecessor attribute is filled accordingly to the successor
 * and target relation. The dominator attribute is not filled here (see
 * getDominators()).
 */
BasicBlock* getStaticBBLs( RTN rtn,
                           const leaders_t& leaders,
                           const jumps_t& jumps ) {

    std::map<ADDRINT, BasicBlock*> leaderBBLMap, exitBBLMap;
    BasicBlock *root = nullptr, *curBBL = nullptr;

    RTN_Open(rtn);

    // head instruction
    INS in = RTN_InsHead(rtn);
    if (INS_Valid(in)) {
        ADDRINT insAddr = INS_Address(in);
        root = curBBL = new BasicBlock( insAddr, nullptr );
        leaderBBLMap[insAddr] = curBBL;
        curBBL->exit_ins = insAddr;
        in = INS_Next(in);
    }
    
    // other instructions
    bool hasFallThrough = true;
    while (INS_Valid(in)) {
        ADDRINT insAddr = INS_Address(in);
        if (leaders.find( insAddr ) != leaders.end()) {
            exitBBLMap[curBBL->exit_ins] = curBBL;
            if (hasFallThrough) {
                BasicBlock **successor = &curBBL->successor;
                curBBL = new BasicBlock( insAddr, curBBL );
                *successor = curBBL;
            } else
                curBBL = new BasicBlock( insAddr, nullptr );
            leaderBBLMap[insAddr] = curBBL;
        }
        curBBL->exit_ins = insAddr;
        hasFallThrough = INS_HasFallThrough( in ) || INS_IsCall( in );
        in = INS_Next(in);
    }
    RTN_Close(rtn);

    // consider jump instructions (for targets/predecessors)
    for ( auto edge : jumps ) {
        auto source = exitBBLMap.find( edge.first );
        auto target = leaderBBLMap.find( edge.second );

        if (source != exitBBLMap.end()) {
            if (target != leaderBBLMap.end()) {
                target->second->predecessors.insert(source->second->id);
                source->second->target = target->second;
            }
        }
    }

    return root;
}

/* getDominators()
 *
 * computes the dominators of all basic blocks in the cfg
 */
void getDominators( BasicBlock* root ) {

    const size_t nBBL = BasicBlock::size();
    std::unique_ptr< boost::dynamic_bitset<> >* domBitSets =
            new std::unique_ptr< boost::dynamic_bitset<> >[nBBL];

    // for each bbl, create a dominator bitset for efficient set operations
    for (size_t i=0; i<nBBL; ++i)
        domBitSets[i] = std::unique_ptr< boost::dynamic_bitset<> >(
                    new boost::dynamic_bitset<>(nBBL) );
    domBitSets[0]->reset();
    domBitSets[0]->set(0);
    for (size_t i=1; i<nBBL; ++i)
      domBitSets[i]->set();

    // compute dominators (fixpoint algorithm)
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i=1; i<nBBL; ++i) {
            boost::dynamic_bitset<> tmp(*domBitSets[i].get());
            for (BasicBlock::id_t pred : BasicBlock::get(i)->predecessors)
                *domBitSets[i].get() &= *domBitSets[pred].get();
            domBitSets[i]->set(i);
            if (tmp != *domBitSets[i].get())
                changed = true;
        }
    }

    // fill dominators in bbl's
    for (size_t i=0; i<nBBL; ++i)
        for (size_t j=0; j<nBBL; ++j)
            if (domBitSets[i]->test(j))
                BasicBlock::get(i)->dominators.insert(j);

    // compute immediate dominators
    for (size_t i=1; i<nBBL; ++i) {
      for (BasicBlock::id_t m : BasicBlock::get(i)->dominators) {
        bool isIDom = true;
        if (m == i)
          continue;

        for (BasicBlock::id_t p : BasicBlock::get(i)->dominators) {
          if (p == i || p == m)
            continue;
          if (!domBitSets[m]->test(p)) {
            isIDom = false;
            break;
          }
        }

        if (isIDom) {
          BasicBlock::get(i)->iDom = BasicBlock::get(m);
          break;
        }
      }
    }

}

void collectLoop(id_t header,
                 id_t current,
                 Loop& loop,
                 bool initial = true) {

    static std::set<id_t> visited;

    if (initial)
        visited.clear();

    if (visited.find(current) != visited.end())
        return;
    visited.insert(current);

    BasicBlock *tmp = BasicBlock::get(current);
    if (header == tmp->id)
        return;

    if (tmp->dominators.find(header) != tmp->dominators.end()) {
        loop.nodes.insert(tmp->id);
        for (auto predecessor : tmp->predecessors)
            collectLoop(header, predecessor, loop, false);
    }
}

void identifyLoops() {

    for (size_t i=0; i<BasicBlock::size(); ++i) {

        BasicBlock* cur = BasicBlock::get(i);
        if (cur->target && cur->id > cur->target->id) {


           Loop* loop = new Loop();
           loop->head = cur->target->id;
           loop->tail = cur->id;

           for (auto predecessor : BasicBlock::get(loop->tail)->predecessors)
               collectLoop(loop->head, predecessor, *loop, true);

           std::set<id_t> visited;
           visited.insert (loop->head);
           visited.insert (loop->tail);
           for (id_t node : loop->nodes)
               visited.insert (node);

           for (id_t node : visited) {
               BasicBlock* target = BasicBlock::get(node)->target;
               BasicBlock* successor = BasicBlock::get(node)->target;

               if (target && visited.find(target->id) == visited.end())
                   Loop::insLoopMap[target->entry_ins] = loop;
               if (successor && visited.find(successor->id) == visited.end())
                   Loop::insLoopMap[successor->entry_ins] = loop;
           }

           std::cout << "a new loop is born: " << std::endl
                     << "\t head: " << loop->head << std::endl
                     << "\t tail: " << loop->tail << std::endl
                     << "\t nodes: ";
           for (id_t node : loop->nodes)
               std::cout << node << " ";
           std::cout << std::endl;

            Loop::insLoopMap[BasicBlock::get(loop->head)->entry_ins] = loop;
        }
    }
}

std::string getNodeStr(BasicBlock* node) {
    std::stringstream ss;
    ss << "\t\"" << node->id << ": " << dissmap[node->entry_ins] << "\"";
    return ss.str();
}

void printCFG( BasicBlock* node,
               std::ofstream& ofs,
               int level) {

    static std::set<id_t> dumpedNodes;

    if (dumpedNodes.find(node->id) != dumpedNodes.end())
        return;
    else
        dumpedNodes.insert(node->id);

    if (level == 0) {
        ofs << "digraph CFG {" << std::endl;
        ofs << "\tgraph [fontname=\"fixed\"];" << std::endl;
        ofs << "\tnode [fontname=\"fixed\"];" << std::endl;
        ofs << "\tedge [fontname=\"fixed\"];" << std::endl;
    }
    ofs << getNodeStr(node) << ";" << std::endl;

    if (node->successor) {
        printCFG(node->successor, ofs, level+1);
        ofs << getNodeStr(node) << "->" << getNodeStr(node->successor)
            << ";" << std::endl;
    }

    if (node->target) {
        printCFG(node->target, ofs, level+1);
        ofs << getNodeStr(node) << "->" << getNodeStr(node->target)
            << " [style=dotted];" << std::endl;
    }

    if (level == 0)
        ofs << "}";
}

void printDOM( BasicBlock* node,
               std::ofstream& ofs,
               int level) {

    static std::set<id_t> dumpedNodes;

    if (dumpedNodes.find(node->id) != dumpedNodes.end())
        return;
    else
        dumpedNodes.insert(node->id);

    if (level == 0) {
        ofs << "digraph DOM {" << std::endl;
        ofs << "\tgraph [fontname=\"fixed\"];" << std::endl;
        ofs << "\tnode [fontname=\"fixed\"];" << std::endl;
        ofs << "\tedge [fontname=\"fixed\"];" << std::endl;
    }
    ofs << getNodeStr(node) << ";" << std::endl;

    if (node->iDom) {
        ofs << "\t" << getNodeStr(node->iDom) << "->"
                    << getNodeStr(node) << ";" << std::endl;
    }

    if (node->successor)
        printDOM(node->successor, ofs, level+1);

    if (node->target)
        printDOM(node->target, ofs, level+1);

    if (level == 0)
        ofs << "}";
}

static std::map<id_t, size_t> executionCount;
static std::map<id_t, size_t> iterationCount;

VOID callOnLoopEntry(id_t id) {
    iterationCount[id]++;
}

VOID callOnLoopExit(id_t id) {
    executionCount[id]++;
}

void instrumentLoops( RTN rtn ) {

    RTN_Open(rtn);

    for (INS in = RTN_InsHead(rtn); INS_Valid(in); in = INS_Next(in)) {

        ADDRINT addr = INS_Address(in);
        if (Loop::insLoopMap.find(addr) != Loop::insLoopMap.end()) {

            Loop* loop = Loop::insLoopMap[addr];

            // check entry
            BasicBlock* head = BasicBlock::get(loop->head);
            if (head->entry_ins == addr) {
                INS_InsertCall	( in,
                                IPOINT_BEFORE,
                                (AFUNPTR)callOnLoopEntry,
                                IARG_UINT32, loop->head,
                                IARG_END );
               executionCount[loop->head] = 0;
               iterationCount[loop->head] = 0;
            }
            // check exits
             else {
                for (id_t exit : loop->exits) {
                    BasicBlock *tmp = BasicBlock::get(exit);
                    if (tmp->entry_ins == addr)
                        INS_InsertCall ( in,
                                         IPOINT_BEFORE,
                                         (AFUNPTR)callOnLoopExit,
                                         IARG_UINT32, loop->head,
                                         IARG_END );

                }
            }
        }
    }

    RTN_Close(rtn);
}

VOID ImgLoad(IMG img, VOID *v) {

    // only inspect main executable
    if (!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {

            const string& name = RTN_Name(rtn);
            std::unique_ptr<leaders_t> leaders {new leaders_t};
            std::unique_ptr<jumps_t> jumps {new jumps_t};
            BasicBlock::reset();

            if (name == "main")
              std::cout << "main" << std::endl;

            getBBLFrontiers( rtn, *leaders.get(), *jumps.get() );
            BasicBlock* root = getStaticBBLs( rtn, *leaders.get(), *jumps.get() );
            getDominators(root);

            if (name == "main") {
              identifyLoops();
              instrumentLoops(rtn);
              std::cout << "end" << std::endl;
              std::ofstream ofs;
              std::ofstream ofsdom;
              ofs.open ("main.dot", std::ofstream::out);
              ofsdom.open ("dommain.dot", std::ofstream::out);
              printCFG(root, ofs, 0);
              printDOM(root, ofsdom, 0);
              ofs.close();
              ofsdom.close();
            }
        }
    }
}

VOID Fini(INT32 code, VOID *v) {

    std::cout << "finished\n";

    for (auto exec : executionCount)
        std::cout << "executions of loop " << exec.first << ": " << exec.second << std::endl;
    for (auto exec : iterationCount)
        std::cout << "iterations of loop " << exec.first << ": " << exec.second << std::endl;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(int argc, char * argv[]) {


    // prepare for image instrumentation mode
    PIN_InitSymbolsAlt(IFUNC_SYMBOLS);

    // Initialize pin
    if (PIN_Init(argc, argv))
        return -1;

    // Register IMGLoad to be called when a image is loaded
    IMG_AddInstrumentFunction(ImgLoad, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
