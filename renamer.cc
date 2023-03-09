#include "renamer.h"
#include<cassert>
#include<iostream>


////////////////////////////////////
// Constructor                    //
///////////////////////////////////
void renamer :: __space_allocation__()
{	
	//std::cout << "\n __space_allocation__";
	// RMT and AMT
	this->RMT = new uint64_t[this->n_log_regs];
    this->AMT = new uint64_t[this->n_log_regs];

    // free list
    this->FL.length = this->n_phys_regs - this->n_log_regs;
    this->FL.FIFO = new uint64_t[this->FL.length];

    // active list
    this->AL.length = this->n_active;
    this->AL.FIFO = new ActiveListEntry[this->AL.length];

    // PRF
    this->PRF = new uint64_t[this->n_phys_regs];

    // PRF ready bit array
    this->PRF_ready_array = new bool[this->n_phys_regs];


    // Branch Checkpoints - checkpointed RMT 
    this->branch_checkpoint = new BranchCheckpointEntry[this->n_branches];
    for(uint64_t i=0; i<n_branches; i++){
    	this->branch_checkpoint[i].cp_RMT = new uint64_t[this->n_log_regs];
    }

}

void renamer :: __init_FL_AMT_RMT__()
{
	//std::cout << "\n__init_FL_AMT_RMT__"; 
	// Initialize the AMT with committed state PRF mappings.
	// prf tags = 0 : n_log_regs  - 1
	for(uint64_t i=0; i<this->n_log_regs; i++){
		this->AMT[i] = i;
	}

	// Copy the committed state of AMT to the RMT
	for(uint64_t i=0; i<this->n_log_regs; i++){
		this->RMT[i] = this->AMT[i];
	}


	// Push PRF tags on Free list FIFO 
    // prf tags = n_log_regs : n_log_regs + FL.length - 1 
	for(uint64_t i=0; i<this->FL.length; i++)
		this->FL.FIFO[i] = this->n_log_regs + i;

	// Initialize the Free list parameters to make free list fifo full
	// This implies that the Free list is empty
	this->FL.head = 0;
	this->FL.tail = 0;
	this->FL.head_phase =0;
	this->FL.tail_phase = !this->FL.head_phase;

	
}

void renamer :: __init_AL__()
{
	//std::cout <<"\n __init_AL__";
	// Initialize the AL parameters to make AL empty
	this->AL.head = 0;
	this->AL.tail = 0;
	this->AL.head_phase = 0;
	this->AL.tail_phase = this->AL.head_phase;
}

void renamer :: __init_PRF_ready_array__()
{	
	//std::cout << "\n __init_PRF_ready_array__";
	// initialize the prfs as ready in committed state
	// leave the others as dont cares
	// initially ready prf tags = 0 : n_log_regs - 1
	for(int i=0; i<this->n_log_regs; i++)
		this->PRF_ready_array[i] = true;
}

void renamer :: __init_GBM__()
{	
	//std::cout << "\n__init_GBM__";
	// initialize gbm as empty (no unresolved branches) in committed state
	this->GBM = 0;
}


renamer :: renamer(uint64_t n_log_regs,
				   uint64_t n_phys_regs,
				   uint64_t n_branches,
				   uint64_t n_active)
{	
	//std::cout << "\n renamer";
	// Assert the number of physical registers > number logical registers.
	assert(n_phys_regs > n_log_regs);

	// Assert 1 <= n_branches <= 64.
	assert(n_branches >=1 && n_branches <= 64);

	// Assert n_active > 0.
	assert(n_active > 0);

	// sizes for various structures
	this->n_log_regs = n_log_regs; // AMT, RMT
	this->n_phys_regs = n_phys_regs; // PRF, FL
	this->n_branches = n_branches; // GBM, branch_checkpoint
	this->n_active = n_active; // AL

	// allocate space for the primary data structures.
	this->__space_allocation__();

	/* 
	   initialize the data structures based on the knowledge
	   that the pipeline is intially empty (no in-flight instructions yet). 
	*/

	// AMT, RMT and Free list
	this->__init_FL_AMT_RMT__();
	

	// Active list
	this->__init_AL__();

	// PRF

	// PRF ready bit array
	this->__init_PRF_ready_array__();
	
	// GBM 
	this->__init_GBM__();

	// branch checkpoints

}

