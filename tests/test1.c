/*
* memTest.c
*
*	Based on the Zero test just modified.  Each byte within a page is
*	written with some value based on its name.  Then each byte is checked
*	to ensure that everything matches up.
*
*/
#include <usyscall.h>
#include "libuser.h"
#include <assert.h>
#include <mmu.h>
#include "usloss.h"
#include <stdlib.h>
#include <phase3.h>
#include <stdarg.h>
#include <unistd.h>

#define PAGES 5
#define ITERATIONS 4

static char	*vmRegion;
static char	*names[] = { "A","B", "C", "D" };
static int	pageSize;

#ifdef DEBUG
int debugging = 1;
#else
int debugging = 0;
#endif /* DEBUG */




static int
Child(void *arg)
{
	char	*name = (char *)arg;
	int		i, j, k;
	char	*page;

	USLOSS_Console("Child \"%s\" starting.\n", name);
	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < PAGES; j++) {
			page = (char *)(vmRegion + j * pageSize);
			for (k = 0; k < pageSize; k++) {
				page[k] = (char) (*name + (k % 3));				
			}
		}
		Sys_Sleep(1);
	}


	for (i = 0; i < ITERATIONS; i++) {
		for (j = 0; j < PAGES; j++) {
			page = (char *)(vmRegion + j * pageSize);
			for (k = 0; k < pageSize; k++) {

				assert(page[k] == ( (k % 3) + *name));
			}
		}
		Sys_Sleep(1);
	}
	USLOSS_Console("Child \"%s\" done.\n", name);
	return 0;
}

int
P4_Startup(void *arg)
{
	int		i;
	int		rc;
	int		pid;
	int		child;
	int		numChildren = sizeof(names) / sizeof(char *);

	USLOSS_Console("P4_Startup starting.\n");
	rc = Sys_VmInit(PAGES, PAGES, numChildren * PAGES, 3, (void **)&vmRegion);
	USLOSS_Console("P4_Startup: vmRegion obtained after Sys_VmInit call: %x\n", vmRegion);
	/*if (rc != 0) {
	USLOSS_Console("Sys_VmInit failed: %d\n", rc);
	USLOSS_Halt(1);
	}*/
	pageSize = USLOSS_MmuPageSize();
	for (i = 0; i < numChildren; i++) {
		rc = Sys_Spawn(names[i], Child, (void *)names[i], USLOSS_MIN_STACK * 2, 2, &pid);
		assert(rc == 0);
	}
	for (i = 0; i < numChildren; i++) {
		rc = Sys_Wait(&pid, &child);
		assert(rc == 0);
	}
	Sys_VmDestroy();
	USLOSS_Console("P4_Startup done.\n");
	return 0;
}

void setup(void) {
	// Create the swap disk.
	//system("makedisk 1 100");
}

void cleanup(void) {
	// Delete the swap disk.
	/*int rc;
	rc = unlink("disk1");
	assert(rc == 0);*/
}
