#include "p3_globals.h"

void starter_p3_quit(int pid) {
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

void starter_FaultHandler(int type, void *arg) {
	int cause;
	int status = 0;
	Fault fault;
	int size;

	assert(type == USLOSS_MMU_INT);
	cause = USLOSS_MmuGetCause();
	assert(cause == USLOSS_MMU_FAULT);
	P3_vmStats.faults++;
	fault.pid = P1_GetPID();
	fault.addr = arg;
	fault.mbox = P2_MboxCreate(1, 0);
	assert(fault.mbox >= 0);
	size = sizeof(fault);
	status = P2_MboxSend(pagerMbox, &fault, &size);
	USLOSS_Console("Status: %d\n", status);
	assert(status >= 0);
	assert(size == sizeof(fault));
	size = 0;
	status = P2_MboxReceive(fault.mbox, NULL, &size);
	assert(status >= 0);
	status = P2_MboxRelease(fault.mbox);
	assert(status == 0);
}

int starter_Pager(void) {
	while (1) {
		/* Wait for fault to occur (receive from pagerMbox) */
		/* Find a free frame */
		/* If there isn't one run clock algorithm, write page to disk if necessary */
		/* Load page into frame from disk or fill with zeros */
		/* Unblock waiting (faulting) process */
	}
	/* Never gets here. */
	return 1;
}

/**
 * Test step 1: Sys_VMInit initializes the MMU with an equal
 * number of pages and frames, loads some simple mappings that
 * map page 0 to frame 0, and spawns a child that accesses the VM region.
 */
int starter_P3_VmInit(int mappings, int pages, int frames, int pagers) {


	int status;
	int i;
	int tmp;


	//equal number of pages and frames
	frames = pages;



	CheckMode();
	status = USLOSS_MmuInit(mappings, pages, frames);
	if (status != USLOSS_MMU_OK) {
		USLOSS_Console("P3_VmInit: couldn't initialize MMU, status %d\n",
				status);
		USLOSS_Halt(1);
	}
	vmRegion = USLOSS_MmuRegion(&tmp);
	assert(vmRegion != NULL);
	assert(tmp >= pages);
	USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
	for (i = 0; i < P1_MAXPROC; i++) {
		processes[i].numPages = 0;
		processes[i].pageTable = NULL;
	}
	/*
	 * Create the page fault mailbox and fork the pagers here.
	 */

	memset((char *) &P3_vmStats, 0, sizeof(P3_VmStats));
	P3_vmStats.pages = pages;
	P3_vmStats.frames = frames;
	numPages = pages;
	numFrames = frames;

	// Map page 0 to frame 0
	status = USLOSS_MmuMap(0, 0, 0, USLOSS_MMU_PROT_RW);
	Print_MMU_Error_Code(status);
	assert(status == 0);


	return 0;
	//return numPages * USLOSS_MmuPageSize();
}

void starter_P3_VmDestroy(void) {
	CheckMode();
	USLOSS_MmuDone();
	/*
	 * Kill the pagers here.
	 */
	/*
	 * Print vm statistics.
	 */
	USLOSS_Console("P3_vmStats:\n");
	USLOSS_Console("pages: %d\n", P3_vmStats.pages);
	USLOSS_Console("frames: %d\n", P3_vmStats.frames);
	USLOSS_Console("blocks: %d\n", P3_vmStats.blocks);
	/* and so on... */
}

void starter_P3_Fork(pid)
	int pid; /* New process */
{
	int i;

	CheckMode();
	CheckPid(pid);
	processes[pid].numPages = numPages;
	processes[pid].pageTable = (PTE *) malloc(sizeof(PTE) * numPages);
	for (i = 0; i < numPages; i++) {
		processes[pid].pageTable[i].frame = -1;
		processes[pid].pageTable[i].block = -1;
		processes[pid].pageTable[i].state = UNUSED;
	}
}

void starter_P3_Switch(old, new)
	int old; /* Old (current) process */
	int new; /* New process */
{
	int page;
	int status;

	CheckMode();
	CheckPid(old);
	CheckPid(new);

	P3_vmStats.switches++;
	for (page = 0; page < processes[old].numPages; page++) {
		/*
		 * If a page of the old process is in memory then a mapping
		 * for it must be in the MMU. Remove it.
		 */
		if (processes[old].pageTable[page].state == INCORE) {
			assert(processes[old].pageTable[page].frame != -1);
			status = USLOSS_MmuUnmap(TAG, page);
			if (status != USLOSS_MMU_OK) {
				// report error and abort
			}
		}
	}
	for (page = 0; page < processes[new].numPages; page++) {
		/*
		 * If a page of the new process is in memory then add a mapping
		 * for it to the MMU.
		 */
		if (processes[new].pageTable[page].state == INCORE) {
			assert(processes[new].pageTable[page].frame != -1);
			status = USLOSS_MmuMap(TAG, page,
					processes[new].pageTable[page].frame,
					USLOSS_MMU_PROT_RW);
			if (status != USLOSS_MMU_OK) {
				// report error and abort
			}
		}
	}
}

