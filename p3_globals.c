/* Please put all global constant and functions here */
#include "p3_globals.h"



Process	processes[P1_MAXPROC];
int	numPages = 0;
int	numFrames = 0;
int *pagers_pids;
int num_pagers = 0;
P1_Semaphore frame_sem;
Frame *frames_list;
P1_Semaphore pager_sem;
P1_Semaphore disk_sem;
P1_Semaphore disk_list_sem;
int *disk_list;
int nextBlock = 0; // used for finding incremental disk blocks to use when assigning blocks to pages



void	*p3_vmRegion = NULL;

P3_VmStats	P3_vmStats;

int pagerMbox = -1;



int enableVerboseDebug = 0; // will print detailed progress for all functions when set to 1

int IsVmInitialized = FALSE;

/**
 * Checks for Kernel Mode
 */
int InKernelMode() {
	if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == USLOSS_PSR_CURRENT_MODE) {
		return 1;
	} else {
		return 0;
	}
}
int interupts_enabled(){
	if((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_INT) == USLOSS_PSR_CURRENT_INT){
		return 1;
	}
	return 0;
}

int isValidPid(int PID) {
	if (PID >= 0 && PID < P1_MAXPROC) {
		return 1;
	}
	return 0;
}


void int_enable() {
	if (enableVerboseDebug == TRUE) {
		USLOSS_Console("int_enable: Enabling interrupts\n");
	}
	USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
}
void int_disable() {
	if (enableVerboseDebug == TRUE) {
		USLOSS_Console("int_disable: Disabling interrupts\n");
	}

	if(interupts_enabled()){
		USLOSS_PsrSet(USLOSS_PsrGet() - USLOSS_PSR_CURRENT_INT );
	}
}


void switchToKernelMode(){
	USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_MODE);
}
void switchToUserMode(){
	if(InKernelMode()){
		USLOSS_PsrSet(USLOSS_PsrGet()&0xfe);
	}
}





/*
 * Helper routines
 */

void
CheckPid(int pid)
{
    if ((pid < 0) || (pid >= P1_MAXPROC)) {
    	USLOSS_Console("Invalid pid\n");
    	USLOSS_Halt(1);
    }
}

void
CheckMode(void)
{
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) {
	   USLOSS_Console("Invoking protected routine from user mode\n");
	   USLOSS_Halt(1);
    }
}

void DebugPrint(char *fmt, ...){
	if(enableVerboseDebug == TRUE){
		va_list ap;
		va_start(ap, fmt);
		USLOSS_VConsole(fmt, ap);
	}
}

void Print_MMU_Error_Code(int error){
	switch (error) {
		case USLOSS_MMU_OK:
			USLOSS_Console("MMU_Error_Code: No error\n");
			break;
		case USLOSS_MMU_ERR_OFF:
			USLOSS_Console("MMU_Error_Code: MMU has not been initialized\n");
			break;
		case USLOSS_MMU_ERR_ON:
			USLOSS_Console("MMU_Error_Code: MMU has already been initialized\n");
			break;
		case USLOSS_MMU_ERR_PAGE:
			USLOSS_Console("MMU_Error_Code: Invalid page number\n");
			break;
		case USLOSS_MMU_ERR_FRAME:
			USLOSS_Console("MMU_Error_Code: Invalid frame number\n");
			break;
		case USLOSS_MMU_ERR_PROT:
			USLOSS_Console("MMU_Error_Code: Invalid protection\n");
			break;
		case USLOSS_MMU_ERR_TAG:
			USLOSS_Console("MMU_Error_Code: Invalid tag\n");
			break;
		case USLOSS_MMU_ERR_REMAP:
			USLOSS_Console("MMU_Error_Code: Mapping with same tag and page already exists\n");
			break;
		case USLOSS_MMU_ERR_NOMAP:
			USLOSS_Console("MMU_Error_Code: Mapping not found\n");
			break;
		case USLOSS_MMU_ERR_ACC:
			USLOSS_Console("MMU_Error_Code: Invalid access bits\n");
			break;
		case USLOSS_MMU_ERR_MAPS:
			USLOSS_Console("MMU_Error_Code: Too many mappings\n");
			break;
		default:
			break;
	}
}


char *get_MMU_PageFrame_Address(int pageNum) {
	int pageSize = USLOSS_MmuPageSize();
	int numPages;
	char *addr = USLOSS_MmuRegion(&numPages);
	return addr + pageNum * pageSize;
}

void set_MMU_PageFrame_contents(int pageNum,  void *str, int size) {

	int numPagesPtr;
	void *region = USLOSS_MmuRegion(&numPagesPtr);
	int *frame_location = region + (pageNum * USLOSS_MmuPageSize());
	// memcpy to buffer
	memcpy(frame_location, str, size);
}

void set_MMU_PageFrame_To_Zeroes(int pageNum) {
	int numPagesPtr;
	int pageSize = USLOSS_MmuPageSize();
	void *region = USLOSS_MmuRegion(&numPagesPtr);
	memset(region + (pageNum * pageSize), 0, USLOSS_MmuPageSize());
}

/** Wrapper for P1_P to print
 */
int P3_P(P1_Semaphore sem, char *name, int id){
	int pid = P1_GetPID();
	DebugPrint("===================================================START\t\tP1_P:\tp_NAME:%s %d\t\tPID:%d\n",name,id,pid);
	int ret =  P1_P(sem);
	DebugPrint("===================================================FINISHED\t\tP1_P:\tp_NAME:%s %d\t\tPID:%d\n", name,id,pid);
	return ret;

}

/** Wrapper for P1_V to print
 */
int P3_V(P1_Semaphore sem, char *name, int id){
	int pid = P1_GetPID();
	DebugPrint("==================================================_____\t\t\tP1_V:\tv_NAME:%s %d\t\tPID:%d\n", name,id,pid);
	int ret =  P1_V(sem);
	return ret;
}
