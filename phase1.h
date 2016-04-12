/*
 * These are the definitions for Phase 1 of the project (the kernel).
 * DO NOT MODIFY THIS FILE.
 */

#ifndef _PHASE1_H
#define _PHASE1_H

#include "usloss.h"
#include "usyscall.h"

/*
 * Maximum number of processes. 
 */

#define P1_MAXPROC  50

/*
 * Maximum number of semaphores.
 */

#define P1_MAXSEM   1000

typedef void *P1_Semaphore;

/* 
 * Function prototypes for this phase.
 */

extern  int             P1_Fork(char *name, int(*func)(void *), void *arg, 
                                int stackSize, int priority);
extern  void            P1_Quit(int status);
extern  int             P1_Join(int *status);
extern  int             P1_Kill(int pid);
extern  int             P1_GetPID(void);
extern  int             P1_GetState(int pid);
extern  void            P1_DumpProcesses(void);

extern  P1_Semaphore    P1_SemCreate(unsigned int value);
extern  int             P1_SemFree(P1_Semaphore sem);
extern  int             P1_P(P1_Semaphore sem);
extern  int             P1_V(P1_Semaphore sem);

extern  int             P1_WaitDevice(int type, int unit, int *status);
extern  int             P1_ReadTime(void);


extern  int             P2_Startup(void *arg);

#endif /* _PHASE1_H */
