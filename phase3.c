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









int P3_Startup(void *arg){
	int p4_pid;
	int status;

	/*
	 *  Fork P4_Startup process. Do this after all other setup code.
	 */
	p4_pid = P2_Spawn("P4_Startup", P4_Startup, NULL, 4 * USLOSS_MIN_STACK, 3);
	p4_pid = P2_Wait(&status);

	return 0;
}










