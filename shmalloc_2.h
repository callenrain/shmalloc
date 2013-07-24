#ifndef __SHMALLOC_H__
#define __SHMALLOC_H__

//#include "vsoc_config.h"
#include "appsupport.h"
#include "config.h"
#include <stddef.h>


//###################################################################
// Definitions from config.h (not the one in src/core, but the one in shmalloc).
#define SIZEOF_UNSIGNED                 4
#define SIZEOF_UNSIGNED_LONG_LONG       8
#define SIZEOF_PTR                      4
#define SIZEOF_INT                      4
#define SIZEOF_WORD                     0x4 //word size (Bytes) used for interleaving in tcdm xbar

#define STACK_SIZE                      0x400 // 1kB

#define SHR_ICACHE_LINE                 0X4 //line size (Words) used for line interleaving in shared Icache xbar
#define L3_BASE                         CL_L3_BASE
#define L3_SIZE                         CL_L3_SIZE

#define SHARED_BASE                     CL_TCDM_BASE
#define SHARED_SIZE                     CL_TCDM_SIZE  //256 KB
#define BANK_SIZE                       CL_BANK_SIZE
#define BANK_SPACING                    BANK_SIZE //contiguous banks
#ifndef P2012_HW_BAR
  #define NR_LOCKS                      32    //SW BAR ONLY
#else
  #define NR_LOCKS                      64    //HW BAR
#endif
#define SEM_BASE                        CL_SEM_BASE
#define LOCAL_SHARED_OFF                CL_LOCAL_SHARED_OFFSET
#define LOCAL_SHARED_SIZE               CL_LOCAL_SHARED_SIZE
//#endif // __CONFIG_H__

//###################################################################
// Definitions from libgomp_config.h
#ifndef DEFAULT_MAXPROC
#define DEFAULT_MAXPROC			64 
#endif

#define NPROCS					(prv_num_procs)
#define MASTER_ID				(0)

/* Fixed locks */
/* Lock 0 is reserved (see libgomp_globals.h) */
/* #define NEXT_LOCK_LOCK_ADDR SEM_BASE */
#define TRACING_LOCK_ID			1
#define SHMALLOC_LOCK_ID		2

// For VSOC
#define FIRST_FREE_LOCK_ID      HAL_FIRST_FREE_LOCK_ID


/* This is the maximum number of threads we can see in a parallel region
 * In our implementation it is equal to the max number of processors */
#define MAX_PARREG_THREADS DEFAULT_MAXPROC


//###################################################################
// Definitions from lock.c
/*****************Locks global variables*************************/
/* NOTE: Initializing static/global variables declared with LOCAL_SHARED
 * does NOT work, since we don't copy the content of ELF into TCDM.
 * We MUST initialize these variables at the beginning of OMP_INITENV
 */
//volatile int *locks LOCAL_SHARED = (int * volatile ) SEM_BASE;
//volatile int *next_lock LOCAL_SHARED;
//volatile int *global_lock LOCAL_SHARED;
//volatile int *next_lock_lock LOCAL_SHARED = (int * volatile ) SEM_BASE;
static volatile int * locks = (volatile int* ) SEM_BASE;
static volatile int * next_lock = (volatile int*) SEM_BASE;
static volatile int * global_lock = (volatile int* ) SEM_BASE;
static volatile int * next_lock_lock = (volatile int* ) SEM_BASE;

//###################################################################
// Definitions from hal.h
#define HAL_FIRST_FREE_LOCK_ID      3

//###################################################################
// Definitions from memutils.c
#define shmem_next SHMEM_NEXT
#define MEMCHECK_MALLOCS
#define STACK_IN_SHARED

#define SHMEM_CLUSTER_LOCATION 0
#define STATIC_TCDM_SIZE        (MS_BARRIER_SIZE + GLOBAL_INFOS_SIZE + SHMEM_NEXT_SIZE + SHMEM_LOCK_SIZE + CURR_TEAM_SIZE)

//###################################################################
// Definitions from bar.h
#ifndef P2012_HW_BAR
#define MS_BARRIER_SIZE									              (DEFAULT_MAXPROC * SIZEOF_WORD * 2) //512 or 0x200
#else
#define MS_BARRIER_SIZE									              ((DEFAULT_MAXPROC * SIZEOF_WORD * 2) + (2*sizeof(int)))
#endif


//###################################################################
// Definitions from libgomp_globals.h
#ifndef NULL
#define NULL ((void *) 0x0) /* Standard C */
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif /* ALWAYS_INLINE */

#define GOMP_WARN_NOT_SUPPORTED(what) \
  _printstrp("[libGOMP] " what " is not supported yet.");

// different for any proc
#define prv_proc_num (get_proc_id() - 1)
#define prv_num_procs (get_proc_num())


