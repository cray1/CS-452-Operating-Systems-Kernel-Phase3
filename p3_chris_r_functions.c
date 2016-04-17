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
void P3_Quit(pid)
	int pid; {

	if (IsVmInitialized == TRUE) { // do nothing if  VM system is uninitialized
		if (enableVerboseDebug == TRUE)
			USLOSS_Console("P3_Quit called, current PID: %d\n", P1_GetPID());
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
void FaultHandler(type, arg)
	int type; /* USLOSS_MMU_INT */
	void *arg; /* Address that caused the fault */
{
	//if (enableVerboseDebug == TRUE)
	//USLOSS_Console("FaultHandler called, current PID: %d\n", P1_GetPID());

	int cause = 0;
	int status = 0;
	Fault fault;
	int size = 0;

	assert(type == USLOSS_MMU_INT);
	cause = USLOSS_MmuGetCause();
	assert(cause == USLOSS_MMU_FAULT);
	if (num_pagers > 0) {
		P3_vmStats.faults++;
		fault.pid = P1_GetPID();
		fault.addr = arg;
		fault.mbox = P2_MboxCreate(1, 0);
		assert(fault.mbox >= 0);
		size = sizeof(fault);
		status = P2_MboxSend(pagerMbox, &fault, &size);
		if (enableVerboseDebug == TRUE)
			USLOSS_Console("FaultHandler: status: %d, current PID: %d\n",
					status, P1_GetPID());

		//assert(status == 0);
		assert(size == sizeof(fault));
		size = 0;
		status = P2_MboxReceive(fault.mbox, NULL, &size);
		assert(status == 0);
		status = P2_MboxRelease(fault.mbox);
		assert(status == 0);
	} else {
		//if (enableVerboseDebug == TRUE)
		//USLOSS_Console(
		//	"FaultHandler: number of pagers is %d therefore, doing nothing , current PID: %d\n",
		//num_pagers, P1_GetPID());
	}
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
int Pager(void) {
	if (enableVerboseDebug == TRUE)
		USLOSS_Console("Pager called, current PID: %d!\n", P1_GetPID());

	while (1) {
		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		if (enableVerboseDebug == TRUE)
			USLOSS_Console(
					"Pager waiting to receive on mbox: %d, current PID: %d!\n",
					pagerMbox, P1_GetPID());
		P2_MboxReceive(pagerMbox, (void *) &fault, &size);

		if (enableVerboseDebug == TRUE)
			USLOSS_Console("Pager received on mbox: %d, current PID: %d!\n",
					pagerMbox, P1_GetPID());

		/* Find a free frame */
		int freeFrameFound = FALSE;
		int i;
		for (i = 0; i < P1_MAXPROC; i++) {
			if (processes[fault.pid].pageTable[i].state == UNUSED) {
				freeFrameFound = TRUE;
				break;
			}
		}

		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound != TRUE) {
			//run clock algorithm //Part B
		}

		if (freeFrameFound == TRUE) {
			/*To handle a page fault a pager must first allocate a frame to hold the page. First, it checks a free list of frames.
			 * If a free frame is found the page table entry for the faulted process is updated to refer to the free frame
			 * */
			processes[fault.pid].pageTable[i].state = INCORE;
			processes[fault.pid].pageTable[i].frame = i; //TODO: 1:1 mapping may not hold true
			int errorCode = USLOSS_MmuMap(TAG, i, i, USLOSS_MMU_PROT_RW);
			if (errorCode == USLOSS_MMU_OK) {
				char *segment;
				int pages;
				/* Load page into frame from disk (Part B) or fill with zeros (Part A) */ //
				segment = USLOSS_MmuRegion(&pages);
				memset(fault.addr, '\0',sizeof(char));
				*segment = '\0';
			} else {
				// report error and abort
				Print_MMU_Error_Code(errorCode);
				USLOSS_Trace(
						"Pager with pid %d received an MMU error! Halting!\n",
						P1_GetPID());
				P1_DumpProcesses();
				USLOSS_Halt(1);
			}
		}

		/* Unblock waiting (faulting) process */
		P2_MboxCondSend(fault.mbox, NULL, &size);
	}
	/* Never gets here. */
	return 1;
}
