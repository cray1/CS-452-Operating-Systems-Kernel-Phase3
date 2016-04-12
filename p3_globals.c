/* Please put all global constant and functions here */
#include "p3_globals.h"



Process	processes[P1_MAXPROC];
int	numPages = 0;
int	numFrames = 0;



void	*vmRegion = NULL;

P3_VmStats	P3_vmStats;

int pagerMbox = -1;



int enableVerboseDebug = 0; // will print detailed progress for all functions when set to 1

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