#define WS_INITED     (0xdeadbeef)
#define WS_NOT_INITED (0x0)


#define GLOBAL_INFOS_BASE       (SHARED_BASE + MS_BARRIER_SIZE)
#define GLOBAL_INFOS_SIZE       (sizeof(global_infos_t))

#define GLOBAL_THREAD_POOL      (*((unsigned int*) (GLOBAL_INFOS_BASE)))
#define GLOBAL_IDLE_CORES_ADDR  (GLOBAL_INFOS_BASE + SIZEOF_UNSIGNED)
#define GLOBAL_IDLE_CORES       (*((unsigned int *) GLOBAL_IDLE_CORES_ADDR))
#define GLOBAL_LOCK_ADDR        (GLOBAL_INFOS_BASE + SIZEOF_UNSIGNED + SIZEOF_PTR)
#define GLOBAL_LOCK             ((omp_lock_t*) GLOBAL_LOCK_ADDR)

#define GLOBAL_INFOS_WAIT()		  gomp_hal_lock(*(unsigned int **) GLOBAL_LOCK)
#define GLOBAL_INFOS_SIGNAL()	  gomp_hal_unlock(* (unsigned int **) GLOBAL_LOCK)

#define SHMEM_NEXT_ADDR         (GLOBAL_INFOS_BASE + GLOBAL_INFOS_SIZE)
#define SHMEM_NEXT              (*((unsigned int*) SHMEM_NEXT_ADDR))
#define SHMEM_NEXT_SIZE         (SIZEOF_UNSIGNED)

#define SHMEM_LOCK_ADDR         (SHMEM_NEXT_ADDR + SHMEM_NEXT_SIZE)
#define SHMEM_LOCK              (*((volatile unsigned int *) SHMEM_LOCK_ADDR))
#define SHMEM_LOCK_SIZE         (SIZEOF_UNSIGNED)
#define SHMEM_LOCK_WAIT()		gomp_hal_lock((unsigned int *) SHMEM_LOCK)
#define SHMEM_LOCK_SIGNAL()		gomp_hal_unlock((unsigned int *) SHMEM_LOCK)

#define CURR_TEAM_ADDR			(SHMEM_LOCK_ADDR + SHMEM_LOCK_SIZE)
#define CURR_TEAM_PTR(_id)      ((gomp_team_t **) (CURR_TEAM_ADDR + (_id << 2)))
#define CURR_TEAM(_id)          (*CURR_TEAM_PTR(_id))
#define CURR_TEAM_SIZE          (SIZEOF_PTR * DEFAULT_MAXPROC)

#define STATIC_TCDM_SIZE        (MS_BARRIER_SIZE + GLOBAL_INFOS_SIZE + SHMEM_NEXT_SIZE + SHMEM_LOCK_SIZE + CURR_TEAM_SIZE)

#define NEXT_LOCK_LOCK_ADDR		  (SEM_BASE)
#define NEXT_LOCK_LOCK_PTR		  ((volatile int *) NEXT_LOCK_LOCK_ADDR)

/* End of statically allocated global variables */
#define CURR_WS(_id)            CURR_TEAM(_id)->work_share

//###################################################################
// Definitions from config/vsoc/omp-lock.h
typedef unsigned int omp_lock_t;
//extern volatile int *locks;
//volatile int *locks;
#define OFFSET(_x)    ((_x) << 2)
#define LOCKS(_id)    ((unsigned int *) ((int)locks + OFFSET(_id)))

void omp_set_lock (omp_lock_t *lock);
void omp_unset_lock (omp_lock_t *lock);

/*
*********************************************************************************
*                                                                               *
*                                Structures and                                 *
*                                  Functions                                    *
*                                                                               *
*********************************************************************************
*/
//###########################################################################
// Important functions in memutils.c - purpose of writing this file.
void omp_initenv(int nprocs, int pid);
inline void print_shmem_utilization();
void shmalloc_init(unsigned int address);
inline void * shmalloc (int size);

//####################################################################
// Functions from lock.c
void gomp_hal_init_locks(int offset);
void gomp_hal_init_lock(unsigned int *id);
ALWAYS_INLINE void gomp_hal_lock(volatile unsigned int *id);
ALWAYS_INLINE void gomp_hal_unlock(unsigned int *id);


//###########################################################################
// Structures defined in libgomp_globals.h
/* Threads/tasks support */
typedef struct global_infos_s
{
  unsigned int thread_pool;
  unsigned int idle_cores;
  omp_lock_t* lock;
} global_infos_t;

/* This struct encapsulate a generic work-share
 * (e.g. dynamic loop, sections, etc)
 */
