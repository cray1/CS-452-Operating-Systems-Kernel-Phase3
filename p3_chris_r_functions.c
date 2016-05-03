#include "p3_globals.h"


int isFrameUsed(int frameId){
	int accessPtr;
	USLOSS_MmuGetAccess(frameId, &accessPtr);
	int used = accessPtr & USLOSS_MMU_REF;

	if(used == USLOSS_MMU_REF){
		return TRUE;
	}
	return FALSE;
}

void setFrameUnused(int frameId){
	int accessPtr;
	USLOSS_MmuGetAccess(frameId, &accessPtr);

	USLOSS_MmuSetAccess(frameId, accessPtr & ~USLOSS_MMU_REF); 
}

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
	DebugPrint("=============================================P3_Quit\n");
	if(enableVerboseDebug == TRUE)
		P1_DumpProcesses();
	DebugPrint("=============================================P3_Quit\n");
	 

	P3_P(processes[pid].mutex, "Process_Sem", pid);
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
					P3_P(frame_sem, "Frame_Sem", -1);
						frames_list[processes[pid].pageTable[i].frame].used = UNUSED;
						frames_list[processes[pid].pageTable[i].frame].page = -1;
						frames_list[processes[pid].pageTable[i].frame].process = -1;
					P3_V(frame_sem, "Frame_Sem", -1);
				}
				processes[pid].pageTable[i].frame = -1;
				processes[pid].pageTable[i].state = UNUSED;
				if(processes[pid].pageTable[i].block != -1){
					P3_P(disk_list_sem,"DL_Sem", -1);
						disk_list[processes[pid].pageTable[i].block] = UNUSED;
					P3_V(disk_list_sem,"DL_Sem", -1);
				}
				processes[pid].pageTable[i].block = -1;
				USLOSS_MmuUnmap(TAG,i);
			}

			/* Clean up the page table. */
			free(processes[pid].pageTable);
			processes[pid].numPages = 0;
			processes[pid].pageTable = NULL;
		}
	P3_V(processes[pid].mutex, "Process_Sem", pid);

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
	
	P3_P(pager_sem, "Pager_Sem", -1);
		if(pager_mutex == NULL){
			pager_mutex  = P1_SemCreate(1);
		}
	P3_V(pager_sem, "Pager_Sem", -1);

	while (1) {

		/* Wait for fault to occur (receive from pagerMbox) */
		Fault fault;
		int size = sizeof(Fault);

		P3_P(pager_sem, "Pager_Sem", -1);
			DebugPrint( "Pager waiting to receive on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
			P2_MboxReceive(pagerMbox, (void *) &fault, &size);
		P3_V(pager_sem, "Pager_Sem", -1);

			DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
			if(fault.pid == -1 || processes[P1_GetPID()].pager_daemon_marked_to_kill == TRUE){
						P1_Quit(0);
					}
			P3_P(pager_mutex, "Pager_Mutex", -1);
			/* Find a free frame */
			int freeFrameFound = FALSE;
			int freeFrameId;
			for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {
				//DebugPrint("Before P #1\n");
				P3_P(frame_sem, "Frame_Sem", -1);
					if (frames_list[freeFrameId].used == UNUSED) {
						freeFrameFound = TRUE;
						frames_list[freeFrameId].used = INUSE;
						frames_list[freeFrameId].process = fault.pid;
						frames_list[freeFrameId].page = fault.page;
					P3_V(frame_sem, "Frame_Sem", -1);
						break;
					}
				P3_V(frame_sem, "Frame_Sem", -1);
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
				int clock;
				int tempId;

				while(isFrameUsed(swapFrameId)){
					setFrameUnused(swapFrameId++);
					swapFrameId %= numFrames;
				}
				int pageExists = TRUE;
				P3_P(frame_sem, "Frame_Sem3", -1);
					if(processes[frames_list[swapFrameId].pid].pageTable == NULL){
						pageExists = FALSE;
					}
				
					int swapPage = frames_list[swapFrameId].page;
					int swapPid = frames_list[swapFrameId].pid;
					frames_list[swapFrameId].used = INUSE;
				P3_V(frame_sem, "Frame_Sem3", -1);

				//if swap frame is dirty
				if(isFrameDirty(swapFrameId) == TRUE && pageExists){


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

					P3_vmStats.pageOuts++;
					P3_P(processes[P1_GetPID()].mutex, "Process_Sem3", P1_GetPID());
						processes[P1_GetPID()].pageTable[swapPage].frame = swapFrameId;
						processes[P1_GetPID()].pageTable[swapPage].state = INCORE;
					P3_V(processes[P1_GetPID()].mutex, "Process_Sem3", P1_GetPID());
					
					P3_P(processes[swapPid].mutex, "Process_Sem", swapPid);
						int swapBlock = processes[swapPid].pageTable[swapPage].block;
					P3_V(processes[swapPid].mutex, "Process_Sem", swapPid);

					DebugPrint("Pager: swapBlock [%d], current PID: %d!\n", swapBlock, P1_GetPID());
					if(swapBlock < 0){
						//get new block
						DebugPrint("Pager: swapBlock [%d] is -1, getting new block, current PID: %d!\n", swapBlock, P1_GetPID());

						while(disk_list[nextBlock] != UNUSED){
							P3_P(disk_list_sem, "Frame_Sem5", -1);
								nextBlock = (nextBlock + 1)%track;
							P3_V(disk_list_sem, "Frame_Sem5", -1);
						}

						P3_P(disk_list_sem,"DL_Sem", -1);
							disk_list[nextBlock] = INUSE;
							swapBlock = nextBlock;
							nextBlock = (nextBlock+1)%track;
						P3_V(disk_list_sem,"DL_Sem", -1);
						
						P3_P(processes[swapPid].mutex, "Process_Sem4", swapPid);
							processes[swapPid].pageTable[swapPage].block = swapBlock;
						P3_V(processes[swapPid].mutex, "Process_Sem4", swapPid);
						DebugPrint("Pager: swapBlock assigned [%d], current PID: %d!\n", swapBlock, P1_GetPID());

					}

					//create buffer to store page
					void *buffer = malloc(USLOSS_MmuPageSize());
					//frame address
					void  *frameAddr =  p3_vmRegion + (swapPage * USLOSS_MmuPageSize());

					//copy to buffer and write to disk
					memcpy(buffer, frameAddr, USLOSS_MmuPageSize());
					DebugPrint("Pager: writing to disk, current PID: %d!\n",  P1_GetPID());
					
					P3_P(processes[swapPid].mutex, "Process_Sem", swapPid);
						int b = processes[swapPid].pageTable[swapPage].block;
					P3_V(processes[swapPid].mutex, "Process_Sem", swapPid);

					//write to disk
					P3_P(disk_sem, "Disk_Sem", -1);
						P2_DiskWrite(unit,b,0,track,buffer);
					P3_V(disk_sem, "Disk_Sem", -1);
					
					DebugPrint("\n\n%s @ %p %d %d %d %d\n\n", buffer, frameAddr, swapPage, swapFrameId, swapBlock, P1_GetPID());
					DebugPrint("Pager: done writing to disk, current PID: %d!\n", P1_GetPID());


					free(buffer);

					USLOSS_MmuUnmap(TAG,swapPage);

					USLOSS_Console("swappid: %d\t page: %d\n", swapPid,swapPage);
					P3_P(processes[P1_GetPID()].mutex, "Process_Sem", P1_GetPID());
						processes[P1_GetPID()].pageTable[swapPage].frame = -1;
						processes[P1_GetPID()].pageTable[swapPage].state = UNUSED;
					P3_V(processes[P1_GetPID()].mutex, "Process_Sem", P1_GetPID());
					P3_P(processes[swapPid].mutex, "Process_Sem", swapPid );
						if(processes[swapPid].pageTable != NULL){
							processes[swapPid].pageTable[swapPage].state = ONDISK;
							processes[swapPid].pageTable[swapPage].frame = -1;
						}
					P3_V(processes[swapPid].mutex, "Process_Sem", swapPid);
				}else{
					P3_P(processes[swapPid].mutex, "Process_Sem", swapPid);
					//update swap frame owning process's page table entry for its page to reflect page no longer being in frame
						if(processes[swapPid].pageTable != NULL){
							processes[swapPid].pageTable[swapPage].frame = -1;
							processes[swapPid].pageTable[swapPage].state = UNUSED;
						}
					P3_V(processes[swapPid].mutex, "Process_Sem", swapPid);
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
			P3_P(processes[fault.pid].mutex, "Process_Sem", fault.pid);
				processes[fault.pid].pageTable[page].frame = frame;
				processes[fault.pid].pageTable[page].state = INCORE;
				int block = processes[fault.pid].pageTable[page].block;
			P3_V(processes[fault.pid].mutex, "Process_Sem", fault.pid);

			P3_P(processes[P1_GetPID()].mutex, "Process_Sem", P1_GetPID());
				//same for this process
				processes[P1_GetPID()].pageTable[page].frame = frame;
				processes[P1_GetPID()].pageTable[page].state = INCORE;
			P3_V(processes[P1_GetPID()].mutex, "Process_Sem", P1_GetPID());

			P3_P(frame_sem, "Frame_Sem", -1);
				//update frames_list
				frames_list[frame].page = fault.page;
				frames_list[frame].pid = fault.pid;
			P3_V(frame_sem, "Frame_Sem", -1);

			//if new page
			//fill with zeros
			set_MMU_PageFrame_To_Zeroes(page);
			USLOSS_MmuSetAccess(frame,USLOSS_MMU_PROT_RW);

			if(block >= 0){ //load from disk

				//read page from disk into frame
				char *buf = malloc(USLOSS_MmuPageSize());

				P3_P(disk_sem, "Disk_Sem", -1);
					P2_DiskRead(unit,block,0,track, buf);
				P3_V(disk_sem, "Disk_Sem", -1);
				
				/*P3_P(processes[fault.pid].mutex, "Process_Sem", fault.pid);
					disk_list[block] = UNUSED;
					processes[fault.pid].pageTable[page].block = -1;
				P3_V(processes[fault.pid].mutex, "Process_Sem", fault.pid);
				*/
				
				// calculate where in the P3_vmRegion to write
				//P1_P(frame_sem);

				void *destination = p3_vmRegion + (USLOSS_MmuPageSize() * page);

				// copy contents of buffer to the frame
				P3_vmStats.pageIns++;
				memcpy(destination, buf, USLOSS_MmuPageSize());
				
				
				//P1_V(frame_sem);

				// free buffer
				free(buf);

			}
			//unmap from this process so that P3_Switch can map it
			USLOSS_MmuUnmap(TAG,page);

			P3_P(processes[fault.pid].mutex, "Process_Sem", fault.pid);
				if(processes[fault.pid].pageTable[page].init == FALSE){
					processes[fault.pid].pageTable[page].init = TRUE;
					P3_vmStats.new++;
				}
			P3_V(processes[fault.pid].mutex, "Process_Sem", fault.pid);

			P3_P(processes[P1_GetPID()].mutex, "Process_Sem",P1_GetPID());
				processes[P1_GetPID()].pageTable[page].frame = -1;
				processes[P1_GetPID()].pageTable[page].state = UNUSED;
			P3_V(processes[P1_GetPID()].mutex, "Process_Sem", P1_GetPID());

			P3_P(frame_sem, "Frame_Sem", -1);
				frames_list[frame].used = INCORE;
			P3_V(frame_sem, "Frame_Sem", -1);

			DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		P3_V(pager_mutex, "Pager_Mutex", -1);
		
		/* Unblock waiting (faulting) process */
		P2_MboxSend(fault.mbox, &fault, &size);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

	}
	/* Never gets here. */
	return 1;
}
