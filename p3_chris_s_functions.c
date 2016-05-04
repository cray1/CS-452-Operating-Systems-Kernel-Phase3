/* So that we don't conflict by both writing to phase2.c, write your functions here. Obviously include tests in the test folder however!  */
#include "p3_globals.h"

int Pager_Wrapper(void *arg);

/*
 *----------------------------------------------------------------------
 *
 * P3_VmInit --
 *
 *	Initializes the VM system by configuring the MMU and setting
 *	up the page tables.
 *
 * Results:
 *      MMU return status
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
int P3_VmInit(int mappings, int pages, int frames, int pagers) {
	int status;
	int i;
	int tmp;

	DebugPrint("P3_VmInit called, current PID: %d\n", P1_GetPID());
	CheckMode();
	frame_sem = P1_SemCreate(1);
	pager_sem = P1_SemCreate(1);
	disk_sem = P1_SemCreate(1);
	disk_list_sem = P1_SemCreate(1);
	P1_P(frame_sem);

	if (pagers > P3_MAX_PAGERS) {
		// too many pagers
		USLOSS_Trace("P3_VmInit: Too many pagers\n");
		return -1;
	} else {
		num_pagers = pagers;
	}
	pagers_pids = malloc(sizeof(int) * pagers);

	if (mappings > USLOSS_MMU_NUM_TAG * pages) {
		USLOSS_Trace("P3_VmInit: mappings > USLOSS_MMU_NUM_TAG * pages\n");
		return -1;
		// mappings too big
	}

	status = USLOSS_MmuInit(mappings, pages, frames);
	if (status == USLOSS_MMU_ERR_ON) {
		return -2;
	} else if (status != USLOSS_MMU_OK) {
		return -1;
	}
	p3_vmRegion = USLOSS_MmuRegion(&tmp);
	assert(p3_vmRegion != NULL);
	assert(tmp >= pages);
	USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
	for (i = 0; i < P1_MAXPROC; i++) {
		processes[i].numPages = 0;
		processes[i].pageTable = NULL;
		processes[i].mutex = P1_SemCreate(1);
	}
	
	int sector;
	int track;
	int disk;
	P1_P(disk_sem);
		P2_DiskSize(1,&sector, &track, &disk);
	P1_V(disk_sem);
		
	int diskSize = sector * track * disk;
	int numBlocksPerDisk = diskSize /  USLOSS_MmuPageSize();
	
	
	disk_list = malloc(sizeof(int)*numBlocksPerDisk);
	memset(disk_list, 0, sizeof(int)*numBlocksPerDisk);

	/*
	 * Create the page fault mailbox and fork the pagers here.
	 */

	pagerMbox = P2_MboxCreate(P1_MAXPROC, sizeof(Fault)); //added by cray1

	memset((char *) &P3_vmStats, 0, sizeof(P3_VmStats));
	
	P3_vmStats.blocks = numBlocksPerDisk;
	P3_vmStats.freeBlocks = numBlocksPerDisk;
	P3_vmStats.pages = pages;
	P3_vmStats.frames = frames;
	numPages = pages;
	numFrames = frames;

	frames_list = malloc(sizeof(Frame) * numFrames);
	for(i = 0; i < numFrames; i++){
		frames_list[i].used = 0;
		frames_list[i].page = -1;
		frames_list[i].process = -1;
	}

	IsVmInitialized = TRUE; //added by cray1
	if (enableVerboseDebug == TRUE)
		USLOSS_Console("pagers: %d\n", num_pagers);



	for (i = 0; i < num_pagers; i++) {
		char name[10];
		sprintf(name, "Pager_%d", i);
		P1_V(frame_sem);
		pagers_pids[i] = P1_Fork(name, Pager_Wrapper, NULL, USLOSS_MIN_STACK,
		P3_PAGER_PRIORITY);
		P1_P(frame_sem);
		if (enableVerboseDebug == TRUE)
			USLOSS_Console("P3_VmInit:  forked pager with pid %d\n",
					pagers_pids[i]);
	}
	P1_V(frame_sem);
	return 0;
}

int Pager_Wrapper(void *arg) {
	return Pager();
}

