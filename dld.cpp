#include <stdio.h>
#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <memory>
#include <algorithm>
#include <bitset>

#include "pin.H"

std::ofstream ofs;

typedef std::set<ADDRINT> leaders_t;
typedef std::map<ADDRINT, ADDRINT> jumps_t;
typedef std::map<ADDRINT, std::set<ADDRINT> > dominators_t;

typedef struct bbl_stub_t {
    leaders_t* leaders;
    jumps_t* jumps;
    bbl_stub_t( leaders_t* leaders,
                jumps_t* jumps )
        : leaders(leaders), jumps(jumps) {}
} bbl_stub_t;

typedef struct bbl_t {
    ADDRINT entry_ins;
    ADDRINT exit_ins;
    std::set<struct bbl_t*> predecessors;
    struct bbl_t* successor;
    struct bbl_t* target;

    bbl_t( ADDRINT entry_ins,
           struct bbl_t* predecessor )
        : entry_ins(entry_ins), successor(nullptr), target(nullptr) {
        predecessors.insert( predecessor );
    }
} bbl_t;

unsigned int firstPhase( RTN rtn,
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

    return bs.leaders->size();
}

bbl_t* secondPhase( RTN rtn,
                    const bbl_stub_t& bs,
                    unsigned int nBBL ) {

    std::map<ADDRINT, bbl_t*> leaderBBLMap, exitBBLMap;
    std::bitset<nBBL>   <
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
            bbl_t** successor = &curBBL->successor;
            bbl_t* predecessor = curBBL;
            exitBBLMap[curBBL->exit_ins] = curBBL;        
            curBBL = new bbl_t( insAddr, curBBL );
            curBBL->predecessors.insert(predecessor);
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

                source->second->predecessors.insert(target->second);
                target->second->target = source->second;
            }
        }
    }

    return root;
}

void getDominators(bbl_t* root,
                   const bbl_stub_t& bs,
                   dominators_t& dom) {

    bbl_t* node = nullptr;

    for (node = root; node != nullptr; node = node->successor) {

        std::set<ADDRINT> &dominators = dom[node->entry_ins];
        std::set<ADDRINT> intersection = leaders;

        for (auto pred : node->predecessors) {
            if (pred) {

                if ( dominators.find(pred->entry_ins) != dominators.end() ) {
                    std::set<ADDRINT> result;


                    std::set_intersection(intersection.begin(), intersection.end(),
                                          dominators[pred->entry_ins].begin(), dominators)

                }

                std::set_intersection()


            }
        }

    }

}

void printBBL(bbl_t* node, int level) {

    ofs << level << " node: " << node->entry_ins << std::endl;
    ofs << level << "    pred: ";
    for (auto pred : node->predecessors) {
        if (pred)
            ofs << pred->entry_ins << ", ";
    }
    ofs << std::endl;
    if (node->successor != nullptr) {
        ofs << level << "     succ: " << std::endl;
        printBBL(node->successor, level+1);
    }
    delete node;
}

VOID ImgLoad(IMG img, VOID *v) {

    // only inspect main executable
    if (!IMG_IsMainExecutable(img)) return;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {

            const string& name = RTN_Name(rtn);
            std::unique_ptr<leaders_t> leaders {new leaders_t};
            std::unique_ptr<jumps_t> jumps {new jumps_t};
            dominators_t dominators;

            bbl_stub_t bbl_stub( leaders.get(), jumps.get() );

            unsigned int nBBL = firstPhase( rtn, bbl_stub );
            bbl_t* root = secondPhase( rtn, bbl_stub, nBBL );

            ofs << "Routine: " << name << std::endl;
            getDominators(root, dominators);

            //printBBL(root, 0);
        }
    }
}

VOID Fini(INT32 code, VOID *v) {

    ofs.close();
}


/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
int main(int argc, char * argv[]) {

    ofs.open ("test.out", std::ofstream::out);

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
