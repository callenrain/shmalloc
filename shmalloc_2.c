#include "appsupport.h"
#include "shmalloc.h"
#define SHMALLOC_DEBUG


//####################################################################
// Functions from memutils.c
void omp_initenv(int nprocs, int pid)
{ 
	int i;
	gomp_team_t * root_team;
	
	shmalloc_init(STATIC_TCDM_SIZE + sizeof(int));

#ifdef HEAP_HANDLERS
	heap_handler = heap_init(2048);
	//change
	pr("####################################", 0x0, PR_STRING | PR_NEWL);
#endif


	gomp_hal_init_locks(FIRST_FREE_LOCK_ID);

	GLOBAL_IDLE_CORES = nprocs - 1;

	GLOBAL_THREAD_POOL = (1 << MASTER_ID);

	/*
	pr("locks = ", locks, PR_STRING | PR_NEWL | PR_HEX);
	pr("next_lock = ", locks, PR_STRING | PR_NEWL | PR_HEX);
	pr("global_lock = ", locks, PR_STRING | PR_NEWL | PR_HEX);
	pr("next_lock_lock = ", locks, PR_STRING | PR_NEWL | PR_HEX);
	*/

	gomp_hal_init_lock(GLOBAL_LOCK);

	for(i=0; i<nprocs; i++){
		CURR_TEAM(i) = (gomp_team_t *) NULL;
	}

	//Create "main" team descriptor. This also intializes master core's curr_team descriptor 
	//   gomp_team_start (_app_main, NULL, 1, &root_team);
	// Equivalent to GOMP_TEAM_START 
	gomp_master_region_start (/*_app_main*/ (void*) 0x3090, NULL, 1, &root_team);
	
	// Update pointer to first available SHMEM location so that application-level queries to shmalloc
	//are returned values non-overlapping with the  addresses used in the runtime system
	//
	//shmalloc_init(0xa000);
	//pr("After initenv, shmem_next = ", shmem_next, PR_STRING | PR_NEWL | PR_HEX);


/*
	for (i=0; i<NUM_FREE_LISTS; i++) {
		global_data.free_lists[i] = NULL;
	}

	global_data.free_lists[NUM_FREE_LISTS] = shmem_next;
	global_data.free_lists[NUM_FREE_LISTS]->s.next = global_data.free_lists[NUM_FREE_LISTS];
	global_data.free_lists[NUM_FREE_LISTS]->s.prev = global_data.free_lists[NUM_FREE_LISTS];
*/
	pr("Heap usage after Initialization:" , shmem_next - 0x8000000, PR_HEX|PR_STRING|PR_NEWL);
}


inline void print_shmem_utilization() {
    _printdecp("Heap occupation (in bytes) is",
            ((unsigned int) shmem_next) - SHARED_BASE);
}

void shmalloc_init(unsigned int address) {
    shmem_next = SHARED_BASE + address;
    SHMEM_LOCK = (unsigned int) LOCKS(SHMALLOC_LOCK_ID);

    // allocation space for the global pointer
	alloc_global_point = shmem_next;
	shmem_next += sizeof(global_point_t);

	// allocate space for initialization flag
	Init_Flag = shmem_next;
	shmem_next += sizeof(int);

	//Allocate Space for 5 barriers. (ID: 0, 1, 2, 3, 4)
	Barrier_Base = shmem_next;
	shmem_next += 5*Barrier_Size;

    global_data_base = shmem_next;
	shmem_next += sizeof(shmalloc_info);
	base = (Header *) shmem_next;
	freep = NULL; 
    //pr ("After shmalloc_init, shmem_next is: ", shmem_next, PR_STRING|PR_NEWL|PR_HEX);
}