////////////////////////////////////
// Destructor                     //
///////////////////////////////////
renamer :: ~renamer()
{

}
// ////////////////////////////////////////
// Functions related to Free List        //
// ////////////////////////////////////////

bool renamer :: __FL_full__()
{
	/*
		 - FL is full -> all prfs between head and tail available for renaming
	*/	
	bool fifo_full = ((this->FL.head == this->FL.tail) && (this->FL.head_phase != this->FL.tail_phase));
	//std::cout << "\n __FL_full__";
	return fifo_full;
}

bool renamer :: __FL_empty__()
{
	/*
		/*
		- FL is empty -> no prfs available for renamings
	*/ 
	bool fifo_empty = ((this->FL.head == this->FL.tail) && (this->FL.head_phase == this->FL.tail_phase));
	//std::cout << "\n__FL_empty__";
	return fifo_empty;
}

uint64_t renamer :: __FL_pop__()
{
	//std::cout << "\n __FL_pop__";
	// Assert free list is not empty while popping out mappings.
	assert(!this->__FL_empty__());	

	// buffer data at head
	uint64_t phys_reg = this->FL.FIFO[this->FL.head];

	// increment head
	this->FL.head++;
	// wrap around check
	if(this->FL.head == this->FL.length)
	{
		this->FL.head = 0;
		this->FL.head_phase = !this->FL.head_phase;
	}
	
	return phys_reg;
}

void renamer :: __FL_push__(uint64_t phys_reg)
{
	//std::cout << "\n __FL_push__ " << " FL Full:" << this->__FL_full__()<< " FL head :" << this->FL.head << "FL h_p:" << this->FL.head_phase;
	//std::cout << " FL tail :" << this->FL.tail << " FL t_p:" << this->FL.tail_phase;
	// Assert that FL is not full
	assert(!this->__FL_full__());

	// insert phys_reg at the tail of FL
	this->FL.FIFO[this->FL.tail] = phys_reg;

	// increment tail
	this->FL.tail++;

	// wrap around check
	if(this->FL.tail == this->FL.length)
	{
		this->FL.tail = 0;
		this->FL.tail_phase = !this->FL.tail_phase;
	}
	


}

// ////////////////////////////////////////
// Functions related to Active List      //
// ////////////////////////////////////////

bool renamer :: __AL_empty__()
{
	/*
		AL is empty -> AL has no inflight instructions 
	*/
	bool fifo_empty = ((this->AL.head == this->AL.tail) && (this->AL.head_phase == this->AL.tail_phase));
	//std::cout << "\n __AL_empty__";
	return fifo_empty;
}

bool renamer :: __AL_full__()
{
	/*
		AL is full -> no entry for another inflight instruction between head and tail
	*/

	bool fifo_full = ((this->AL.head == this->AL.tail) && (this->AL.head_phase != this->AL.tail_phase));
	//std::cout << "\n __AL_full__";
	return fifo_full;
}

