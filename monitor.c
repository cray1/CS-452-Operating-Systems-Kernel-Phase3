
#include "p3_globals.h"




Lock *lock_init(){
	Lock *lock;
	lock = malloc(sizeof(Lock));
	lock->sm = P1_SemCreate(1);
	return lock;
}
void lock_aquire(Lock *l){
	P1_P(l->sm);
}
void lock_release(Lock *l){
	P1_V(l->sm);
}
void lock_free(Lock *l){
	P1_SemFree(l->sm);
	free(l);
}

P1_Semaphore mutex;


Condition *cv_init(){
	Condition *ret = malloc(sizeof(Condition));
	ret->count =0;
	ret->waiting = P1_SemCreate(1);
	mutex = P1_SemCreate(1);
	return ret;
}

void cv_wait(Condition *cond) {
	if(mutex == NULL)
		mutex = P1_SemCreate(1);
	cond->count++;
	P1_V(mutex); // cannot wait in CS
	P1_P(cond->waiting);
	P1_P(mutex); // re-enter CS
}

void cv_signal(Condition *cond) {
	if (cond->count > 0) {
		P1_V(cond->waiting);
		cond->count--;
	}
}

void cv_broadcast(Condition *cond){
	while(cond->count >0){
		P1_V(cond->waiting);
		cond->count--;
	}
}

void cv_free(Condition *cond){
	P1_SemFree(cond->waiting);
	P1_SemFree(mutex);
	P1_SemFree(cond);
}


