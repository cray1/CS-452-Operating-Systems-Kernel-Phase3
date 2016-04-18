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
		DebugPrint("FaultHandler called, type: %d, address arg: %p current PID: %d!\n", type, arg, P1_GetPID());

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
		fault.mbox = P2_MboxCreate(1, sizeof(fault));
		assert(fault.mbox >= 0);
		size = sizeof(fault);
		DebugPrint("FaultHandler: sending on pagerMbox, current PID: %d!\n", P1_GetPID());
		status = P2_MboxSend(pagerMbox, &fault, &size);
		DebugPrint("FaultHandler: done sending on pagerMbox, current PID: %d!\n", P1_GetPID());

		assert(status >= 0);
		assert(size == sizeof(fault));
		size = 0;
		DebugPrint("FaultHandler: waiting to receive on fault mbox %d, current PID: %d!\n", fault.mbox, P1_GetPID());
		status = P2_MboxReceive(fault.mbox, NULL, &size);
		DebugPrint("FaultHandler: done receive on fault mbox %d, current PID: %d!\n", fault.mbox, P1_GetPID());
		assert(status == 0);
		DebugPrint("FaultHandler: releasing fault mbox %d, current PID: %d!\n", fault.mbox, P1_GetPID());
		status = P2_MboxRelease(fault.mbox);
		DebugPrint("FaultHandler: done releasing fault mbox %d, current PID: %d!\n", fault.mbox, P1_GetPID());
		assert(status == 0);
	} else {
		DebugPrint("FaultHandler: number of pagers is %d therefore, doing nothing , current PID: %d\n",
		num_pagers, P1_GetPID());
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
	DebugPrint("Pager called, current PID: %d!\n", P1_GetPID());

	while (1) {
		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		DebugPrint( "Pager waiting to receive on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		P2_MboxReceive(pagerMbox, (void *) &fault, &size);
		
		if(fault.pid == -1){
			P1_Quit(0);
		}

		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Find a free frame */
		int freeFrameFound = FALSE;
		int i;
		for (i = 0; i < numFrames; i++) {
			if (frames_list[i] == UNUSED) {
				freeFrameFound = TRUE;
				frames_list[i] = 1;
				break;
			}
		}
		int page = 0;

		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound != TRUE) {
			//run clock algorithm //Part B
		}

		if (freeFrameFound == TRUE) {
			/*To handle a page fault a pager must first allocate a frame to hold the page. First, it checks a free list of frames.
			 * If a free frame is found the page table entry for the faulted process is updated to refer to the free frame
			 * */

			DebugPrint("Pager: found free frame %d, current PID: %d!\n", i, P1_GetPID());
			processes[fault.pid].pageTable[page].state = INCORE;
			processes[fault.pid].pageTable[page].frame = i; //TODO: 1:1 mapping may not hold true

			DebugPrint("Pager: mapping frame %d to page %d, current PID: %d!\n", i,i, P1_GetPID());
			int errorCode = USLOSS_MmuMap(TAG, page, i, USLOSS_MMU_PROT_RW);
			DebugPrint("Pager: done mapping frame %d to page %d, current PID: %d!\n", i,i, P1_GetPID());
			if (errorCode == USLOSS_MMU_OK) {
				char *segment;
				int pages;
				/* Load page into frame from disk (Part B) or fill with zeros (Part A) */ //
				DebugPrint("Pager: filling with zeroes , current PID: %d!\n",  P1_GetPID());
				set_MMU_Frame_To_Zeroes(0,0);
				DebugPrint("Pager: done filling with zeroes , current PID: %d!\n",  P1_GetPID());
			} else {
				// report error and abort
				Print_MMU_Error_Code(errorCode);
				USLOSS_Trace(
						"Pager with pid %d received an MMU error! Halting!\n",
						P1_GetPID());
				P1_DumpProcesses();
				USLOSS_Halt(1);
			}
			DebugPrint("Pager: unmapping page %d , current PID: %d!\n", i, P1_GetPID());
			errorCode = USLOSS_MmuUnmap(TAG,i);
			DebugPrint("Pager: done unmapping page %d , current PID: %d!\n", i, P1_GetPID());
		}

		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Unblock waiting (faulting) process */
		P2_MboxSend(fault.mbox, NULL, &size);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
	}
	/* Never gets here. */
	return 1;
}