uint64_t renamer :: __AL_push__(bool dest_valid,
	                       uint64_t log_reg,
	                       uint64_t phys_reg,
	                       bool load,
	                       bool store,
	                       bool branch,
	                       bool amo,
	                       bool csr,
	                       uint64_t PC)
{
	//std::cout << "\n __AL_push__";
	// Assert that AL is  not full
	assert(!this->__AL_full__());
	// Assert that AL has space for one instruction
	assert(!this->stall_dispatch(1));

	// buffer the tail of AL
	uint64_t inst_AL_index = this->AL.tail;

	// insert instruction at tail of AL
	
	// insert instruction PC
	this->AL.FIFO[this->AL.tail].PC = PC;
	// insert destination register details
	this->AL.FIFO[this->AL.tail].dest_reg.dest_valid = dest_valid;
	if(dest_valid){
		this->AL.FIFO[this->AL.tail].dest_reg.log_reg = log_reg;
		this->AL.FIFO[this->AL.tail].dest_reg.phys_reg = phys_reg; 
	}
	// insert special instruction signal fields
	this->AL.FIFO[this->AL.tail].spec_inst_signal.load = load;
	this->AL.FIFO[this->AL.tail].spec_inst_signal.store = store;
	this->AL.FIFO[this->AL.tail].spec_inst_signal.branch = branch;
	this->AL.FIFO[this->AL.tail].spec_inst_signal.amo = amo;
	this->AL.FIFO[this->AL.tail].spec_inst_signal.csr = csr;

	// make the completed bit as zero
	this->AL.FIFO[this->AL.tail].completed_bit = false;

	// reset the offending inst signals
	this->AL.FIFO[this->AL.tail].off_inst_signal.exception_bit = false;
	this->AL.FIFO[this->AL.tail].off_inst_signal.load_violation_bit = false;
	this->AL.FIFO[this->AL.tail].off_inst_signal.branch_mispred_bit = false;
	this->AL.FIFO[this->AL.tail].off_inst_signal.value_mispred_bit = false;


	// increment tail
	this->AL.tail++;

	// wrap around check
	if(this->AL.tail == this->AL.length)
	{
		this->AL.tail = 0;
		this->AL.tail_phase = !this->AL.tail_phase;
	}
	

	return inst_AL_index;

}

void renamer :: __AL_restore__(uint64_t resolved_branch_AL_index)
{
	//std::cout << "\n __AL_restore__";
	// AL tail restore
	this->AL.tail = resolved_branch_AL_index + 1;
	// wrap around check
	if(this->AL.tail == this->AL.length)
		this->AL.tail = 0;

	// tail phase restore
	/*
		after tail restore
		- same phase (previously)
			- h == new_t : not possible, AL cannot be empty
			- h < new_t : tail phase remains as is.
		- diff phse (previously)
			- h == new_t : AL full, no tail phase change
			- h > new_t : tail phase flip to head phase
	*/
	if(this->AL.head >= this->AL.tail)
		this->AL.tail_phase = !this->AL.head_phase;
	else
		this->AL.tail_phase = this->AL.head_phase;

	
}

ActiveListEntry renamer :: __AL_pop__()
{
	// Assert that the AL is not empty
	assert(!this->__AL_empty__());

	// buffer data at AL head
	ActiveListEntry inst = this->AL.FIFO[this->AL.head];

	// increment head
	this->AL.head++;
	// wrap around check
	if(this->AL.head == this->AL.length)
	{
		this->AL.head = 0;
		this->AL.head_phase = !this->AL.head_phase;
	}
	//std::cout << "\n __AL_pop__";

	return inst;
}



// //////////////////////////////////////////////////////
// Functions related to GBM and branch_checkpoint     //
// /////////////////////////////////////////////////////

uint64_t renamer :: __GBM_free_bit__()
{
	uint64_t gbm = this->GBM;
	// Assert if GBM has space for an additional unresolved branch
	assert(!this->stall_branch(1));	
	
	//std::cout << "\n __GBM_free_bit__";

	for(uint64_t i=0; i<this->n_branches; i++)
	{
		if((gbm & 1) == 0)
			return i;

		gbm = gbm >> 1;
		
	}
	// control cannot reach here due to the assertion.
	// return -1;
}

void renamer :: __GBM_set_bit__(uint64_t branch_id)
{	
	//std::cout << "\n __GBM_set_bit__";
	// set the bit at position "branch_id" in GBM
	uint64_t mask = 1ul << branch_id;
	this->GBM = this->GBM | mask;
}

void renamer :: __branch_checkpoint__(uint64_t branch_id)
{
	//std::cout << "\n __branch_checkpoint__" << " branch_id : " << branch_id;
	// checkpoint RMT
	for(uint64_t i=0; i<this->n_log_regs; i++){
		this->branch_checkpoint[branch_id].cp_RMT[i] = this->RMT[i];
	}

	// checkpoint GBM
	this->branch_checkpoint[branch_id].cp_GBM = this->GBM;

	// checkpoint Free List
	//std::cout << " FL head:" << this->FL.head << " FL h_p" << this->FL.head_phase;
	this->branch_checkpoint[branch_id].cp_free_list_head = this->FL.head;
	this->branch_checkpoint[branch_id].cp_free_list_head_phase = this->FL.head_phase;
	

}

