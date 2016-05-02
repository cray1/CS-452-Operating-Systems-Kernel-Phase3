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

	/*DebugPrint("P3_Quit called, current PID: %d, %d\n", P1_GetPID(), pid);
	DebugPrint("=============================================P3_Quit\n");
	P1_DumpProcesses();
	DebugPrint("=============================================P3_Quit\n");
	*/
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
			frames_list[processes[pid].pageTable[i].frame].used = UNUSED;
			frames_list[processes[pid].pageTable[i].frame].page = -1;
			frames_list[processes[pid].pageTable[i].frame].process = -1;
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
		
		//P1_P(process_sem);
		P3_vmStats.faults++;
		//P1_V(process_sem);
		
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

int isFrameDirty(int frameId){
	int accessPtr;
	USLOSS_MmuGetAccess(frameId, &accessPtr);
	int dirty = accessPtr & USLOSS_MMU_DIRTY;
	if(dirty == USLOSS_MMU_DIRTY){
		return TRUE;
	}
	return FALSE;
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
	int unit = 1;
	int sector;
    int track;
    int disk;
    P2_DiskSize(unit, &sector, &track, &disk);
	void *temp = malloc(USLOSS_MmuPageSize()*numPages);
	
	while (1) {
		
		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		DebugPrint( "Pager waiting to receive on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		P2_MboxReceive(pagerMbox, (void *) &fault, &size);
			
		if(fault.pid == -1 || processes[P1_GetPID()].pager_daemon_marked_to_kill == TRUE){
			P1_Quit(0);
		}
		
		//P1_P(pager_sem);
		
		
		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Find a free frame */
		int freeFrameFound = FALSE;
		int freeFrameId;
		for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {
			//DebugPrint("Before P #1\n");
			P1_P(process_sem);
			if (frames_list[freeFrameId].used == UNUSED) {
				freeFrameFound = TRUE;
				frames_list[freeFrameId].used = 1;
				frames_list[freeFrameId].process = fault.pid;
				frames_list[freeFrameId].page = fault.page;
				P1_V(process_sem);
				break;
			}
			P1_V(process_sem);
			//DebugPrint("Before V #1\n");
		}
	 
		int page = fault.page;
		int result;
		
		USLOSS_MmuUnmap(TAG,page);

		int frame = 0;
		

		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound == TRUE) {
			DebugPrint("Pager: free Frame found: %d, current PID: %d!\n", freeFrameId, P1_GetPID());
			frame = freeFrameId;
		}
		else{
			P1_P(process_sem);
			//use clock algorithm to find best frame to use
			//find random frame to use (for now)
			int swapFrameId = 0; //zero for now
			int swapPage = frames_list[swapFrameId].page;
			int swapPid = frames_list[swapFrameId].pid;
			
			P1_V(process_sem);
			
			

			//if swap frame is dirty
			if(isFrameDirty(swapFrameId) == TRUE){
				
				
				DebugPrint("Pager: swapFrame [%d] is dirty, current PID: %d!\n", freeFrameId, P1_GetPID());
				//write swap frame to swap disk
				// get diskBlock to write to
								//map page to frame in order to load it into memory
				int mmuResult = USLOSS_MmuMap(TAG, swapPage, swapFrameId, USLOSS_MMU_PROT_RW);
				if(mmuResult != USLOSS_MMU_OK){
					//report error, abort
					Print_MMU_Error_Code(mmuResult);
					USLOSS_Trace("Pager with pid %d received an MMU error! Halting!\n",P1_GetPID());
					P1_DumpProcesses();
					USLOSS_Halt(1);
				}
				
				P1_P(process_sem);
				processes[P1_GetPID()].pageTable[swapPage].frame = swapFrameId;
				processes[P1_GetPID()].pageTable[swapPage].state = INCORE;
				int swapBlock = processes[swapPid].pageTable[swapPage].block;
				P1_V(process_sem);
				
				DebugPrint("Pager: swapBlock [%d], current PID: %d!\n", swapBlock, P1_GetPID());
				if(swapBlock <0){
					//get new block
					DebugPrint("Pager: swapBlock [%d] is -1, getting new block, current PID: %d!\n", swapBlock, P1_GetPID());								
					P1_P(process_sem);
					swapBlock = nextBlock;
					nextBlock++;
					processes[swapPid].pageTable[swapPage].block = swapBlock;
					DebugPrint("Pager: swapBlock assigned [%d], current PID: %d!\n", swapBlock, P1_GetPID());								
					P1_V(process_sem);
				}

				//create buffer to store page
				void *buffer = malloc(USLOSS_MmuPageSize());

				DebugPrint("One\n");
				//frame address
				void  *frameAddr =  p3_vmRegion + (swapPage * USLOSS_MmuPageSize());

				DebugPrint("two\n");
				
				//copy to buffer and write to disk
				memcpy(buffer, frameAddr, USLOSS_MmuPageSize());
				DebugPrint("Pager: writing to disk, current PID: %d!\n",  P1_GetPID());

				//write to disk
				P2_DiskWrite(unit,processes[swapPid].pageTable[swapPage].block,0,sector,buffer);

				DebugPrint("Pager: done writing to disk, current PID: %d!\n", P1_GetPID());


				free(buffer);
				
				USLOSS_MmuUnmap(TAG,swapPage);
				
				P1_P(process_sem);
				processes[P1_GetPID()].pageTable[swapPage].frame = -1;
				processes[P1_GetPID()].pageTable[swapPage].state = UNUSED;
				P1_V(process_sem);
			}
			
			P1_P(process_sem);
			//update swap frame owning process's page table entry for its page to reflect page no longer being in frame
			processes[swapPid].pageTable[swapPage].frame = -1;
			processes[swapPid].pageTable[swapPage].state = ONDISK;
			P1_V(process_sem);

			frame = swapFrameId;
		}
		
		//map page to frame in order to load it into memory
		int mmuResult = USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);
		if(mmuResult != USLOSS_MMU_OK){
			//report error, abort
			Print_MMU_Error_Code(mmuResult);
			USLOSS_Trace("Pager with pid %d received an MMU error! Halting!\n",P1_GetPID());
			P1_DumpProcesses();
			USLOSS_Halt(1);
		}

		//update faulting process's page table entry to map page to frame
		P1_P(process_sem);
		processes[fault.pid].pageTable[page].frame = frame;
		processes[fault.pid].pageTable[page].state = INCORE;

		//same for this process
		processes[P1_GetPID()].pageTable[page].frame = frame;
		processes[P1_GetPID()].pageTable[page].state = INCORE;

		//update frames_list
		frames_list[frame].page = page;
		frames_list[frame].pid = fault.pid;

		int block = processes[fault.pid].pageTable[page].block;

		P1_V(process_sem);
		//if new page
		if(block <0){
			//fill with zeros
			set_MMU_PageFrame_To_Zeroes(page);
			USLOSS_MmuSetAccess(frame,USLOSS_MMU_PROT_RW);
		}
		else{ //load from disk

			//read page from disk into frame
			char *buf = malloc(USLOSS_MmuPageSize());
			P2_DiskRead(unit,block,0,sector, buf);

			// calculate where in the P3_vmRegion to write
			void *destination = p3_vmRegion + (USLOSS_MmuPageSize() * page);

			// copy contents of buffer to the frame
			memcpy(destination, buf, USLOSS_MmuPageSize());

			// free buffer
			free(buf);
			
			processes[fault.pid].pageTable[page].block = -1;
		}
		//unmap from this process so that P3_Switch can map it
		USLOSS_MmuUnmap(TAG,page);
		P1_P(process_sem);
		processes[P1_GetPID()].pageTable[page].frame = -1;
		processes[P1_GetPID()].pageTable[page].state = UNUSED;
		P1_V(process_sem);
		
		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Unblock waiting (faulting) process */
		
		P2_MboxSend(fault.mbox, &fault, &size);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		
	}
	/* Never gets here. */
	return 1;
}
