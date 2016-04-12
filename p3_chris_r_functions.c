#include "p3_globals.h"



/*
 *----------------------------------------------------------------------
 *
 * P3_Quit --
 *
 *	Called when a process quits and tears down the page table
 *	for the process and frees any frames and disk space used
 *      by the process.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
void
P3_Quit(pid)
    int		pid;
{
    CheckMode();
    CheckPid(pid);
    assert(processes[pid].numPages > 0);
    assert(processes[pid].pageTable != NULL);

    /*
     * Free any of the process's pages that are on disk and free any page frames the
     * process is using.
     */

    /* Clean up the page table. */

    free((char *) processes[pid].pageTable);
    processes[pid].numPages = 0;
    processes[pid].pageTable = NULL;
}



/*
 *----------------------------------------------------------------------
 *
 * P3_Switch
 *
 *	Called during a context switch. Unloads the mappings for the old
 *	process and loads the mappings for the new.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the MMU are changed.
 *
 *----------------------------------------------------------------------
 */
void
P3_Switch(old, new)
    int		old;	/* Old (current) process */
    int		new;	/* New process */
{
    int		page;
    int		status;

    CheckMode();
    CheckPid(old);
    CheckPid(new);

    P3_vmStats.switches++;
    for (page = 0; page < processes[old].numPages; page++) {
	/*
	 * If a page of the old process is in memory then a mapping
	 * for it must be in the MMU. Remove it.
	 */
	if (processes[old].pageTable[page].state == INCORE) {
	    assert(processes[old].pageTable[page].frame != -1);
	    status = USLOSS_MmuUnmap(TAG, page);
	    if (status != USLOSS_MMU_OK) {
		  // report error and abort
	    }
	}
    }
    for (page = 0; page < processes[new].numPages; page++) {
	/*
	 * If a page of the new process is in memory then add a mapping
	 * for it to the MMU.
	 */
	if (processes[new].pageTable[page].state == INCORE) {
	    assert(processes[new].pageTable[page].frame != -1);
	    status = USLOSS_MmuMap(TAG, page, processes[new].pageTable[page].frame,
			USLOSS_MMU_PROT_RW);
	    if (status != USLOSS_MMU_OK) {
		  // report error and abort
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 *	Handles an MMU interrupt.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */
void
FaultHandler(type, arg)
    int		type;	/* USLOSS_MMU_INT */
    void	*arg;	/* Address that caused the fault */
{
    int		cause;
    int		status;
    Fault	fault;
    int     size;

    assert(type == USLOSS_MMU_INT);
    cause = USLOSS_MmuGetCause();
    assert(cause == USLOSS_MMU_FAULT);
    P3_vmStats.faults++;
    fault.pid = P1_GetPID();
    fault.addr = arg;
    fault.mbox = P2_MboxCreate(1, 0);
    assert(fault.mbox >= 0);
    size = sizeof(fault);
    status = P2_MboxSend(pagerMbox, &fault, &size);
    assert(status == 0);
    assert(size == sizeof(fault));
    size = 0;
    status = P2_MboxReceive(fault.mbox, NULL, &size);
    assert(status == 0);
    status = P2_MboxRelease(fault.mbox);
    assert(status == 0);
}

/*
 *----------------------------------------------------------------------
 *
 * Pager
 *
 *	Kernel process that handles page faults and does page
 *	replacement.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Pager(void)
{
    while(1) {
    	/* Wait for fault to occur (receive from pagerMbox) */
    	/* Find a free frame */
    	/* If there isn't one run clock algorithm, write page to disk if necessary */
    	/* Load page into frame from disk or fill with zeros */
    	/* Unblock waiting (faulting) process */
    }
    /* Never gets here. */
    return 1;
}