void renamer :: __GBM_clear_bit__(uint64_t branch_id)
{
	// clear the bit at position branch id
	uint64_t mask = ~(1ul << branch_id);
	this->GBM = this->GBM & mask;	
	//std::cout << "\n __GBM_clear_bit__";
}

void renamer :: __branch_checkpoint_cp_GBM_clear_bit__(uint64_t branch_id)
{
	/* - iterate gbm 0 : n_braches - 1
	   - valid entry in gbm (set bit)
	   		- checkpoint[valid_entry_index].cp_GBM clear branch_id index bit
	*/
	uint64_t mask = ~(1ul << branch_id);

	for(uint64_t i=0; i<this->n_branches; i++)
	{
		
		this->branch_checkpoint[i].cp_GBM =  this->branch_checkpoint[i].cp_GBM & mask;
		
	}
	//std::cout << "\n__branch_checkpoint_cp_GBM_clear_bit__";


}

void renamer :: __branch_checkpoint_GBM_RMT_FL_restore__(uint64_t branch_id)
{
	// std::cout << "\n__branch_checkpoint_GBM_RMT_FL_restore__ "; 
	// std::cout << " FL head :" << this->FL.head << "FL h_p:" << this->FL.head_phase;
	// std::cout << " FL tail :" << this->FL.tail << " FL t_p:" << this->FL.tail_phase;
	// GBM restore
	this->GBM = this->branch_checkpoint[branch_id].cp_GBM;

	// RMT restore
	for(uint64_t i=0; i<this->n_log_regs; i++)
		this->RMT[i] =  this->branch_checkpoint[branch_id].cp_RMT[i];

	// FL restore
	this->FL.head = this->branch_checkpoint[branch_id].cp_free_list_head;
	this->FL.head_phase = this->branch_checkpoint[branch_id].cp_free_list_head_phase;
	//std::cout << " FL head :" << this->FL.head << "FL h_p:" << this->FL.head_phase;
	//std::cout << " FL tail :" << this->FL.tail << " FL t_p:" << this->FL.tail_phase;

	
	
}

// ////////////////////////////////////////
// Functions related to Rename Stage     //
// ////////////////////////////////////////

bool renamer :: stall_reg(uint64_t bundle_dst)
{
	/*The Rename Stage must stall if there aren't enough free physical
	  registers available for renaming all logical destination registers
	  in the current rename bundle.

	  - FL empty = FL FIFO empty = prf tags not available -> stalls
	  - FL full = FL FIFO full = prf tags  available -> no stalls
	  - # entries between FL head and tail = # free phys regs
	  - # free phys regs > bundle_dst for no stall
	*/
	
	// free_phys_regs_count = #occupied in FL
	uint64_t free_phys_regs_count = 0;

	// head < tail and empty
	if(this->FL.head_phase == this->FL.tail_phase)
		free_phys_regs_count = this->FL.tail - this->FL.head;

	// head > tail and full
	else
		free_phys_regs_count = this->FL.length - (this->FL.head - this->FL.tail);

	bool stall = (free_phys_regs_count < bundle_dst) ? true : false;
	// std::cout << "\n stall_reg :" << stall << " FL head :" << this->FL.head << "FL h_p:" << this->FL.head_phase;
	// std::cout << " FL tail :" << this->FL.tail << " FL t_p:" << this->FL.tail_phase;
	return stall;
}

bool renamer :: stall_branch(uint64_t bundle_branch)
{
	/*
		The Rename Stage must stall if there aren't enough free
		checkpoints for all branches in the current rename bundle.

		- # bits in GBM = # total check points = # supported unresolved branches
		- GBM ith bit  = 0 -> free checkpoint
		- GBM ith bit  = 1 -> occupied checkpoint
	*/
	// std::cout << "\nstall_branch ";
	uint64_t free_checkpoint_count = 0;
	uint64_t gbm = this->GBM;

	for(uint64_t i=0; i<this->n_branches; i++)
	{
		if((gbm & 1ul) ==  0)
			free_checkpoint_count++;
		gbm = gbm >> 1;
	}

	bool stall = (free_checkpoint_count < bundle_branch) ? true : false;
	
	return stall;
}

