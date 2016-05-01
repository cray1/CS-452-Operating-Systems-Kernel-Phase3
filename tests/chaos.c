/*
 * chaos.c
 *  
 *  Stress test for Phase 3. Children run concurrently, reading
 *  and writing pages at random.
 *
 */
#include <usyscall.h>
#include <libuser.h>
#include <assert.h>
#include <mmu.h>
#include <usloss.h>
#include <stdlib.h>
#include <phase3.h>
#include <stdarg.h>
#include <string.h>

#define CHILDREN    4
#define PAGES       4 // # of pages per child
#define FRAMES      (CHILDREN * 2)
#define PRIORITY    3
#define ITERATIONS  100
#define PAGERS      2

char    *fmt = "** Child %d, page %d";
void    *vmRegionTest;
int pageSize;
char    *zeros;

#define Rand(limit) ((int) ((((double)((limit)+1)) * rand()) / \
            ((double) RAND_MAX)))
void
Vm_Config(int *entries, int *pages, int *frames)
{
    *entries = PAGES;
    *pages = PAGES;
    *frames = FRAMES;
}

double
Time(void)
{
    int tod;

    Sys_GetTimeofDay(&tod);
    return tod / 1000000.0;
}

int
Child(void *arg)
{
    int     page;
    int     id = (int) arg;
    int     i;
    char    *target;
    char    buffer[128];
    int     pid;
    int     action;
    int     valid[PAGES];

    Sys_GetPID(&pid);
    USLOSS_Console("%f Child %d (%d) starting\n", Time(), id, pid);
    for (i = 0; i < PAGES; i++) {
       valid[i] = 0;
    }
    for (i = 0; i < ITERATIONS; i++) {
        page = Rand(PAGES-1);
        assert((page >= 0) && (page < PAGES));
        action = Rand(2);
        assert ((action >= 0) && (action <= 2));
        snprintf(buffer, sizeof(buffer), fmt, id, page);
        target = (char *) (vmRegionTest + (page * pageSize));
        if (action == 0) {
            USLOSS_Console("%f: Child %d (%d) writing page %d @ %p\n", Time(), id, pid, page, target);
            strcpy(target, buffer);
            valid[page] = 1;
        } else if (action == 1) {
            if (valid[page]) {
                USLOSS_Console("%f: Child %d (%d) reading page %d @ %p\n", Time(), id, pid, page, target);
                if (strcmp(target, buffer)) {
                    USLOSS_Console("Child %d (%d) read \"%s\" from page %d, not \"%s\"\n",
                        id, pid, target, page, buffer);
                    abort();
                }
            } else {
                USLOSS_Console("%f: Child %d (%d) reading (zero-filled) page %d @ %p\n", 
                    Time(), id, pid, page, target);
                if (memcmp(target, zeros, pageSize)) {
                    USLOSS_Console("Child %d (%d) page %d @ %p is not zero-filled\n",
                        id, pid, page, target);
                    abort();
                }
            }
        } else {
            USLOSS_Console("%f: Child %d (%d) sleeping\n", Time(), id, pid);
            Sys_Sleep(1);
        }
    }
    USLOSS_Console("%f: Child %d done\n", Time(), id);
    return 0;
}

int
P4_Startup(void *arg)
{
    int     child;
    int     pid;
    int     i;
    int     rc;
    char    name[100];

    USLOSS_Console("P4_Startup starting.\n");
    USLOSS_Console("Pages: %d, Frames: %d, Children %d, Iterations %d, Priority %d, Pagers %d\n",
    PAGES, FRAMES, CHILDREN, ITERATIONS, PRIORITY, PAGERS);
    rc = Sys_VmInit(PAGES, PAGES, FRAMES, PAGERS, (void **) &vmRegionTest);
    if (rc != 0) {
        USLOSS_Console("Sys_VmInit failed: %d\n", rc);
        USLOSS_Halt(1);
    }
    assert(vmRegionTest != NULL);

    pageSize = USLOSS_MmuPageSize();
    zeros = malloc(pageSize);
    memset(zeros, 0, pageSize);

    for (i = 0; i < CHILDREN; i++) {
        snprintf(name, sizeof(name), "Child %d", i);
        rc = Sys_Spawn(name, Child, (void *) i, USLOSS_MIN_STACK * 2, PRIORITY, &pid);
        assert(rc == 0);
    }
    for (i = 0; i < CHILDREN; i++) {
        rc = Sys_Wait(&pid, &child);
        assert(rc == 0);
    }
    USLOSS_Console("P3_Startup done\n");
    return 0;
}

void setup(void) {
    // Create the swap disk.
    int     rc;
    int     tracks;
    char    cmd[100];
    tracks = (PAGES * CHILDREN * USLOSS_MmuPageSize() / USLOSS_DISK_SECTOR_SIZE / USLOSS_DISK_TRACK_SIZE) + 1;
    snprintf(cmd, sizeof(cmd), "makedisk 1 %d", tracks);
    rc = system(cmd);
    assert(rc == 0);
}

void cleanup(void) {
}