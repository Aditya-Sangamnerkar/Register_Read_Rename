#include <inttypes.h>


////////////////////////////////////////////
// FREE LIST 
///////////////////////////////////////////
struct FreeList
{
	uint64_t head, tail;
	bool head_phase, tail_phase;
	uint64_t *FIFO;
	uint64_t  length;
};

//////////////////////////////////////////
// ACTIVE LIST
//////////////////////////////////////////

// Fields related to destination register
struct DestRegFields
{
	bool dest_valid;
	uint64_t log_reg;
	uint64_t phys_reg;
};

// Fields for signalling offending instruction
struct OffendingInstSignalFields
{
	bool exception_bit;
	bool load_violation_bit;
	bool branch_mispred_bit;
	bool value_mispred_bit;
};

// Fields indication special instruction type
struct SpecialInstSignalFields
{
	bool load;
	bool store;
	bool branch;
	bool amo;
	bool csr;
};

// active list entry
struct ActiveListEntry
{
	// field 1-3
	DestRegFields dest_reg;

	// field 4
	bool completed_bit;

	// fields 5-8
	OffendingInstSignalFields off_inst_signal;

	// fields 9-13
	SpecialInstSignalFields spec_inst_signal;

	// field 14
	uint64_t PC;
};

// active list
struct ActiveList
{
	uint64_t head, tail;
	bool head_phase, tail_phase;
	ActiveListEntry *FIFO;
	uint64_t length;

};

//////////////////////////////////////////
// Branch Checkpoint
//////////////////////////////////////////
struct BranchCheckpointEntry
{
	// Shadow map table 
	uint64_t *cp_RMT;
	// Checkpointed Free List
	uint64_t cp_free_list_head;
	bool cp_free_list_head_phase;
	// Checkpointed GBM
	uint64_t cp_GBM;
};



class renamer {
private:
	/////////////////////////////////////////////////////////////////////
	// Put private class variables here.
	/////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Structure 1: Rename Map Table
	/////////////////////////////////////////////////////////////////////
	uint64_t *RMT;

	/////////////////////////////////////////////////////////////////////
	// Structure 2: Architectural Map Table
	/////////////////////////////////////////////////////////////////////
	uint64_t *AMT;

	/////////////////////////////////////////////////////////////////////
	// Structure 3: Free List
	/////////////////////////////////////////////////////////////////////
	FreeList FL;

	/////////////////////////////////////////////////////////////////////
	// Structure 4: Active List
	/////////////////////////////////////////////////////////////////////
	ActiveList AL;

	/////////////////////////////////////////////////////////////////////
	// Structure 5: Physical Register File
	/////////////////////////////////////////////////////////////////////
	uint64_t *PRF;

	/////////////////////////////////////////////////////////////////////
	// Structure 6: Physical Register File Ready Bit Array
	/////////////////////////////////////////////////////////////////////
	bool *PRF_ready_array;

	/////////////////////////////////////////////////////////////////////
	// Structure 7: Global Branch Mask (GBM)
	/////////////////////////////////////////////////////////////////////
	uint64_t GBM;

	/////////////////////////////////////////////////////////////////////
	// Structure 8: Branch Checkpoints
	/////////////////////////////////////////////////////////////////////
	BranchCheckpointEntry *branch_checkpoint;


	//////////////////////////////
	// sizes of various structures
	//////////////////////////////
	uint64_t n_log_regs;
	uint64_t n_phys_regs;
	uint64_t n_branches;
	uint64_t n_active;

	/////////////////////////////////////////////////////////////////////
	// Private functions.
	/////////////////////////////////////////////////////////////////////

	// helper methods for constructor execution
	void __space_allocation__();
	void __init_FL_AMT_RMT__();
	void __init_AL__();
	void __init_PRF_ready_array__();
	void __init_GBM__();

	// helper methods for Free list
	bool __FL_empty__();
	bool __FL_full__();
	uint64_t __FL_pop__();

	void __FL_push__(uint64_t phys_reg);
	

	// helper methods for Acive list
	bool __AL_full__();
	bool __AL_empty__();
	uint64_t __AL_push__(bool dest_valid,
	                       uint64_t log_reg,
	                       uint64_t phys_reg,
	                       bool load,
	                       bool store,
	                       bool branch,
	                       bool amo,
	                       bool csr,
	                       uint64_t PC);

	void __AL_restore__(uint64_t resolved_branch_AL_index);

	ActiveListEntry __AL_pop__();

	// helper methods for GBM and checkpoint

	uint64_t __GBM_free_bit__();
	void __GBM_set_bit__(uint64_t branch_id);
	void __branch_checkpoint__(uint64_t branch_id);

	void __GBM_clear_bit__(uint64_t branch_id);
	void __branch_checkpoint_cp_GBM_clear_bit__(uint64_t branch_id);

	void __branch_checkpoint_GBM_RMT_FL_restore__(uint64_t branch_id);


	


public:
	////////////////////////////////////////
	// Public functions.
	////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// This is the constructor function.
	/////////////////////////////////////////////////////////////////////
	renamer(uint64_t n_log_regs,
		uint64_t n_phys_regs,
		uint64_t n_branches,
		uint64_t n_active);