uint64_t renamer :: get_branch_mask()
{
	//std::cout<<"\nget_branch_mask";
	return this->GBM;
}

uint64_t renamer :: rename_rsrc(uint64_t log_reg)
{
	/*
	- This function is used to rename a single source register.
	- fetch the most recent mapping of the logical register 
	  in the PRF.
	  	- index the RMT via log_res
	*/
	uint64_t phys_reg = this->RMT[log_reg];
	//std::cout << "\n rename_rsrc";
	return phys_reg;
}

uint64_t renamer :: rename_rdst(uint64_t log_reg)
{
	/*
		- pop a free entry (prf mapping) from the head of the free list.
		- update the RMT for the latest mapping of the given (destination)
		  logical register.
	*/
	//std::cout << "\n rename_rdst";
	// pop free entry from FL
	uint64_t phys_reg = this->__FL_pop__();
	// update RMT
	this->RMT[log_reg] = phys_reg;
	
	return phys_reg;
}

uint64_t renamer :: checkpoint()
{
	/*
		- find a free entry in gbm ( == 0). -> #pos
		- checkpoint data @ branch_checkpoint[#pos]
			- cp_RMT
			- FL head and head phase
			- GBM
		- set the entry at #index as 1.
		- branchID = #gbm_pos
		

	*/
	//std::cout << "\ncheckpoint before_gbm : " << this->GBM;

	//std::cout << "\n checkpoint";
	// find a free entry in gbm ( == 0)
	uint64_t branch_id = this->__GBM_free_bit__();

	assert(branch_id < this->n_branches);
	

	// checkpoint branch
	this->__branch_checkpoint__(branch_id);


	// set the entry in gbm
	this->__GBM_set_bit__(branch_id);
	
	//std::cout << "\ncheckpoint after_gbm : " << this->GBM;

	
	
	return branch_id;
	
}



//////////////////////////////////////////
// Functions related to Dispatch Stage. //
//////////////////////////////////////////

bool renamer :: stall_dispatch(uint64_t bundle_inst)
{
	/* The Dispatch Stage must stall if there are not enough free
	   entries in the Active List for all instructions in the current
	   dispatch bundle.
		- AL empty = AL FIFO empty = no inflight instructions in AL -> no stall
		- AL full = AL FIFO full = all inflight instructions  in AL -> stall
		- # free entries in AL = # entries between tail and head of FL
		- # free entries in AL >= bundle_inst
	*/

	// free_AL_entries_count = #empty in AL

	//std::cout << "\n stall_dispatch";
	uint64_t free_AL_entries_count = 0;

	// head < tail and empty
	if(this->AL.head_phase == this->AL.tail_phase)
		free_AL_entries_count = this->AL.length - (this->AL.tail - this->AL.head);

	// head > tail and full
	else
		free_AL_entries_count = this->AL.head - this->AL.tail;

	bool stall = (free_AL_entries_count < bundle_inst ) ? true : false;
	
	return stall;
}

uint64_t renamer :: dispatch_inst(bool dest_valid,
	                       uint64_t log_reg,
	                       uint64_t phys_reg,
	                       bool load,
	                       bool store,
	                       bool branch,
	                       bool amo,
	                       bool csr,
	                       uint64_t PC)
{	
	/*
		push an instruction at the tail of AL
		and return the index of the instruction in AL
	*/
	//std::cout << "\n dispatch_inst";
	uint64_t inst_AL_index = this->__AL_push__(dest_valid,
										  log_reg,
										  phys_reg,
										  load,
										  store,
										  branch,
										  amo,
										  csr,
										  PC);
	
	return inst_AL_index;
}

