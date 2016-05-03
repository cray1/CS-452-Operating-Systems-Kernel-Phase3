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
			if(processes[pid].pageTable[i].frame != -1){
				frames_list[processes[pid].pageTable[i].frame].used = UNUSED;
				frames_list[processes[pid].pageTable[i].frame].page = -1;
				frames_list[processes[pid].pageTable[i].frame].process = -1;
			}
			processes[pid].pageTable[i].frame = -1;
			processes[pid].pageTable[i].state = UNUSED;
			processes[pid].pageTable[i].block = -1;
			USLOSS_MmuUnmap(TAG,i);
		}

		/* Clean up the page table. */
		free(processes[pid].pageTable); 
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


P1_Semaphore pager_mutex;
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
	int swapFrameId = 0; //zero for now
	if(pager_mutex == NULL){
		pager_mutex  = P1_SemCreate(1);
	}


	while (1) {

		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		P1_P(pager_sem);
		DebugPrint( "Pager waiting to receive on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		P2_MboxReceive(pagerMbox, (void *) &fault, &size);

		P1_V(pager_sem);

		if(fault.pid == -1 || processes[P1_GetPID()].pager_daemon_marked_to_kill == TRUE){
			P1_Quit(0);
		}

		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		P1_P(pager_mutex);
		/* Find a free frame */
		int freeFrameFound = FALSE;
		int freeFrameId;
		for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {
			//DebugPrint("Before P #1\n");
			P1_P(process_sem);
			if (frames_list[freeFrameId].used == UNUSED) {
				freeFrameFound = TRUE;
				frames_list[freeFrameId].used = INUSE;
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

			//use clock algorithm to find best frame to use
			//find random frame to use (for now)


			P1_P(process_sem);
			while(processes[frames_list[swapFrameId].process].pageTable[frames_list[swapFrameId].page].clock != UNUSED){
				if(frames_list[swapFrameId].used != INUSE){
					processes[frames_list[swapFrameId].process].pageTable[frames_list[swapFrameId].page].clock = UNUSED;
				}
				swapFrameId++;
				swapFrameId = swapFrameId%numFrames;
			}

			int swapPage = frames_list[swapFrameId].page;
			int swapPid = frames_list[swapFrameId].pid;

			frames_list[swapFrameId].used = INUSE;

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
				P3_vmStats.pageOuts++;
				processes[P1_GetPID()].pageTable[swapPage].frame = swapFrameId;
				processes[P1_GetPID()].pageTable[swapPage].state = INCORE;
				int swapBlock = processes[swapPid].pageTable[swapPage].block;
				P1_V(process_sem);

				DebugPrint("Pager: swapBlock [%d], current PID: %d!\n", swapBlock, P1_GetPID());
				if(swapBlock < 0){
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
				//frame address
				void  *frameAddr =  p3_vmRegion + (swapPage * USLOSS_MmuPageSize());

				//copy to buffer and write to disk
				memcpy(buffer, frameAddr, USLOSS_MmuPageSize());
				DebugPrint("Pager: writing to disk, current PID: %d!\n",  P1_GetPID());

				//write to disk
				int test = P2_DiskWrite(unit,processes[swapPid].pageTable[swapPage].block,0,track,buffer);

				DebugPrint("\n\n%s @ %p %d %d %d %d\n\n", buffer, frameAddr, swapPage, swapFrameId, swapBlock, P1_GetPID());
				DebugPrint("Pager: done writing to disk, current PID: %d!\n", P1_GetPID());


				free(buffer);

				USLOSS_MmuUnmap(TAG,swapPage);

				P1_P(process_sem);
				processes[P1_GetPID()].pageTable[swapPage].frame = -1;
				processes[P1_GetPID()].pageTable[swapPage].state = UNUSED;
				processes[swapPid].pageTable[swapPage].state = ONDISK;
				processes[swapPid].pageTable[swapPage].frame = -1;
				P1_V(process_sem);
			}else{
				P1_P(process_sem);
				//update swap frame owning process's page table entry for its page to reflect page no longer being in frame
				processes[swapPid].pageTable[swapPage].frame = -1;
				processes[swapPid].pageTable[swapPage].state = UNUSED;
				P1_V(process_sem);
			}


			frame = swapFrameId;
			swapFrameId++;
			swapFrameId = swapFrameId%numFrames;
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
		frames_list[frame].page = fault.page;
		frames_list[frame].pid = fault.pid;

		int block = processes[fault.pid].pageTable[page].block;

		P1_V(process_sem);
		//if new page
		//fill with zeros
		set_MMU_PageFrame_To_Zeroes(page);
		USLOSS_MmuSetAccess(frame,USLOSS_MMU_PROT_RW);

		if(block >= 0){ //load from disk

			//read page from disk into frame
			char *buf = malloc(USLOSS_MmuPageSize());

			P2_DiskRead(unit,block,0,track, buf);
			// calculate where in the P3_vmRegion to write
			P1_P(process_sem);

			void *destination = p3_vmRegion + (USLOSS_MmuPageSize() * page);

			// copy contents of buffer to the frame
			P3_vmStats.pageIns++;
			memcpy(destination, buf, USLOSS_MmuPageSize());
			P1_V(process_sem);

			// free buffer
			free(buf);

			//processes[fault.pid].pageTable[page].block = -1;
		}
		//unmap from this process so that P3_Switch can map it
		USLOSS_MmuUnmap(TAG,page);
		P1_P(process_sem);

		if(processes[fault.pid].pageTable[page].init == FALSE){
			processes[fault.pid].pageTable[page].init = TRUE;
			P3_vmStats.new++;
		}

		processes[P1_GetPID()].pageTable[page].frame = -1;
		processes[P1_GetPID()].pageTable[page].state = UNUSED;

		frames_list[frame].used = INCORE;
		P1_V(process_sem);

		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());


		/* Unblock waiting (faulting) process */
		P2_MboxSend(fault.mbox, &fault, &size);

		P1_V(pager_mutex);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

	}
	/* Never gets here. */
	return 1;
}

int isFrameUsed(int frameId){
	int accessPtr;
	USLOSS_MmuGetAccess(frameId, &accessPtr);
	int used = accessPtr & USLOSS_MMU_REF;

	if(used == USLOSS_MMU_REF){
		return TRUE;
	}
	return FALSE;
}
