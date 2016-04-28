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
		P3_vmStats.faults++;

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
		  int frame = processes[P1_GetPID()].pageTable[page].frame;
		  int result = USLOSS_MmuMap(0, page, frame, USLOSS_MMU_PROT_RW);
		  if (result != USLOSS_MMU_OK) {
		    USLOSS_Console("process %d: Pager failed mapping: %d\n",P1_GetPID(), result);
		    USLOSS_Halt(1);
		  }
	} else {
		DebugPrint("FaultHandler: number of pagers is %d therefore, doing nothing , current PID: %d\n",
				num_pagers, P1_GetPID());
	}
}

int nextBlock = 0; // used for finding incremental disk blocks to use when assigning blocks to pages


int findFrame(int pagerID) {

	DebugPrint("findFrame%i(): started\n", pagerID);

	/* Look for free frame */
	int frame = 0;
	int freeFrames = P3_vmStats.freeFrames != 0;
	for (frame = 0; frame < numFrames-1 && freeFrames; ++frame){
		if(frames_list[frame].state == UNUSED){
			freeFrames = TRUE;
			break;
		}
	}


	DebugPrint("\n\nfindFrame%i(): frame %i (0 base) of %i (1 base)\n", pagerID, frame, numFrames);

	// TODO: Add mutex here
	if(freeFrames == FALSE){
		{
			DebugPrint("Pager%i(): No free frames, starting clock algorithm\n", pagerID);
		}
		// If there isn't one then use clock algorithm to
		// replace a page (perhaps write to disk)
		while(TRUE){

			if((frames_list[frameArm].referenced) == FALSE){
				//remove mutex
				int tmp = frameArm;
				if(++frameArm > numFrames -1)
					frameArm = 0;
				return tmp;
			}
			else{
				// frames_list[frameArm].referenced = referencebit & ~USLOSS_MMU_REF;
				frames_list[frameArm].referenced = FALSE;
				frames_list[frameArm].state = UNUSED;
				USLOSS_MmuSetAccess(frameArm, frames_list[frameArm].referenced);
			}
			if (++frameArm > numFrames-1){
				frameArm = 0;
			}
		}
		// Setting the frame to the first unused frame the clock finds
		frame = frameArm;
	}

	//TODO:  remove mutex here
	return frame;
}

int outputFrame(int pid, int pagenum, void *bufDisk){
	DebugPrint("outputFrame%i(): started, about to write page to disk\n", pid);
	// might switch map and unmap
	// USLOSS_MmuMap(0, pagenum, processes[pid%MAXPROC].pageTable[pagenum].frame, USLOSS_MMU_PROT_RW);
	// USLOSS_MmuUnmap(0, pagenum);
	memcpy(bufDisk, p3_vmRegion, USLOSS_MmuPageSize());
	int track = -1;
	int i;
	for(i = 0; i < NUMTRACKS; i++){
		if(trackBlockTable[i].status == UNUSED){
			track = i;
			break;
		}
	}
	if(track == -1){
		USLOSS_Console("Disk is FULL!\n");
		USLOSS_Halt(1);
	}
	P2_DiskWrite(1, track, 0, 8, bufDisk);
	trackBlockTable[track].status = INCORE;
	P3_vmStats.pageOuts++;
	return track;
}

int getFrame(int pid,int page,void *bufDisk){
  // get diskblock from disk
  int track;
  int frame;
  int i;
  for(i = 0; i < NUMTRACKS; i++)
  {
	if(trackBlockTable[i].pid == pid && trackBlockTable[i].page == page)
	{
		track = i;
		frame = trackBlockTable[i].frame;
		break;
	}
  }
  //int diskblock = processes[pid].pageTable[page].diskBlock;
  P2_DiskRead(1, track, 0, 8, bufDisk);
  P3_vmStats.pageIns++;
  return frame;
}