bool renamer :: is_ready(uint64_t phys_reg)
{	
	// Test the ready bit of the indicated physical register.
	// Returns 'true' if ready.

	//bool is_ready = (this->PRF_ready_array[phys_reg] == true) ? true : false;
	bool is_ready = this->PRF_ready_array[phys_reg];
	//std::cout << "\n is_ready";
	return is_ready;
}

void renamer :: clear_ready(uint64_t phys_reg)
{
	// Clear the ready bit of the indicated physical register.
	this->PRF_ready_array[phys_reg] = false;
	//std::cout << "\n clear_ready";
}

//////////////////////////////////////////
// Functions related to the Reg. Read   //
// and Execute Stages.                  //
//////////////////////////////////////////

uint64_t renamer :: read(uint64_t phys_reg)
{
	// Return the contents (value) of the indicated physical register.
	uint64_t value = this->PRF[phys_reg];
	//std::cout << "\n read";
	return value;
	
}

void renamer :: set_ready(uint64_t phys_reg)
{
	//Set the ready bit of the indicated physical register.

	this->PRF_ready_array[phys_reg] = true;
	//std::cout << "\n set_ready";

}

//////////////////////////////////////////
// Functions related to Writeback Stage.//
//////////////////////////////////////////


void renamer :: write(uint64_t phys_reg, uint64_t value)
{
	// Write a value into the indicated physical register.
	this->PRF[phys_reg] = value;
	//std::cout << "\n write";
	
}

void renamer :: set_complete(uint64_t AL_index)
{
	// Set the completed bit of the indicated entry in the Active List.
	this->AL.FIFO[AL_index].completed_bit = true;
	//std::cout << "\n set_complete";
}


void renamer :: resolve(uint64_t AL_index,
		     uint64_t branch_ID,
		     bool correct)
{
	/*
	- correct branch
		- clear branch's bit in the GBM.
		- clear branch's bit from all checkpointed GBM.

	- mispredicted branch
		- GBM restoration from branch's checkpoint.
		- Restore the RMT using the branch's checkpoint.
		- Restore the Free List head pointer and its phase bit,
		  using the branch's checkpoint.
		- Restore the Active List tail pointer and its phase bit
	      corresponding to the entry after the branch's entry.

	*/
	
	// correct branch
	if(correct)
	{
		//std::cout << "\n correct";
		// clear branch's bit in the GBM
		this->__GBM_clear_bit__(branch_ID);
		// clear branch's bit from all checkpointed GBM
		this->__branch_checkpoint_cp_GBM_clear_bit__(branch_ID);
	}
	// mispredicted branch
	else
	{
		//std::cout << "\n resolve incorrect gbm :" << this->GBM << " branch_id" << branch_ID ;

		// GBM, FL, RMT restore
		this->__branch_checkpoint_GBM_RMT_FL_restore__(branch_ID);
		// AL restore
		this->__AL_restore__(AL_index);

	}
	

}


//////////////////////////////////////////
// Functions related to Retire Stage.   //
//////////////////////////////////////////


bool renamer :: precommit(bool &completed,
                       bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
	               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
		       uint64_t &PC)
{
	/*
		examine instruction at the head of the Active list
		- AL empty = no inst at AL head
			- don't set output arguments
			- return false
		- AL not empty = valid entry at AL head
			- set output arguments
			- return true
	*/
	// std::cout << "\n precommit";
	// std::cout << " FL head :" << this->FL.head << " FL h_p:" << this->FL.head_phase;
	// std::cout << " FL tail :" << this->FL.tail << " FL t_p:" << this->FL.tail_phase;
	bool AL_empty = this->__AL_empty__();
	/*

	error checking code
	*/

	//std::cout << " !AL_empty " << !AL_empty;
	
	if(!AL_empty)
	{
		completed = this->AL.FIFO[this->AL.head].completed_bit;

		exception = this->AL.FIFO[this->AL.head].off_inst_signal.exception_bit;
		load_viol = this->AL.FIFO[this->AL.head].off_inst_signal.load_violation_bit;
		br_misp = this->AL.FIFO[this->AL.head].off_inst_signal.branch_mispred_bit;
		val_misp = this->AL.FIFO[this->AL.head].off_inst_signal.value_mispred_bit;

		load = this->AL.FIFO[this->AL.head].spec_inst_signal.load;
		store = this->AL.FIFO[this->AL.head].spec_inst_signal.store;
		branch = this->AL.FIFO[this->AL.head].spec_inst_signal.branch;
		amo = this->AL.FIFO[this->AL.head].spec_inst_signal.amo;
		csr = this->AL.FIFO[this->AL.head].spec_inst_signal.csr;

		PC = this->AL.FIFO[this->AL.head].PC;
	}
	
	return !AL_empty;
}

