#include "p3_globals.h"


int isFrameDirty(int frameId){
	int accessPtr;
	USLOSS_MmuGetAccess(frameId, &accessPtr);
	int dirty = accessPtr & USLOSS_MMU_DIRTY;
	if(dirty == USLOSS_MMU_DIRTY){
		return TRUE;
	}
	return FALSE;
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
		int page = fault.page;
		int frame = -1;
		/* Find a free frame */
		int freeFrameFound = FALSE;
		int freeFrameId;
		for (freeFrameId = 0; freeFrameId < numFrames; freeFrameId++) {

			if (frames_list[freeFrameId].state == UNUSED) {
				freeFrameFound = TRUE;
				break;
			}
		}

		if(freeFrameFound == TRUE){
			frame = freeFrameId;
		}
		else{
			//use clock algorithm to find best frame to use
			//find random frame to use (for now)
			int swapFrameId = 0; //zero for now
			int swapPage = frames_list[swapFrameId].page;
			int swapPid = frames_list[swapFrameId].pid;

			//if swap frame is dirty
			if(isFrameDirty(swapFrameId) == TRUE){
				//write swap frame to swap disk
				// get diskBlock to write to
				int swapBlock = processes[swapPid].pageTable[swapPage].block;
				if(swapBlock <0){
					//get new block
					swapBlock = nextBlock;
					nextBlock++;
					processes[swapPid].pageTable[swapPage].block = swapBlock;
				}

				//create buffer to store page
				void *buffer = malloc(USLOSS_MmuPageSize());

				//frame address
				void  *frameAddr =  p3_vmRegion + (swapPage * USLOSS_MmuPageSize());

				//copy to buffer and write to disk
				memcpy(buffer,frameAddr, USLOSS_MmuPageSize());

				//write to disk
				P2_DiskWrite(diskUnit,processes[swapPid].pageTable[swapPage].block,0,Disk_Information.numSectorsPerTrack,buffer);

				free(buffer);
			}


			//update swap frame owning process's page table entry for its page to reflect page no longer being in frame
			processes[swapPid].pageTable[swapPage].frame = -1;
			processes[swapPid].pageTable[swapPage].state = ONDISK;

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

		int block = processes[fault.pid].pageTable[page].block;
		//if new page
		if(block <0){
			//fill with zeros
			set_MMU_PageFrame_To_Zeroes(page);
			USLOSS_MmuSetAccess(freeFrameId,USLOSS_MMU_PROT_RW);
		}
		else{ //load from disk

			//read page from disk into frame
			char *buf = malloc(USLOSS_MmuPageSize());
			P2_DiskRead(diskUnit,block,0,Disk_Information.numSectorsPerTrack , buf);

			// calculate where in the P3_vmRegion to write
			void *destination = p3_vmRegion + (USLOSS_MmuPageSize() * page);

			// copy contents of buffer to the frame
			memcpy(destination, buf, USLOSS_MmuPageSize());

			// free buffer
			free(buf);
		}
		//unmap from this process so that P3_Switch can map it
		USLOSS_MmuUnmap(TAG,page);


		//update frames_list
		frames_list[frame].state = INCORE;
		frames_list[frame].page = page;
		frames_list[frame].pid = fault.pid;
		frames_list[frame].frameId = frame;

		//update faulting process's page table entry to map page to frame
		processes[fault.pid].pageTable[page].state = INCORE;
		processes[fault.pid].pageTable[page].frame = frame;

		//send response to faulting process
		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		P2_MboxSend(fault.mbox, &fault, &size);
		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());


	}
	/* Never gets here. */
	return 1;
}
