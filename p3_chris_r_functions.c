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
			frames_list[processes[pid].pageTable[i].frame].state =  UNUSED;
			frames_list[processes[pid].pageTable[i].frame].page =  -1;
			frames_list[processes[pid].pageTable[i].frame].frameId = -1;
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

int nextBlock = 0; // used for finding incremental disk blocks to use when assigning blocks to pages

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

//	int numPagesPtr;
//	void *region = USLOSS_MmuRegion(&numPagesPtr);

	while (1) {

		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		DebugPrint( "Pager waiting to receive on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		P2_MboxReceive(pagerMbox, (void *) &fault, &size);

		if(fault.pid == -1 || processes[P1_GetPID()].pager_daemon_marked_to_kill == TRUE){
			processes[P1_GetPID()].pager_daemon_marked_to_kill = FALSE;
			P1_Quit(0);
		}



		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		//P1_P(process_sem);
		//P1_P(frame_sem);

		/* Find a free frame */
		int freeFrameFound = FALSE;
		int freeFrameId;
		for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {

			if (frames_list[freeFrameId].state == UNUSED) {
				freeFrameFound = TRUE;
				frames_list[freeFrameId].state = INCORE;
				frames_list[freeFrameId].page = fault.page;
				break;
			}
		}



		int page = fault.page;

		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound != TRUE) {
			USLOSS_Trace("Pager:  did not find free frame , current PID: %d!\n", P1_GetPID());
			//find random frame to use (for now)
			int swapFrameId = 0;
			while(1){
				int i = rand() % 3;
				if(frames_list[i].state == INCORE){
					swapFrameId = i;
					break;
				}
			}
			USLOSS_Trace("\tPager: swapFrameId %d  , current PID: %d!\n",swapFrameId, P1_GetPID());

			int accessPtr;
			USLOSS_MmuGetAccess(swapFrameId, &accessPtr);
			int dirty = accessPtr & USLOSS_MMU_DIRTY;

			//get swap page associated with frame
			int swapPageId =  frames_list[swapFrameId].page;

			USLOSS_Trace("\tPager: swapPageId %d  , current PID: %d!\n",swapPageId, P1_GetPID());

			//if frame is dirty, write its page to swap disk
			if(dirty == USLOSS_MMU_DIRTY){
				USLOSS_Trace("\tPager: swapFrameId %d is dirty  , current PID: %d!\n",swapFrameId, P1_GetPID());

				//write swap frame's page to disk
				// get diskBlock to write to
				int block = processes[fault.pid].pageTable[swapPageId].block;
				if(block <0){
					//get new block
					block = nextBlock;
					nextBlock++;
					processes[fault.pid].pageTable[swapPageId].block = block;
				}
				USLOSS_Trace("\tPager: swapFrameId %d, using block %d  , current PID: %d!\n",swapFrameId, block, P1_GetPID());

				//create buffer to store page
				void *buffer = malloc(USLOSS_MmuPageSize());

				//frame address
				char *frameAddr = "Z";

				//copy to buffer and write to disk
				//memcpy(buffer,frameAddr, USLOSS_MmuPageSize());

				USLOSS_Trace("\tPager: swapFrameId %d, writing to disk  , current PID: %d!\n",swapFrameId, P1_GetPID());
				//write to disk
				P2_DiskWrite(diskUnit,0,0,1,frameAddr);

				free(buffer);
				USLOSS_Trace("\tPager: swapFrameId %d, done writing to disk, current PID: %d!\n",swapFrameId, P1_GetPID());
			}
			//update swap frame owner's page table to reflect that page is no longer in frame
			processes[fault.pid].pageTable[swapPageId].frame = -1;
			processes[fault.pid].pageTable[swapPageId].isInMainMemory = FALSE;

			//if new page on disk, load
			if(processes[fault.pid].pageTable[page].block != -1){
				char *buf = malloc(USLOSS_MmuPageSize());
				P2_DiskRead(diskUnit,processes[fault.pid].pageTable[page].block,0,Disk_Information.numSectorsPerTrack, buf);

				//map memory
				int result = USLOSS_MmuMap(TAG,page, swapFrameId, USLOSS_MMU_PROT_RW);
				if (result != USLOSS_MMU_OK) {
					// report error and abort
					Print_MMU_Error_Code(result);
					USLOSS_Trace( "Pager with pid %d received an MMU error! Halting!\n", P1_GetPID());
					P1_DumpProcesses();
					USLOSS_Halt(1);
				}
				set_MMU_PageFrame_contents(page,buf,USLOSS_MmuPageSize());
			}



		}

		else {
			processes[fault.pid].pageTable[page].state = INCORE;
			processes[fault.pid].pageTable[page].frame = freeFrameId;

			int errorCode = USLOSS_MmuMap(TAG, page, freeFrameId, USLOSS_MMU_PROT_RW);
			if (errorCode == USLOSS_MMU_OK) {
				//if new page, fill with zeros
				if(	processes[fault.pid].pageTable[page].block <0){
					set_MMU_PageFrame_To_Zeroes(page);
					USLOSS_MmuSetAccess(freeFrameId,USLOSS_MMU_PROT_RW);
				}
				else{ // else load from disk

				}
			}
			else {
				// report error and abort
				Print_MMU_Error_Code(errorCode);
				USLOSS_Trace("Pager with pid %d received an MMU error! Halting!\n",P1_GetPID());
				P1_DumpProcesses();
				USLOSS_Halt(1);
			}
			errorCode = USLOSS_MmuUnmap(TAG,freeFrameId);
		}

		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Unblock waiting (faulting) process */
		//P1_V(frame_sem);
		//P1_V(process_sem);

		P2_MboxSend(fault.mbox, &fault, &size);



		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());


	}
	/* Never gets here. */
	return 1;
}