typedef struct gomp_work_share_s
{
	/* This is the "business" lock for the WS */
	omp_lock_t * lock;
	int end;
	int next;
	int chunk_size;
	int incr;
	
	/* These locks are to decouple enter phase (and exit phase)
	* from the "business" phase. If only one WS is defined,
	* they are the same lock (see gomp_new_work_share()) */
	omp_lock_t * enter_lock;
	int checkfirst;
	omp_lock_t * exit_lock;
	unsigned int completed;
	
} gomp_work_share_t;


/* This structure contains all of the thread-local data associated with 
 * a thread team.  This is the data that must be saved when a thread
 * encounters a nested PARALLEL construct.
 */
typedef struct gomp_team_s
{
	/****************** 1) Thread Info ****************************/
	/* This is the function that any thread
	* in the team must run upon launch. */
	void *omp_task_f;
	void *data;
	void *omp_args;
	
	/* Nesting level.  */
	unsigned level;
	struct gomp_team_s *parent;

	/******************** 2) Team Info ****************************/
	/* This is the number of threads in the current team.  */
	unsigned nthreads;
	
	/* This is the team descriptor/mask */
	unsigned /*long long*/ int team; // FIXME int is enough for 1 cluster
	
	/* These are the local ids assigned to processors */
	unsigned int proc_ids[MAX_PARREG_THREADS];
	unsigned int thread_ids[DEFAULT_MAXPROC];
	
#if defined (P2012_HW_BAR) || defined (MPARM_HW_BAR)
	//NOTE _P2012_ HW Event related to TEAM
	int hw_event;
#endif

	/******************** 3) Work Info ****************************/
	/* This is the task that the thread is currently executing.  */
	/* Team (parreg) specific locks */
	omp_lock_t critical_lock; // in GCC's libgomp, they call this default_lock
	omp_lock_t atomic_lock;
	gomp_work_share_t *work_share;

  /******************** 4) (OMP 3.0) Tasks Info *************************/

//   struct gomp_task *task;
//   gomp_mutex_t task_lock;
//   struct gomp_task *task_queue;
//   int task_count;
//   int task_running_count;

/* This array contains structures for implicit tasks.  */
//   struct gomp_task implicit_task[];

} gomp_team_t;


//###########################################################################
// Functions from team.c
inline int gomp_resolve_num_threads (int specified);
inline void gomp_master_region_start (void *fn, void *data, int specified, gomp_team_t **team);
inline gomp_team_t * gomp_new_team();

//###########################################################################
// Stuff we imported from MPARM

volatile unsigned int alloc_global_point;                   // For global pointer
volatile unsigned int Init_Flag;                            // For INITIALIZATION 
#define Barrier_Size         (2 * sizeof(int))     // For Barrier
volatile unsigned int Barrier_Base;

typedef struct {
  void * global_point;
}global_point_t;

void make_global_point(void * point);
void * get_global_point();

void clear_Init_Flag();
void WAIT_FOR_INITIALIZATION();
void INITIALIZATION_DONE();

#ifdef 	USE_SEM_BARRIER
pr("WARNING! Using semophore barriers! Not defined!", 0x0, PR_NEWL | PR_STRING );
#define	BARINIT(_id)         SEM_BARINIT((_id), get_proc_num()) 
#define	BARRIER(_id,_nnodes) SEM_BARRIER((_id), get_proc_id(),(_nnodes) ) 
#else
#define BARINIT(_id)         STD_BARINIT((_id))
#define BARRIER(_id,_nnodes) STD_BARRIER((_id),(_nnodes))
#endif
void STD_BARINIT(int ID);
void STD_BARRIER(int ID, int n_proc);

//cferri semaphore based version of barriers
//not used here! not defined.
void SEM_BARRIER(int ID ,int proc_id, int n_cpu);// ID = barrier ID, 
						// proc_id = ID of the calling core
						// n_cpu = cores pending on the barrier
void SEM_BARINIT(int ID, int n_cpu);

/********************* New Shmalloc and Free Functions *************/

typedef long Align;

union header {
	struct {
		union header *next;
		union header *prev;
		unsigned size;
	} s;
	Align x;
};

typedef union header Header;

static Header * base;
Header * freep;

// how many free linked lists to have
#ifndef NUM_FREE_LISTS
#define NUM_FREE_LISTS (4)
#endif

#define AVAILABLE_SHMALLOC_SIZE CL_LOCAL_SHARED_OFFSET - (shmem_next - 0x08000000)

//int free_l_sizes[NUM_FREE_LISTS]= {16,128,512};

typedef struct {
    Header * first;// pointer to the first block in mem
    //Header * free_lists[NUM_FREE_LISTS];

} shmalloc_info;

shmalloc_info shmalloc_infos;

unsigned global_data_base;

#endif // __SHMALLOC_H__
