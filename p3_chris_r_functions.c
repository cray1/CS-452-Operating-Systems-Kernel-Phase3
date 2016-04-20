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
	DebugPrint("P3_Quit called, current PID: %d, %d\n", P1_GetPID(), pid);

	
	P1_P(process_sem);
	if (IsVmInitialized == TRUE && processes[pid].numPages > 0 && processes[pid].pageTable != NULL ) { // do nothing if  VM system is uninitialized

		CheckMode();
		CheckPid(pid);
		
		assert(processes[pid].numPages > 0);
		assert(processes[pid].pageTable != NULL);

		/*
		 * Free any of the process's pages that are on disk and free any page frames the
		 * process is using.
		 */
		 
		int i;
		i = 0;
		for(i=0; i<numPages; i++){
			frames_list[processes[pid].pageTable[i].frame] = UNUSED;
			processes[pid].pageTable[i].frame = -1;
			processes[pid].pageTable[i].state = UNUSED;
			USLOSS_MmuUnmap(TAG,i);
		}

		/* Clean up the page table. */
		free(processes[pid].pageTable); //this is where basic fails
		processes[pid].numPages = 0;
		processes[pid].pageTable = NULL;
	}

	P1_V(process_sem);
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
		
		P1_P(process_sem);
		P3_vmStats.faults++;
		P1_V(process_sem);
		
		fault.pid = P1_GetPID();
		fault.addr = arg;
		fault.mbox = P2_MboxCreate(1, sizeof(fault));
		int page = ((int)fault.addr / USLOSS_MmuPageSize()) ;
		fault.page = page;

			
		assert(fault.mbox >= 0);
		size = sizeof(fault);
		DebugPrint("FaultHandler: sending on pagerMbox, page %d current PID: %d!\n",page, P1_GetPID());
		status = P2_MboxSend(pagerMbox, &fault, &size);
		DebugPrint("FaultHandler: done sending on pagerMbox, page %d current PID: %d!\n", page, P1_GetPID());

		assert(status >= 0);
		assert(size == sizeof(fault));
		size = 0;
		DebugPrint("FaultHandler: waiting to receive on fault mbox %d, page %d current PID: %d!\n", fault.mbox,  page, P1_GetPID());
		status = P2_MboxReceive(fault.mbox, NULL, &size);
		DebugPrint("FaultHandler: done receive on fault mbox %d, page %d current PID: %d!\n", fault.mbox,  page, P1_GetPID());
		assert(status == 0);
		DebugPrint("FaultHandler: releasing fault mbox %d, page %d current PID: %d!\n", fault.mbox, page,  P1_GetPID());
		status = P2_MboxRelease(fault.mbox);
		DebugPrint("FaultHandler: done releasing fault mbox %d, page %d current PID: %d!\n", fault.mbox, page,  P1_GetPID());
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
		
		//P1_P(pager_sem);
		
		
		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Find a free frame */
		int freeFrameFound = FALSE;
		int freeFrameId;
		for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {
			P1_P(process_sem);
			if (frames_list[freeFrameId] == UNUSED) {
				freeFrameFound = TRUE;
				frames_list[freeFrameId] = 1;
				P1_V(process_sem);
				break;
			}
			P1_V(process_sem);
		}
	 
		int page = fault.page;
		
		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound != TRUE) {
			//run clock algorithm //Part B
		}

		if (freeFrameFound == TRUE) {
			/*To handle a page fault a pager must first allocate a frame to hold the page. First, it checks a free list of frames.
			 * If a free frame is found the page table entry for the faulted process is updated to refer to the free frame
			 * */
			
			P1_P(process_sem);
			DebugPrint("Pager: found free frame %d, current PID: %d!\n", freeFrameId, P1_GetPID());
			processes[fault.pid].pageTable[page].state = INCORE;
			processes[fault.pid].pageTable[page].frame = freeFrameId; //TODO: 1:1 mapping may not hold true
			P1_V(process_sem);
			
			DebugPrint("Pager: mapping frame %d to page %d, current PID: %d!\n", freeFrameId,page, P1_GetPID());
			int errorCode = USLOSS_MmuMap(TAG, page, freeFrameId, USLOSS_MMU_PROT_RW);
			DebugPrint("Pager: done mapping frame %d to page %d, current PID: %d!\n", freeFrameId,page, P1_GetPID());
			if (errorCode == USLOSS_MMU_OK) {

				/* Load page into frame from disk (Part B) or fill with zeros (Part A) */ //
				DebugPrint("Pager: filling page %d frame %d with zeroes , current PID: %d!\n", page, freeFrameId, P1_GetPID());
				DebugPrint("zero\n");
				set_MMU_Frame_To_Zeroes(page,freeFrameId);
				DebugPrint("after zero\n");
				
				//void *destination = vmRegion + (USLOSS_MmuPageSize() * page);

				// copy nothing into the fram
				//memset(destination, 0, USLOSS_MmuPageSize());
				USLOSS_MmuSetAccess(freeFrameId, 0);

				// unmap
				//errorCode = USLOSS_MmuUnmap(0, page);
				
				
				DebugPrint("Pager: done filling  page %d frame %d with zeroes , current PID: %d!\n", page, freeFrameId,  P1_GetPID());
			} else {
				// report error and abort
				Print_MMU_Error_Code(errorCode);
				USLOSS_Trace(
						"Pager with pid %d received an MMU error! Halting!\n",
						P1_GetPID());
				P1_DumpProcesses();
				USLOSS_Halt(1);
			}
			DebugPrint("Pager: unmapping page %d , current PID: %d!\n", page, P1_GetPID());
			errorCode = USLOSS_MmuUnmap(TAG,freeFrameId);
			DebugPrint("Pager: done unmapping page %d , current PID: %d!\n", page, P1_GetPID());
		}

		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Unblock waiting (faulting) process */
		
		P2_MboxSend(fault.mbox, &fault, &size);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		//P1_V(pager_sem);
	}
	/* Never gets here. */
	return 1;
}
