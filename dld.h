#ifndef DLD_H
#define DLD_H

#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <map>
#include <boost/dynamic_bitset.hpp>

#include "pin.H"

// forward declaration
struct Loop;

// types
typedef std::set<ADDRINT> leaders_t;
typedef std::map<ADDRINT, ADDRINT> jumps_t;
typedef std::map<ADDRINT, std::string> dissmap_t;

/*
 * class BasicBlock
 */
class BasicBlock {
public:

  // types---------------------------------------------------------------------
  typedef size_t id_t;
  typedef std::vector<BasicBlock*> bbls_t;
  typedef std::map<ADDRINT, Loop*> loops_t;

  // attributes----------------------------------------------------------------
  id_t id;							// id of the basic block (starting from 0)
  ADDRINT entry_ins;				// address of the first instruction
  ADDRINT exit_ins;				// address of the last instruction
  std::set<id_t> predecessors;		// set of id's of all predecessors
  std::set<id_t> dominators;		// set of id's of all dominators
  BasicBlock* successor;			// pointer to the successor
  BasicBlock* target;				// pointer to the target (if existing)
  BasicBlock* iDom;				// immediate dominator

  // methods-------------------------------------------------------------------

  // the constructor
  explicit BasicBlock( ADDRINT entry_ins,
                       BasicBlock* predecessor );

  // static methods------------------------------------------------------------

  // resets the static information
  static void reset();

  // get pointer to the basic block with given id
  static BasicBlock* get(id_t id) { return gBasicBlocks[id]; }

  // get current number of inserted basic blocks
  static size_t size() { return gBasicBlocks.size(); }

  // get root block
  static BasicBlock* getRoot() { return root; }

  // dump control flow graph to the given stream
  static void printCFG( std::ofstream& ofs );

  // print dominator tree
  static void printDOM( std::ofstream& ofs );

  // identify loops by using the cfg and the dominators
  static void identifyLoops( RTN rtn,
                             const leaders_t& leaders,
                             const jumps_t& jumps,
                             loops_t& loops );

private:

   // print control flow graph of the given node and all successors/targets
   static void printCFG( BasicBlock* node,
                         std::ofstream& ofs,
                         int level );

   // print dominator tree of the given node and all successors/targets
   static void printDOM( BasicBlock* node,
                         std::ofstream& ofs,
                         int level );

   // get loops dynamically
   static void buildDomTree( RTN rtn,
                             const leaders_t& leaders,
                             const jumps_t & jumps );

   // collect all loops
   static void collectLoop( id_t header,
                            id_t current,
                            Loop& loop,
                            bool initial = true);

   // identify loops by using the cfg and the dominators
   static void identifyLoops( loops_t& loops );

  /* buildStaticBBLs()
  *
  * builds a cfg consisting of newly allocated bbl's and returning the root
  * of the cfg.
  * This is done by considering each leader instruction as the beginning of a
  * bbl, each transition as a successor relation, and each jump as a target
  * relation. The predecessor attribute is filled accordingly to the successor
  * and target relation. The dominator attribute is not filled here (see
  * getDominators()).
  */
  static void buildStaticBBLs( RTN rtn,
                               const leaders_t& leaders,
                               const jumps_t& jumps );

  /* getDominators()
   *
   * computes the dominators of all basic blocks in the cfg
   */
  static void getDominators( BasicBlock* root );

  static BasicBlock::id_t gID;
  static bbls_t gBasicBlocks;
  static BasicBlock* root;
};

/*
 * struct Loop
 */
struct Loop {
    BasicBlock::id_t head;
    BasicBlock::id_t tail;
    std::set<BasicBlock::id_t> nodes;
    std::set<BasicBlock::id_t> exits;
};

#endif // DLD_H