void printFT(void){
	USLOSS_Console("\n============Frame Table============\n");
	int i;
	for (i = 0; i < numFrames; ++i){
		USLOSS_Console("frames_list[%i].pid: %i\n", i, frames_list[i].pid);
		USLOSS_Console("frames_list[%i].referenced: %i\n", i, frames_list[i].referenced);
		USLOSS_Console("frames_list[%i].clean: %i\n", i, frames_list[i].clean);
		USLOSS_Console("frames_list[%i].state: %i\n", i, frames_list[i].state);
		USLOSS_Console("frames_list[%i].page: %i\n", i, frames_list[i].page);
		USLOSS_Console("frames_list[%i].pid: %i\n\n", i, frames_list[i].pid);
	}
	USLOSS_Console("\n============Frame Table============\n");
}
void printPT(int pid){
	USLOSS_Console("\n============PID %i's Page Table============\n", pid);
	int i;
	for (i = 0; i < processes[pid].numPages; ++i){
		USLOSS_Console("pageTable[%i].state: %i\n", i, processes[pid].pageTable[i].state);
		USLOSS_Console("pageTable[%i].frame %i\n", i, processes[pid].pageTable[i].frame);
		USLOSS_Console("pageTable[%i].trackBlock %i\n\n", i, processes[pid].pageTable[i].trackBlock);
	}
	USLOSS_Console("\n============PID %i's Page Table============\n", pid);

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
	int result;
	int pagerID = P1_GetPID();



	void *bufDisk = malloc(USLOSS_MmuPageSize());

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

		char *pagerName ="[x]";


		DebugPrint("Pager received on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());




		int page = fault.page;
		int pid = fault.pid;

		int frame = findFrame(pagerID);

		DebugPrint("Pager%s(): Frame #%i found\n", pagerName, frame);



		if(frames_list[frame].pid == -1){

			DebugPrint("Pager%s(): Frame #%i is unused\n", pagerName, frame);
			// vmStats.freeFrames--;

			P3_vmStats.new++;
		}
		else{

			DebugPrint("Pager%s(): Frame #%i is USED\n", pagerName, frame);
			P3_vmStats.freeFrames--;
			// The frame is used...
			// get access with USELOSS call
			int refBit;
			USLOSS_MmuUnmap(0,page);

			// int USLOSS_MmuGetAccess(int frame, int *accessPtr);
			// USLOSS_MmuGetAccess(frame, &refBit);
			// result = USLOSS_MmuGetAccess(frame, &refBit);
			// if (result != USLOSS_MMU_OK) {
			//   DebugPrint("process %d: Pager failed mapping: %d\n", getpid(), result);
			//   USLOSS_Halt(1);
			// }

			// Not sure if this bang goes here...
			if (!(frames_list[frame].clean)){

				DebugPrint("Pager%s(): Frame #%i is DIRTY\n", pagerName, frame);
				int trackBlock = outputFrame(pid, page, &bufDisk);
				// store diskBlock in old pagetable Entry
				processes[pid].pageTable[page].trackBlock = trackBlock;
				frames_list[trackBlockTable[trackBlock].frame].pid = pid;
			}

			if (processes[pid].pageTable[page].trackBlock == -1){

				DebugPrint("Pager%s(): page #%i is NOT on disk\n", pagerName, page);
				// This is the easy case of it not being on the disk
				// Page might be 0 because we map it for page?

				int oldPid = frames_list[frame].pid;
				int oldPage = frames_list[frame].page;


				DebugPrint("Pager%s(): old page %i\n", pagerName, oldPage);

				processes[oldPid].pageTable[oldPage].frame = -1;

				// result = USLOSS_MmuMap(0, 0, oldFrame, USLOSS_MMU_PROT_RW);
				//  if (result != USLOSS_MMU_OK) {
				//   DebugPrint("process %d: Pager failed mapping: %d\n", getpid(), result);
				//   USLOSS_Halt(1);
				// }
				USLOSS_MmuUnmap(0, oldPage);

				DebugPrint("Pager%s(): unmaped oldPage %i\n", pagerName, oldPage);

				// memset(p3_vmRegion, '\0', USLOSS_MmuPageSize());


				DebugPrint("Pager%s(): finished memset\n", pagerName);
			}
			else {

				DebugPrint("Pager%s(): page #%i is on disk\n", pagerName, page);
				// If the frame is on disk, get the data and copy it too the p3_vmRegion
				int oldFrame = getFrame(pid, page, bufDisk);

				result = USLOSS_MmuMap(0, 0, oldFrame, USLOSS_MMU_PROT_RW);
				if (result != USLOSS_MMU_OK) {
					DebugPrint("process %d: Pager failed mapping: %d\n", P1_GetPID(), result);
					USLOSS_Halt(1);
				}

				memcpy(p3_vmRegion, bufDisk, USLOSS_MmuPageSize());
			}
		}


		// USLOSS_mmu_setaccess(frame, 0);
		// USLOSS_MmuUnmap(0, page); ?????

		DebugPrint("Pager%s(): About to call USLOSS_MmuMap on page %i and frame %i\n", pagerName, page, frame);




		// cleaning up the frames_list
		frames_list[frame].pid = pid;
		frames_list[frame].state = INCORE;
		frames_list[frame].clean = TRUE;
		frames_list[frame].referenced = TRUE;
		frames_list[frame].page = page;
		processes[pid].pageTable[page].frame = frame;
		processes[pid].pageTable[page].state = USED;

		if(enableVerboseDebug == TRUE)
		{
			printFT();
			printPT(pid);
		}



		DebugPrint("Pager%s(): About to free process %i\n", pagerName, pid);

		DebugPrint("Pager%s(): faultMailbox %i\n", pagerName, fault.mbox);


		/* Unblock waiting (faulting) process */


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
