
#ifndef P3_GLOBALS_H_
#define P3_GLOBALS_H_



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <queue_ll.h>
#include <libuser.h>
#include <monitor.h>
#include "p3_chris_r_functions.h"
#include "p3_chris_s_functions.h"


#define PROC_STATE_RUNNING 0
#define PROC_STATE_READY 1
#define PROC_STATE_KILLED 2
#define PROC_STATE_QUIT 3
#define PROC_STATE_WAITING 4
#define PROC_STATE_INVALID_PID -1
#define TRUE 1
#define FALSE 0

#define MUSTBEINKERNELMODE USLOSS_Trace("P2: A Kernel only function was called from user mode! Not running function!\n ");

/**
 * Checks for Kernel Mode
 */
int InKernelMode();
int interupts_enabled();

int isValidPid(int PID);


void int_enable();
void int_disable();


void switchToKernelMode();
void switchToUserMode();


#endif /* P3_GLOBALS_H_ */
