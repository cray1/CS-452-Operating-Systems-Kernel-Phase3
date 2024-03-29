
#ifndef P3_GLOBALS_H_
#define P3_GLOBALS_H_



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <assert.h>
#include <queue_ll.h>
#include <libuser.h>
#include <monitor.h>
#include "p3_chris_r_functions.h"
#include "p3_chris_s_functions.h"


#define PROC_STATE_RUNNING 0
#define PROC_STATE_READY 1
#define PROC_STATE_KILLED 2
#define PROC_STATE_QUIT 3
#define PROC_STATE_WAITING 4
#define PROC_STATE_INVALID_PID -1
#define TRUE 1
#define FALSE 0

#define MUSTBEINKERNELMODE USLOSS_Trace("P2: A Kernel only function was called from user mode! Not running function!\n ");
extern int enableVerboseDebug;

/* Page Table Entry */
typedef struct PTE {
    int		state;		/* See above. */
    int		frame;		/* The frame that stores the page. */
    int		block;		/* The disk block that stores the page. */
    /* Add more stuff here */
	int 	clock; 		/* references in clock algorithm */
	int 	init;
} PTE;

/*rr
 * Per-process information
 */
typedef struct Process {
    int		numPages;	/* Size of the page table. */
    PTE		*pageTable;	/* The page table for the process. */
    /* Add more stuff here if necessary. */
	int 	has_pages;
	int 	pager_daemon_marked_to_kill;
	P1_Semaphore mutex;
} Process;

/*
 * Information about page faults.
 */
typedef struct Fault {
    int		pid;		/* Process with the problem. */
    void	*addr;		/* Address that caused the fault. */
    int		mbox;		/* Where to send reply. */
    /* Add more stuff here if necessary. */
	int page;
} Fault;

typedef struct Frame {
	int used;
	int page;
	int process;
	int pid;
} Frame;

extern Process	processes[P1_MAXPROC];
extern int	numPages;
extern int	numFrames;
extern int *pagers_pids;
extern int num_pagers;
extern void	*p3_vmRegion;
extern P3_VmStats	P3_vmStats;
extern int pagerMbox;
extern int IsVmInitialized;
extern Frame *frames_list;
extern P1_Semaphore frame_sem;
extern P1_Semaphore pager_sem;
extern P1_Semaphore disk_sem;
extern P1_Semaphore disk_list_sem;
extern int *disk_list;
extern int nextBlock; // used for finding incremental disk blocks to use when assigning blocks to pages


/*
 * Everybody uses the same tag.
 */
#define TAG 0
/*
 * Page table entry
 */

#define UNUSED	0
#define INCORE	1
/* You'll probably want more states */
#define ONDISK 2
#define INUSE 3


void DebugPrint(char *fmt, ...);
/**
 * Checks for Kernel Mode
 */
int InKernelMode();
int interupts_enabled();

int isValidPid(int PID);


void int_enable();
void int_disable();


void switchToKernelMode();
void switchToUserMode();


void CheckPid(int);
void CheckMode(void);

void Print_MMU_Error_Code(int error);
char *get_MMU_Frame_Address(int pageNum);
void set_MMU_PageFrame_contents(int pageNum,  void *str, int size) ;
void set_MMU_PageFrame_To_Zeroes(int pageNum);

int P3_P(P1_Semaphore sem, char *name, int id);
int P3_V(P1_Semaphore sem, char *name, int id);

#endif /* P3_GLOBALS_H_ */
