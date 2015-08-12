#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <boost/dynamic_bitset.hpp>

#include "pin.H"

// types
typedef std::set<ADDRINT> leaders_t;
typedef std::map<ADDRINT, ADDRINT> jumps_t;

/*
 * class BasicBlock
 */
class BasicBlock {
public:

    // types
    typedef unsigned int id_t;
    typedef std::vector<BasicBlock*> bbls_t;

    // attributes
    id_t id;
    ADDRINT entry_ins;
    ADDRINT exit_ins;
    std::set<id_t> predecessors;
    std::set<id_t> dominators;
    BasicBlock* successor;
    BasicBlock* target;

    //--------------------------------------------------------------------------
    BasicBlock( ADDRINT entry_ins,
                BasicBlock* predecessor )
            : id(gID++), entry_ins(entry_ins),
              successor(nullptr), target(nullptr) {

        if (predecessor)
            predecessors.insert( predecessor->id );
        gBasicBlocks.push_back(this);
    }

    static void reset() {
        BasicBlock::gID = 0;
        BasicBlock::gBasicBlocks.clear();
    }

    static BasicBlock* get(id_t id) {
        return gBasicBlocks[id];
    }

    static size_t size() {
        return gBasicBlocks.size();
    }

private:
    static BasicBlock::id_t gID;
    static bbls_t gBasicBlocks;
};
BasicBlock::id_t BasicBlock::gID = 0;
BasicBlock::bbls_t BasicBlock::gBasicBlocks;

/*******************************************************************************
 * global functions
 ******************************************************************************/

/*
 * firstphase
 */
void firstPhase( RTN rtn,
                 leaders_t& leaders,
                 jumps_t& jumps ) {

    bool isFirst = true;

    RTN_Open(rtn);
    for (INS in = RTN_InsHead(rtn); INS_Valid(in); in = INS_Next(in))
    {
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

BasicBlock* secondPhase( RTN rtn,
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
        leaderBBLMap[insAddr] = root;
        root->exit_ins = insAddr;
        in = INS_Next(in);
    }
    
    // other instructions
    while (INS_Valid(in)) {
        ADDRINT insAddr = INS_Address(in);
        if (leaders.find( insAddr ) != leaders.end()) {
            BasicBlock **successor = &curBBL->successor;
            BasicBlock *predecessor = curBBL;
            exitBBLMap[curBBL->exit_ins] = curBBL;        
            curBBL = new BasicBlock( insAddr, curBBL );
            curBBL->predecessors.insert(predecessor->id);
            leaderBBLMap[insAddr] = curBBL;
            *successor = curBBL;
        }
        curBBL->exit_ins = insAddr;
        in = INS_Next(in);
    }

    RTN_Close(rtn);

    for ( auto edge : jumps ) {

        auto source = exitBBLMap.find( edge.first );
        auto target = leaderBBLMap.find( edge.second );

        if (source != exitBBLMap.end()) {
            if (target != leaderBBLMap.end()) {

                source->second->predecessors.insert(target->second->id);
                target->second->target = source->second;
            }
        }
    }

    return root;
}

void getDominators(BasicBlock* root,
                   const size_t nBBL) {

    bool changed = true;
    std::unique_ptr< boost::dynamic_bitset<> >* domBitSets =
            new std::unique_ptr< boost::dynamic_bitset<> >[nBBL];
    for (size_t i=0; i<nBBL; ++i)
        domBitSets[i] = std::unique_ptr< boost::dynamic_bitset<> >(new boost::dynamic_bitset<>(nBBL));

    // initialize dominator bitsets
    domBitSets[0]->reset();
    domBitSets[0]->set(0);
    for (size_t i=1; i<nBBL; ++i)
        domBitSets[i]->set();

    // compute dominators (fixpoint algorithm)
    while (changed) {
        changed = false;
        for (size_t i=1; i<nBBL; ++i) {
            boost::dynamic_bitset<> tmp(*domBitSets[i].get());
            for (BasicBlock::id_t pred : BasicBlock::get(i)->predecessors)
                *domBitSets[i].get() &= *domBitSets[pred].get();
            if (tmp != *domBitSets[i].get())
                changed = true;
        }
    }

    // fill dominators in bbl's
    for (size_t i=0; i<nBBL; ++i)
        for (size_t j=0; j<nBBL; ++j)
            if (domBitSets[i]->test(j))
                BasicBlock::get(i)->dominators.insert(j);
}

std::string getNodeStr(BasicBlock* node) {
    std::stringstream ss;
    ss << "\t\"" << node->id << ": " << node->entry_ins << "\"";
    return ss.str();
}

void printCFG( BasicBlock* node,
               std::ofstream& ofs,
               int level) {

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
        ofs << getNodeStr(node) << "->" << getNodeStr(node->target)
            << " [style=dotted];" << std::endl;
    }

    if (level == 0)
        ofs << "}";
}

void printDOM( BasicBlock* node,
               std::ofstream& ofs,
               int level) {

    if (level == 0) {
        ofs << "digraph DOM {" << std::endl;
        ofs << "\tgraph [fontname=\"fixed\"];" << std::endl;
        ofs << "\tnode [fontname=\"fixed\"];" << std::endl;
        ofs << "\tedge [fontname=\"fixed\"];" << std::endl;
    }
    ofs << getNodeStr(node) << ";" << std::endl;

    for (auto dom : node->dominators) {
        ofs << "\t" << getNodeStr(BasicBlock::get(dom)) << "->"
                    << getNodeStr(node) << ";" << std::endl;
    }

    if (node->successor)
        printDOM(node->successor, ofs, level+1);

    if (level == 0)
        ofs << "}";
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

            firstPhase( rtn, *leaders.get(), *jumps.get() );
            BasicBlock* root = secondPhase( rtn, *leaders.get(), *jumps.get() );
            getDominators(root, BasicBlock::size());

            if (name == "main") {
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
