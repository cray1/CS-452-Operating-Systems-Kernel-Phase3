/* So that we don't conflict by both writing to phase2.c, write your functions here. Obviously include tests in the test folder however!  */
#include "p3_globals.h"

/*
 *----------------------------------------------------------------------
 *
 * P3_VmInit --
 *
 *	Initializes the VM system by configuring the MMU and setting
 *	up the page tables.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int
P3_VmInit(int mappings, int pages, int frames, int pagers)
{
    int		status;
    int		i;
    int		tmp;

    CheckMode();
    status = USLOSS_MmuInit(mappings, pages, frames);
    if (status != USLOSS_MMU_OK) {
	   USLOSS_Console("P3_VmInit: couldn't initialize MMU, status %d\n", status);
	   USLOSS_Halt(1);
    }
    vmRegion = USLOSS_MmuRegion(&tmp);
    assert(vmRegion != NULL);
    assert(tmp >= pages);
    USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
    for (i = 0; i < P1_MAXPROC; i++) {
    	processes[i].numPages = 0;
    	processes[i].pageTable = NULL;
    }
    /*
     * Create the page fault mailbox and fork the pagers here.
     */

    memset((char *) &P3_vmStats, 0, sizeof(P3_VmStats));
    P3_vmStats.pages = pages;
    P3_vmStats.frames = frames;
    numPages = pages;
    numFrames = frames;
    return numPages * USLOSS_MmuPageSize();
}

/*
 *----------------------------------------------------------------------
 *
 * P3_VmDestroy --
 *
 *	Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
P3_VmDestroy(void)
{
    CheckMode();
    USLOSS_MmuDone();
    /*
     * Kill the pagers here.
     */
    /*
     * Print vm statistics.
     */
    USLOSS_Console("P3_vmStats:\n");
    USLOSS_Console("pages: %d\n", P3_vmStats.pages);
    USLOSS_Console("frames: %d\n", P3_vmStats.frames);
    USLOSS_Console("blocks: %d\n", P3_vmStats.blocks);
    /* and so on... */
}


/*
 *----------------------------------------------------------------------
 *
 * P3_Fork --
 *
 *	Sets up a page table for the new process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A page table is allocated.
 *
 *----------------------------------------------------------------------
 */
void
P3_Fork(pid)
    int		pid;		/* New process */
{
    int		i;

    CheckMode();
    CheckPid(pid);
    processes[pid].numPages = numPages;
    processes[pid].pageTable = (PTE *) malloc(sizeof(PTE) * numPages);
    for (i = 0; i < numPages; i++) {
    	processes[pid].pageTable[i].frame = -1;
    	processes[pid].pageTable[i].block = -1;
    	processes[pid].pageTable[i].state = UNUSED;
    }
}