/*
 *----------------------------------------------------------------------
 *
 * P3_VmDestroy --
 *
 *	Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void P3_VmDestroy(void) {

	USLOSS_Console("P3_VmDestroy called, current PID: %d\n", P1_GetPID());
	CheckMode();
	int result = USLOSS_MmuDone();
	if (result == USLOSS_MMU_ERR_OFF) {
		return;
	}
	/*
	 * Kill the pagers here.
	 */
	int i;
	for (i = 0; i < num_pagers; i++) {
		P1_P(frame_sem);
		if(isValidPid(pagers_pids[i]) == TRUE)
		{
			processes[pagers_pids[i]].pager_daemon_marked_to_kill = 1;
			P1_Kill(pagers_pids[i]);
		}
		Fault fault;
		int size = sizeof(Fault);
		fault.pid = -1;
		P1_V(frame_sem);

		P2_MboxCondSend(pagerMbox, (void *) &fault, &size);

	}
	/*
	 * Print vm statistics.
	 */
	P1_P(frame_sem);

	P3_vmStats.freeFrames = 0;
	int l;
	for (l = 0; l < numFrames; l++) {
		if(frames_list[l].used == UNUSED){
			P3_vmStats.freeFrames = P3_vmStats.freeFrames +1;
		}
	}


	USLOSS_Console("P3_vmStats:\n");
	USLOSS_Console("pages: %d\n", P3_vmStats.pages);
	USLOSS_Console("frames: %d\n", P3_vmStats.frames);
	USLOSS_Console("blocks: %d\n", P3_vmStats.blocks);
	USLOSS_Console("freeFrames: %d\n", P3_vmStats.freeFrames);
	USLOSS_Console("freeBlocks: %d\n", P3_vmStats.freeBlocks);
	USLOSS_Console("switches: %d\n", P3_vmStats.switches);
	USLOSS_Console("faults: %d\n", P3_vmStats.faults);
	USLOSS_Console("new: %d\n", P3_vmStats.new);
	USLOSS_Console("pageIns: %d\n", P3_vmStats.pageIns);
	USLOSS_Console("pageOuts: %d\n", P3_vmStats.pageOuts);
	USLOSS_Console("replaced: %d\n", P3_vmStats.replaced);
	P1_V(frame_sem);
	/* and so on... */
}

/*
 *----------------------------------------------------------------------
 *
 * P3_Switch
 *
 *	Called during a context switch. Unloads the mappings for the old
 *	process and loads the mappings for the new.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The contents of the MMU are changed.
 *
 *----------------------------------------------------------------------
 */
void P3_Switch(old, new)
	int old; /* Old (current) process */
	int new; /* New process */
{

	if (IsVmInitialized == TRUE) { // do nothing if  VM system is uninitialized

		//DebugPrint("P3_Switch called, current PID: %d\n", P1_GetPID());

		int page, pages;
		int status;

		CheckMode();
		CheckPid(old);
		CheckPid(new);

		//P1_P(processes[old].mutex);
			P3_vmStats.switches++;
			pages = processes[old].numPages;
			//P1_V(process_sem);

			if (processes[old].pageTable != NULL) {
				for (page = 0; page < pages; page++) {
					/*
					 * If a page of the old process is in memory then a mapping
					 * for it must be in the MMU. Remove it.
					 */
					//P1_P(process_sem);
					if (processes[old].pageTable[page].state == INCORE) {
						assert(processes[old].pageTable[page].frame != -1);
						status = USLOSS_MmuUnmap(0, page);
						/*if (status != USLOSS_MMU_OK) {
							// report error and abort
							USLOSS_Console("P3_Switch: ");
							Print_MMU_Error_Code(status);
							USLOSS_Halt(1);
						} */
					}
					//P1_V(process_sem);
				}
			}
		//P1_V(processes[old].mutex);

		//P1_P(processes[new].mutex);
 			pages = processes[new].numPages;

			if (processes[new].pageTable != NULL) {
				for (page = 0; page < pages; page++) {
					/*
					 * If a page of the new process is in memory then add a mapping
					 * for it to the MMU.
					 */
					if (processes[new].pageTable[page].state == INCORE) {
						assert(processes[new].pageTable[page].frame != -1);
						USLOSS_MmuUnmap(TAG, page);
						status = USLOSS_MmuMap(TAG, page,
								processes[new].pageTable[page].frame,
								USLOSS_MMU_PROT_RW);
						if (status != USLOSS_MMU_OK) {
							// report error and abort
							USLOSS_Console("P3_Switch: ");
							Print_MMU_Error_Code(status);
							USLOSS_Halt(1);
						}
						processes[new].pageTable[page].clock = 1;
					}
				}
			}
		//P1_V(processes[new].mutex);


	}
}

/*
 *----------------------------------------------------------------------
 *
 * P3_Fork --
 *
 *	Sets up a page table for the new process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A page table is allocated.
 *
 *----------------------------------------------------------------------
 */
void P3_Fork(pid)
	int pid; /* New process */
{

	DebugPrint("P3_Fork called, current PID: %d\n", P1_GetPID());

	if (IsVmInitialized == TRUE) { // do nothing if  VM system is uninitialized
		int i;

		CheckMode();
		CheckPid(pid);

		P3_P(processes[pid].mutex, "Fork_1", pid);
			processes[pid].has_pages = TRUE;
			processes[pid].numPages = numPages;
			processes[pid].pageTable = (PTE *) malloc(sizeof(PTE) * numPages);
			for (i = 0; i < numPages; i++) {
				processes[pid].pageTable[i].frame = -1;
				processes[pid].pageTable[i].block = -1;
				processes[pid].pageTable[i].state = UNUSED;
				processes[pid].pageTable[i].clock = 0;
				processes[pid].pageTable[i].init = FALSE;
			}
		P3_V(processes[pid].mutex, "Fork_1", pid);
	} else {
		P3_P(processes[pid].mutex, "Fork_2", pid);
			processes[pid].has_pages = FALSE;
		P3_V(processes[pid].mutex, "Fork_2", pid);
	}
}