	/////////////////////////////////////////////////////////////////////
	// This is the destructor, used to clean up memory space and
	// other things when simulation is done.
	/////////////////////////////////////////////////////////////////////
	~renamer();


	//////////////////////////////////////////
	// Functions related to Rename Stage.   //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// The Rename Stage must stall if there aren't enough free physical
	// registers available for renaming all logical destination registers
	// in the current rename bundle.
	/////////////////////////////////////////////////////////////////////
	bool stall_reg(uint64_t bundle_dst);

	/////////////////////////////////////////////////////////////////////
	// The Rename Stage must stall if there aren't enough free
	// checkpoints for all branches in the current rename bundle.
	/////////////////////////////////////////////////////////////////////
	bool stall_branch(uint64_t bundle_branch);

	/////////////////////////////////////////////////////////////////////
	// This function is used to get the branch mask for an instruction.
	/////////////////////////////////////////////////////////////////////
	uint64_t get_branch_mask();

	/////////////////////////////////////////////////////////////////////
	// This function is used to rename a single source register.
	/////////////////////////////////////////////////////////////////////
	uint64_t rename_rsrc(uint64_t log_reg);

	/////////////////////////////////////////////////////////////////////
	// This function is used to rename a single destination register.
	/////////////////////////////////////////////////////////////////////
	uint64_t rename_rdst(uint64_t log_reg);

	/////////////////////////////////////////////////////////////////////
	// This function creates a new branch checkpoint.
	/////////////////////////////////////////////////////////////////////
	uint64_t checkpoint();

	//////////////////////////////////////////
	// Functions related to Dispatch Stage. //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// The Dispatch Stage must stall if there are not enough free
	// entries in the Active List for all instructions in the current
	// dispatch bundle.
	/////////////////////////////////////////////////////////////////////
	bool stall_dispatch(uint64_t bundle_inst);

	/////////////////////////////////////////////////////////////////////
	// This function dispatches a single instruction into the Active
	// List.
	/////////////////////////////////////////////////////////////////////
	uint64_t dispatch_inst(bool dest_valid,
	                       uint64_t log_reg,
	                       uint64_t phys_reg,
	                       bool load,
	                       bool store,
	                       bool branch,
	                       bool amo,
	                       bool csr,
	                       uint64_t PC);

	/////////////////////////////////////////////////////////////////////
	// Test the ready bit of the indicated physical register.
	// Returns 'true' if ready.
	/////////////////////////////////////////////////////////////////////
	bool is_ready(uint64_t phys_reg);

	/////////////////////////////////////////////////////////////////////
	// Clear the ready bit of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void clear_ready(uint64_t phys_reg);


	//////////////////////////////////////////
	// Functions related to the Reg. Read   //
	// and Execute Stages.                  //
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Return the contents (value) of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	uint64_t read(uint64_t phys_reg);

	/////////////////////////////////////////////////////////////////////
	// Set the ready bit of the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void set_ready(uint64_t phys_reg);


	//////////////////////////////////////////
	// Functions related to Writeback Stage.//
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Write a value into the indicated physical register.
	/////////////////////////////////////////////////////////////////////
	void write(uint64_t phys_reg, uint64_t value);

	/////////////////////////////////////////////////////////////////////
	// Set the completed bit of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	void set_complete(uint64_t AL_index);

	/////////////////////////////////////////////////////////////////////
	// This function is for handling branch resolution.
	/////////////////////////////////////////////////////////////////////
	void resolve(uint64_t AL_index,
		     uint64_t branch_ID,
		     bool correct);

	//////////////////////////////////////////
	// Functions related to Retire Stage.   //
	//////////////////////////////////////////

	///////////////////////////////////////////////////////////////////
	// This function allows the caller to examine the instruction at the head
	// of the Active List.
	/////////////////////////////////////////////////////////////////////
	bool precommit(bool &completed,
                       bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
	               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
		       uint64_t &PC);

	/////////////////////////////////////////////////////////////////////
	// This function commits the instruction at the head of the Active List.
	/////////////////////////////////////////////////////////////////////
	void commit();

	//////////////////////////////////////////////////////////////////////
	// Squash the renamer class.
	//
	// Squash all instructions in the Active List and think about which
	// sructures in your renamer class need to be restored, and how.
	/////////////////////////////////////////////////////////////////////
	void squash();

	//////////////////////////////////////////
	// Functions not tied to specific stage.//
	//////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////
	// Functions for individually setting the exception bit,
	// load violation bit, branch misprediction bit, and
	// value misprediction bit, of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	void set_exception(uint64_t AL_index);
	void set_load_violation(uint64_t AL_index);
	void set_branch_misprediction(uint64_t AL_index);
	void set_value_misprediction(uint64_t AL_index);

	/////////////////////////////////////////////////////////////////////
	// Query the exception bit of the indicated entry in the Active List.
	/////////////////////////////////////////////////////////////////////
	bool get_exception(uint64_t AL_index);
};