void renamer :: commit()
{	
	//std::cout << "\n commit";
	// assert checks

	// there is a head instruction (the active list isn't empty)
	assert(!this->__AL_empty__());

	// the head instruction is completed
	assert(this->AL.FIFO[this->AL.head].completed_bit == true);

	// the head instruction is not marked as an exception
	assert(!this->AL.FIFO[this->AL.head].off_inst_signal.exception_bit);

	//the head instruction is not marked as a load violation
	assert(!this->AL.FIFO[this->AL.head].off_inst_signal.load_violation_bit);

	/*
		- push the phys_reg tag of the destination log_reg in the AMT.
		- AMT[dest_log_reg] = inst.dest_phys_reg
		- pop instruction from AL.
	*/

	// instruction has a destination register
	if(this->AL.FIFO[this->AL.head].dest_reg.dest_valid)
	{
		uint64_t dest_log_reg = this->AL.FIFO[this->AL.head].dest_reg.log_reg;
		uint64_t dest_phys_reg = this->AL.FIFO[this->AL.head].dest_reg.phys_reg;
		uint64_t prev_dest_phys_reg = this->AMT[dest_log_reg];

		// push the previous mapping to the tail of free list
		this->__FL_push__(prev_dest_phys_reg);

		// push the current mapping to the AMT
		this->AMT[dest_log_reg] = dest_phys_reg;
	}

	// pop instruction from head of AL
	ActiveListEntry inst = this->__AL_pop__();
	

}

void renamer :: squash()
{
	/*
		Squash all instructions in the pipeline and 
		roll back to the committed state.
	*/

	//std::cout << "\n squash";
	// RMT restore
	for(uint64_t i=0; i<this->n_log_regs; i++)
		this->RMT[i] = this->AMT[i];

	// GBM restore (also restores branch checkpoints)
	this->GBM = 0;

	// AL restore : AL empty
	this->AL.tail = this->AL.head;
	this->AL.tail_phase = this->AL.head_phase;

	// FL restore : FL full
	this->FL.head = this->FL.tail;
	this->FL.head_phase = !this->FL.tail_phase;

	// PRF ready array restore : make 1 for committed phys regs
	for(uint64_t i=0; i<this->n_log_regs; i++)
	{
		uint64_t phys_reg = this->AMT[i];
		this->PRF_ready_array[phys_reg] = true;
	}
	

}

//////////////////////////////////////////
// Functions not tied to specific stage.//
//////////////////////////////////////////


void renamer :: set_exception(uint64_t AL_index)
{
	this->AL.FIFO[AL_index].off_inst_signal.exception_bit = true;
	//std::cout << "\n set_exception";
}

void renamer :: set_load_violation(uint64_t AL_index)
{
	this->AL.FIFO[AL_index].off_inst_signal.load_violation_bit = true;
	//std::cout << "\n set_load_violation";
}

void renamer :: set_branch_misprediction(uint64_t AL_index)
{
	this->AL.FIFO[AL_index].off_inst_signal.branch_mispred_bit = true;
	//std::cout <<"\n set_branch_misprediction";
}

void renamer :: set_value_misprediction(uint64_t AL_index)
{
	this->AL.FIFO[AL_index].off_inst_signal.value_mispred_bit = true;
	//std::cout <<"\n set_value_misprediction";
}

bool renamer :: get_exception(uint64_t AL_index)
{
	bool exception_bit = this->AL.FIFO[AL_index].off_inst_signal.exception_bit;
	//std::cout << "\n get_exception";
	return exception_bit;
}

