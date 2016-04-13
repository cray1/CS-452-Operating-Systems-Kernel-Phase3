/*
 * skeleton.c
 *
 * 	This is a skeleton for phase3 of the programming assignment. It
 *	doesn't do much -- it is just intended to get you started. Feel free 
 *  to ignore it.
 */

#include <assert.h>
#include <phase3.h>
#include <string.h>
#include <p3_globals.h>





typedef struct spawn_wrapper {
	int (*f)(void *);
	void *arg;
} spawn_wrapper;


int P4_Startup_Spawn_Wrapper(void* arg) {
	spawn_wrapper *s = malloc(sizeof(spawn_wrapper));
	s = (spawn_wrapper *) arg;
	int i = s->f(s->arg);
	//P3_Startup should call Sys_VmDestroy if P4_Startup returns
	Sys_VmDestroy();
	return i;
}



int P3_Startup(void *arg){
	int p4_pid;
	int status;





	/*
	 *  Fork P4_Startup process. Do this after all other setup code.
	 */
	//P3_Startup should call Sys_VmDestroy if P4_Startup returns
	spawn_wrapper *helper = malloc(sizeof(spawn_wrapper));
			helper->f = P4_Startup;
			helper->arg = NULL;
	p4_pid = P2_Spawn("P4_Startup", &P4_Startup_Spawn_Wrapper, (void *) helper, 4 * USLOSS_MIN_STACK, 3);
	p4_pid = P2_Wait(&status);

	return 0;
}










