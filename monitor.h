

#ifndef MONITOR_H_
#define MONITOR_H_

#include "p3_globals.h"

typedef struct Condition {
	P1_Semaphore waiting;
	int count; // # processes waiting
} Condition;

typedef struct Lock {
	P1_Semaphore sm;
} Lock;

Lock *lock_init();
void lock_aquire(Lock *l);
void lock_release(Lock *l);
void lock_free(Lock *l);

Condition *cv_init();
void cv_wait(Condition *cond) ;
void cv_signal(Condition *cond);
void cv_broadcast(Condition *cond);
void cv_free(Condition *cond);

#endif