void * shmalloc (int size) {

	pr("\n\nCalled shmalloc with size: ", size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	Header *p, *prevp;
	pr("Size of Header: ", sizeof(Header), PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
	unsigned num_headers = (size + sizeof(Header) - 1)/sizeof(Header) + 1;
	pr("num_headers is: ", num_headers, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	if((prevp = freep) == NULL) {
		base->s.next = freep = prevp = base;
		base->s.size = (AVAILABLE_SHMALLOC_SIZE)/sizeof(Header);
		pr("Initalized number of Headers: ", base->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
	}

	for(p=prevp->s.next;; prevp = p, p = p->s.next) {
		pr("In for loop", 0, PR_CPU_ID | PR_STRING | PR_NEWL);
		if(p->s.size >= num_headers) {
			pr("Address of p: ", (int)p, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
			if (p->s.size <= num_headers + 1) {
				prevp->s.next = p->s.next;
				pr("Found exactly the right size", 0x0, PR_CPU_ID | PR_STRING | PR_NEWL);
			} else {
				pr("Split free memory", 0x0, PR_CPU_ID | PR_STRING | PR_NEWL);
				pr("Original large chunk: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
				p->s.size -= num_headers;
				pr("Smaller Chunk: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
				pr("Reduced by: ", num_headers, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
				pr("p before shift: ", (int)p, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
				p = p + p->s.size;
				pr("p after shift: ", (int)p, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
				p->s.size = num_headers;
				pr("New chunk size: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
			}
			freep = prevp;
			return (void *) (p+1); // return beginning of user's data
		}

		if (p == freep) { // we wrapped around
			pr("[ERROR] Shared malloc is out of Memory!", 0x0, PR_CPU_ID | PR_STRING | PR_NEWL);
			force_shutdown();
		}
	}

}

void shfree(void *ap) {
	Header *bp, *p;
	pr("\n\nIn shfree", 0, PR_CPU_ID | PR_STRING | PR_NEWL);

	bp = (Header *)ap - 1; // bp points to the header of this block
	pr("Size of freeing chunk: ", bp->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	for (p = freep; (bp <= p || bp >= p-> s.next); p = p->s.next) {
		if (p >= p->s.next && (bp > p || bp < p-> s.next)) {
			pr("Using corner case to break", 0, PR_CPU_ID | PR_STRING | PR_NEWL);
			break; // break out of for loop if we are at the beginning or end
		}
	}

	pr("p size before attachment: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	if (bp + bp->s.size == p->s.next) { // bp goes right before p_next
		pr("free chunk is before p's next", 0, PR_CPU_ID | PR_STRING | PR_NEWL);
		pr("bp original size: ", bp->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
		bp->s.size += p->s.next->s.size;
		pr("bp full size: ", bp->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
		bp->s.next = p->s.next->s.next;
	} else { // attach bp's next pointer to p's former next pointer
		bp->s.next = p->s.next;
	}

	if (p + p->s.size == bp) { // bp goes right after p
		pr("free chunk is directly after p", 0, PR_CPU_ID | PR_STRING | PR_NEWL);
		pr("p original size: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
		p->s.size += bp->s.size;
		pr("p full size: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);
		p->s.next = bp->s.next;
	} else { // attach p's next pointer to bp
		p->s.next = bp;
	}

	pr("p_next size after attachment: ", p->s.next->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	pr("p size after attachment: ", p->s.size, PR_CPU_ID | PR_STRING | PR_HEX | PR_NEWL);

	freep = p;
}







//####################################################################
// Functions from lock.c
void gomp_hal_init_locks(int offset) {
	pr("In GHIL", 0, PR_CPU_ID | PR_STRING | PR_NEWL);

	static int locks_inited = 0;
	if (!locks_inited) {
#ifdef HEAP_HANDLERS
		next_lock = (volatile int *) shmalloc(heap_handler, sizeof(int));
#else
		next_lock = (volatile int *) shmalloc(sizeof(int));
		ASSERT((int)next_lock);
		pr("Called GHIL shmalloc", 0, PR_CPU_ID | PR_STRING | PR_NEWL);
#endif
		locks_inited = 1;
	}
	(*next_lock) = SEM_BASE + sizeof(int) * (2 + offset);

}

/* gomp_hal_init_lock () - get a lock */
void gomp_hal_init_lock(unsigned int *id) {
	gomp_hal_lock((unsigned int *) next_lock_lock);
	*id = (*next_lock);
	(*next_lock) += sizeof(int);

	gomp_hal_unlock((unsigned int *) next_lock_lock);
}

/* gomp_hal_lock() - block until able to acquire lock "id" */
ALWAYS_INLINE void gomp_hal_lock(volatile unsigned int *id) {
	while (*id);
}

/* gomp_hal_lock() - release lock "id" */
ALWAYS_INLINE void gomp_hal_unlock(unsigned int *id) {
	*id = 0;
}


//####################################################################
// Functions from team.c

inline int
gomp_resolve_num_threads (int specified)
{ 
  int nthr;
  
  nthr = GLOBAL_IDLE_CORES + 1;
  
  /* If a number of threads has been specified by the user
   * and it is not bigger than max idle threads use that number
   */
  if (specified && (specified < nthr))
    nthr = specified;
  
  GLOBAL_IDLE_CORES -= (nthr - 1);
  
  return nthr;
}

inline gomp_team_t *
gomp_new_team()
{
  gomp_team_t * new_team;
#ifdef HEAP_HANDLERS
  new_team = (gomp_team_t *) shmalloc(heap_handler, sizeof(gomp_team_t));
#else
  new_team = (gomp_team_t *) shmalloc(sizeof(gomp_team_t));
#endif
  return new_team;
}

inline void
gomp_master_region_start (void *fn, void *data, int specified, gomp_team_t **team)
{
	unsigned int i, nprocs, myid, local_id_gen, num_threads,
		curr_team_ptr, my_team_ptr;
	unsigned long long mask;
	gomp_team_t *new_team, *parent_team;
	
	nprocs = prv_num_procs;
	myid = prv_proc_num;
	
	curr_team_ptr = (unsigned int) CURR_TEAM_PTR(0);
	/* Proc 0 calls this... */
	my_team_ptr = curr_team_ptr;
	
	/* Fetch free processor(s) */
	GLOBAL_INFOS_WAIT();


	num_threads = gomp_resolve_num_threads (specified);
	/* Create the team descriptor for current parreg */
	new_team = gomp_new_team();
	
	new_team->omp_task_f = (void *)(fn);
	new_team->omp_args = data;
	new_team->nthreads = num_threads; // also the master

	new_team->team = 0xFFFF;
	
	/* Use the global array to fetch idle cores */
	local_id_gen = 1; // '0' is master
	
	num_threads--; // Decrease NUM_THREADS to account for the master
	new_team->thread_ids[myid] = 0;
	new_team->proc_ids[0] = myid;

#ifdef P2012_HW_BAR
	//NOTE _P2012_ this is the dock barrier
	new_team->hw_event = DOCK_EVENT;
#elif defined (VSOC_HW_BAR) && defined (VSOC)
	new_team->hw_event = get_new_hw_event(16);
	DOCK_EVENT = new_team->hw_event;
#endif
	// to make SW barrier work
	for(i=1; i<prv_num_procs; i++)
	{
		new_team->proc_ids[local_id_gen] = i;
		new_team->thread_ids[i] = local_id_gen++;
	}
	GLOBAL_INFOS_SIGNAL();
	new_team->level = 0;
	
	new_team->parent = 0x0;
	*((gomp_team_t **) my_team_ptr) = new_team;
	*team = new_team;
	
}

//####################################################################
// Functions that we imported from MPARM

// declare a pointer that all cores can get to using get_struct_global
void make_global_point(void * point){
	global_point_t * globaldata =(global_point_t *)alloc_global_point;
	globaldata->global_point=point;  
}

// get the pointer to the global struct
void * get_global_point(){
	global_point_t * globaldata =(global_point_t *)alloc_global_point;
	return globaldata->global_point;  
}

void clear_Init_Flag()
{
	#ifdef ULTRADEBUG
	pr("clear_Init_Flag() invoked", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
	if(*((volatile int*)Init_Flag) == (int)0xdadbebad ){
		*((volatile int*)Init_Flag) = 0;
	}
	#ifdef ULTRADEBUG
	pr("clear_Init_Flag() done!", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
}
	
// WAIT_FOR_INITIALIZATION - Blocks until the system initialization is done
void WAIT_FOR_INITIALIZATION()
{

	// Can't define Init_Flag here, because addresses are MPARM-based
	// need to use shmalloc to allocate space for Init_Flag.
	//volatile int *Init_Flag = (int *)(shared + Init_Flag_OFFSET);
	#ifdef ULTRADEBUG
	pr("WAIT_FOR_INITIALIZATION() invoked", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
	while ( dummy(*((volatile int*)Init_Flag)) != (int)0xdadbebad )
	{
	}
	#ifdef ULTRADEBUG
	pr("WAIT_FOR_INITIALIZATION() done!", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
}


// INITIALIZATION_DONE - Marks the completion of the system initialization
void INITIALIZATION_DONE()
{
	//#ifdef WITH_SHARED_ALLOC
	//		init_multi();                   // initialize remaining portion of shared memory
	//#endif
	//  volatile int *Init_Flag = (int *)(shared + Init_Flag_OFFSET);
	*((volatile int*)Init_Flag) = 0xdadbebad;
	#ifdef ULTRADEBUG
	pr("INITIALIZATION_DONE() done!", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
}

///////////////////////////////////////////////////////////////////////////////
// BARINIT - Initializes the barrier system
// Don't call with id >= 5.
void STD_BARINIT(int ID)
{
	if (ID >= 5){
		pr("Error! BARINIT called with ID >= 5!", 0x0, PR_STRING | PR_NEWL);
		force_shutdown();
	} 
	
	volatile int *BARRIER = (volatile int*)(Barrier_Base + ID * Barrier_Size);
	#ifdef ULTRADEBUG
	pr("BARINIT(ID) done! by ID", ID, PR_CPU_ID | PR_STRING | PR_DEC | PR_TSTAMP | PR_NEWL);
	#endif
	BARRIER[0] = 0;
	BARRIER[1] = 0;
}

///////////////////////////////////////////////////////////////////////////////
// BARRIER - Implements a barrier synchronization
void STD_BARRIER(int ID, int n_proc)
{

	if (ID >= 5){
		pr("Error! BARRIER called with ID >= 5!", 0x0, PR_STRING | PR_NEWL);
		force_shutdown();
	} 
	volatile int *BARRIER = (volatile int*)(Barrier_Base + ID * Barrier_Size);
  
	#ifdef ULTRADEBUG
	pr("BARRIER(ID, n_proc) invoked by ID", ID, PR_CPU_ID | PR_STRING | PR_DEC | PR_TSTAMP | PR_NEWL);
	pr("BARRIER(ID, n_proc) invoked with n_proc", n_proc, PR_CPU_ID | PR_STRING | PR_DEC | PR_TSTAMP | PR_NEWL);
	#endif

	// A previous barrier is already up
	while (dummy(BARRIER[1]) != 0)
	{
	}
  
	WAIT(1);
	BARRIER[0] ++;
	SIGNAL(1);
	  
	#ifdef ULTRADEBUG
	pr("BARRIER(): step 1 done", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
	
	while (dummy(BARRIER[0]) != n_proc)
	{
		#ifdef WITH_POWER_IDLE
		__asm("swi " SWI_CORE_GO_IDLEstr); //go sleep....
		#endif
	}
	
	#ifdef ULTRADEBUG
	pr("BARRIER(): all processes arrived", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
	
	WAIT(1);
	BARRIER[1] ++;
	if (BARRIER[1] == n_proc)
	{
		BARRIER[0] = 0;
		BARRIER[1] = 0;
	}
	SIGNAL(1);
	
	#ifdef ULTRADEBUG
	pr("BARRIER() done!", 0x0, PR_CPU_ID | PR_STRING | PR_TSTAMP | PR_NEWL);
	#endif
}
	
		
