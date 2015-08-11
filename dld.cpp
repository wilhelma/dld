#include <stdio.h>
#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <memory>
#include <algorithm>
#include <bitset>
#include <sstream>
#include <string>
#include <boost/dynamic_bitset.hpp>

#include "pin.H"

std::ofstream ofs;
std::ofstream ofsdom;

typedef unsigned int id_t;
typedef std::set<ADDRINT> leaders_t;
typedef std::map<ADDRINT, ADDRINT> jumps_t;

typedef struct bbl_stub_t {
    leaders_t* leaders;
    jumps_t* jumps;
    bbl_stub_t( leaders_t* leaders,
                jumps_t* jumps )
        : leaders(leaders), jumps(jumps) {}
} bbl_stub_t;

class bbl_t {
public:
    typedef std::vector<bbl_t*> bblVec_t;

    id_t id;
    ADDRINT entry_ins;
    ADDRINT exit_ins;
    std::set<id_t> predecessors;
    std::set<id_t> dominators;
    bbl_t* successor;
    bbl_t* target;

    bbl_t( ADDRINT entry_ins,
           bbl_t* predecessor )
        : id(gID++), entry_ins(entry_ins), successor(nullptr), target(nullptr) {
        if (predecessor)
            predecessors.insert( predecessor->id );
        gBasicBlocks.push_back(this);
    }

    static void reset() {
        bbl_t::gID = 0;
        bbl_t::gBasicBlocks.clear();
    }

    static bbl_t* get(id_t id) {
        return gBasicBlocks[id];
    }

    static size_t size() {
        return gBasicBlocks.size();
    }

private:
    static id_t gID;
    static bblVec_t gBasicBlocks;
};

id_t bbl_t::gID = 0;
bbl_t::bblVec_t bbl_t::gBasicBlocks;

void firstPhase( RTN rtn,
                 bbl_stub_t& bs ) {

    bool isFirst = true;

    // Prepare for processing of RTN, an  RTN is not broken up into
    // BBLs, it is merely a sequence of INSs
    RTN_Open(rtn);

    for (INS in = RTN_InsHead(rtn); INS_Valid(in); in = INS_Next(in))
    {
        if (isFirst) {
            bs.leaders->insert( INS_Address(in) );
            isFirst = false;
        }

        if (INS_IsDirectBranchOrCall(in)) {
            ADDRINT target = INS_DirectBranchOrCallTargetAddress(in);
            bs.leaders->insert( target );
            (*bs.jumps)[INS_Address(in)] = target;
            isFirst = true;
        }

    }

    // to preserve space, release data associated with RTN after we
    // have processed it
    RTN_Close(rtn);
}

bbl_t* secondPhase( RTN rtn,
                    const bbl_stub_t& bs ) {

    std::map<ADDRINT, bbl_t*> leaderBBLMap, exitBBLMap;
    bbl_t *root = nullptr, *curBBL = nullptr;

    RTN_Open(rtn);

    // head instruction
    INS in = RTN_InsHead(rtn);
    if (INS_Valid(in)) {
        ADDRINT insAddr = INS_Address(in);
        root = curBBL = new bbl_t( insAddr, nullptr );
        leaderBBLMap[insAddr] = root;
        root->exit_ins = insAddr;
        in = INS_Next(in);
    }
    
    // other instructions
    while (INS_Valid(in)) {
        ADDRINT insAddr = INS_Address(in);
        if (bs.leaders->find( insAddr ) != bs.leaders->end()) {
            bbl_t **successor = &curBBL->successor;
            bbl_t *predecessor = curBBL;
            exitBBLMap[curBBL->exit_ins] = curBBL;        
            curBBL = new bbl_t( insAddr, curBBL );
            curBBL->predecessors.insert(predecessor->id);
            leaderBBLMap[insAddr] = curBBL;
            *successor = curBBL;
        }
        curBBL->exit_ins = insAddr;
        in = INS_Next(in);
    }

    RTN_Close(rtn);

    for ( auto edge : *bs.jumps ) {

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

void getDominators(bbl_t* root,
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
            for (id_t pred : bbl_t::get(i)->predecessors)
                *domBitSets[i].get() &= *domBitSets[pred].get();
            if (tmp != *domBitSets[i].get())
                changed = true;
        }
    }

    // fill dominators in bbl's
    for (size_t i=0; i<nBBL; ++i)
        for (size_t j=0; j<nBBL; ++j)
            if (domBitSets[i]->test(j))
                bbl_t::get(i)->dominators.insert(j);
}

std::string getNodeStr(bbl_t* node) {
    std::stringstream ss;
    ss << "\t\"" << node->id << ": " << node->entry_ins << "\"";
    return ss.str();
}

void printCFG(bbl_t* node, int level) {

    if (level == 0) {
        ofs << "digraph CFG {" << std::endl;
        ofs << "\tgraph [fontname=\"fixed\"];" << std::endl;
        ofs << "\tnode [fontname=\"fixed\"];" << std::endl;
        ofs << "\tedge [fontname=\"fixed\"];" << std::endl;
    }
    ofs << getNodeStr(node) << ";" << std::endl;

    if (node->successor) {
        printCFG(node->successor, level+1);
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

void printDOM(bbl_t* node, int level) {

    if (level == 0) {
        ofsdom << "digraph DOM {" << std::endl;
        ofsdom << "\tgraph [fontname=\"fixed\"];" << std::endl;
        ofsdom << "\tnode [fontname=\"fixed\"];" << std::endl;
        ofsdom << "\tedge [fontname=\"fixed\"];" << std::endl;
    }
    ofsdom << getNodeStr(node) << ";" << std::endl;

    for (auto dom : node->dominators) {
        ofsdom << "\t" << getNodeStr(bbl_t::get(dom)) << "->"
                    << getNodeStr(node) << ";" << std::endl;
    }

    if (node->successor)
        printDOM(node->successor, level+1);

    if (level == 0)
        ofsdom << "}";
}

VOID ImgLoad(IMG img, VOID *v) {

    // only inspect main executable
    if (!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {

            const string& name = RTN_Name(rtn);
            std::unique_ptr<leaders_t> leaders {new leaders_t};
            std::unique_ptr<jumps_t> jumps {new jumps_t};
            bbl_t::reset();

            bbl_stub_t bbl_stub( leaders.get(), jumps.get() );
            firstPhase( rtn, bbl_stub );
            bbl_t* root = secondPhase( rtn, bbl_stub );
            getDominators(root, bbl_t::size());

            if (name == "main") {
                printCFG(root, 0);
                printDOM(root, 0);
            }
        }
    }
}

VOID Fini(INT32 code, VOID *v) {

    ofs.close();
    ofsdom.close();
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(int argc, char * argv[]) {

    ofs.open ("main.dot", std::ofstream::out);
    ofsdom.open ("dommain.dot", std::ofstream::out);

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
