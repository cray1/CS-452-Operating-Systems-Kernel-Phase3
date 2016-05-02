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
		}
	 
		int page = fault.page;
		int result;
		
		/* If there isn't one run clock algorithm, write page to disk if necessary */
		if (freeFrameFound != TRUE) {
			//run clock algorithm //Part B
			while(freeFrameFound != TRUE){
				// run through each process to find one to use
				int p;
				for(p = 0; p < numFrames; p++){
					//P1_P(process_sem);
					
					//found open process
					if(processes[frames_list[p].process].pageTable[frames_list[p].page].clock == UNUSED && P1_GetPID() != frames_list[p].process){
						
						freeFrameId = p;
						freeFrameFound = TRUE;					
							
						USLOSS_MmuUnmap(0,frames_list[p].page);
						result = USLOSS_MmuMap(0, frames_list[p].page, freeFrameId, USLOSS_MMU_PROT_RW);
						
						if (result != USLOSS_MMU_OK) {
							USLOSS_Console("Pager(1): Mapping error, halting (%d)\n",result);
							USLOSS_Halt(1);
						}
						
						
						int access;
						USLOSS_MmuGetAccess(freeFrameId, &access);
						int dirty = access & USLOSS_MMU_DIRTY;   
						
						//check if dirty bit and write to swap disk 
						if(dirty == USLOSS_MMU_DIRTY){
							
							
							P1_P(process_sem);
							P3_vmStats.pageOuts++;
							P1_V(process_sem);
							
							if(processes[frames_list[p].process].pageTable[frames_list[p].page].block == -1){
								P1_P(process_sem);
								processes[frames_list[p].process].pageTable[frames_list[p].page].block = nextDiskBlock;
								nextDiskBlock++;
								P1_V(process_sem);
							}
							
							void *buffer = malloc(USLOSS_MmuPageSize());
							int *frameAddr = p3_vmRegion + (freeFrameId * USLOSS_MmuPageSize());
							
							memcpy(buffer, frameAddr, USLOSS_MmuPageSize());
							P2_DiskWrite(unit, processes[frames_list[p].process].pageTable[frames_list[p].page].block, 0, track, buffer);
							free(buffer);
						}		

						//zero the page
						int *pageAddr = p3_vmRegion + (frames_list[p].page * USLOSS_MmuPageSize());
						memset(pageAddr, 0, USLOSS_MmuPageSize());
						USLOSS_MmuSetAccess(freeFrameId, 0);

						result = USLOSS_MmuUnmap(0, frames_list[p].page);
						if (result != USLOSS_MMU_OK) {
							USLOSS_Console("Pager(2): Mapping error, halting\n");
							USLOSS_Halt(1);
						}
						
						P1_P(process_sem);
						
						processes[frames_list[p].process].pageTable[frames_list[p].page].frame = -1;
						processes[frames_list[p].process].pageTable[frames_list[p].page].state = UNUSED;
						
						frames_list[p].process = fault.pid;
						frames_list[p].page = page;
						frames_list[p].used = 1;
						
						P1_V(process_sem);
						
						
						break;					
					}else if(processes[frames_list[p].process].pageTable[frames_list[p].page].frame != -1){
						
						P1_P(process_sem);
						processes[frames_list[p].process].pageTable[frames_list[p].page].clock = UNUSED;
						P1_V(process_sem);
					}
				}
			}
		}
		
		//load from disk
		if(processes[fault.pid].pageTable[page].block != -1){
			P1_P(process_sem);		
			P3_vmStats.pageIns++;
			P1_V(process_sem);
			
			char *buffer = malloc(USLOSS_MmuPageSize());
			
			P2_DiskRead(unit, processes[fault.pid].pageTable[page].block, 0, track, buffer);
			
			result = USLOSS_MmuMap(0, page, freeFrameId, 3);
			if (result != USLOSS_MMU_OK) {
				USLOSS_Console("Pager(3): Mapping error, halting\n");
				USLOSS_Halt(1);
			}
			
			void *writeAddr = p3_vmRegion + (page * USLOSS_MmuPageSize());
			
			memcpy(writeAddr, buffer, USLOSS_MmuPageSize());
			free(buffer);

			// unmap
			result = USLOSS_MmuUnmap(0, page);
			if (result != USLOSS_MMU_OK) {
				USLOSS_Console("Pager(4): Mapping error, halting\n");
				USLOSS_Halt(1);
			}
			
			//P1_V(process_sem);
		}else{
            result = USLOSS_MmuMap(0, page, freeFrameId, 3);
            if (result != USLOSS_MMU_OK) {
				USLOSS_Console("Pager(5): Mapping error, halting\n");
                USLOSS_Halt(1);
            }
			
            void *writeAddr = p3_vmRegion + (USLOSS_MmuPageSize() * page);

            memset(writeAddr, 0, USLOSS_MmuPageSize());
            USLOSS_MmuSetAccess(freeFrameId, 0);

            result = USLOSS_MmuUnmap(0, page);
            if (result != USLOSS_MMU_OK) {
				USLOSS_Console("Pager(6): Mapping error, halting\n");
                USLOSS_Halt(1);
            }
		}
		
		P1_P(process_sem);
		if(processes[fault.pid].pageTable[page].init == FALSE){
			processes[fault.pid].pageTable[page].init = TRUE;
			P3_vmStats.new++;
		}
		
		DebugPrint("Pager: found free frame %d, current PID: %d!\n", freeFrameId, P1_GetPID());
		processes[fault.pid].pageTable[page].state = INCORE;
		processes[fault.pid].pageTable[page].frame = freeFrameId; //TODO: 1:1 mapping may not hold true
		processes[fault.pid].pageTable[page].block = -1;
		P1_V(process_sem);

		
		DebugPrint("Pager: send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());

		/* Unblock waiting (faulting) process */
		
		P2_MboxSend(fault.mbox, &fault, &size);

		DebugPrint("Pager: done with send on mbox: %d, current PID: %d!\n", pagerMbox, P1_GetPID());
		
	}
	/* Never gets here. */
	return 1;
}
