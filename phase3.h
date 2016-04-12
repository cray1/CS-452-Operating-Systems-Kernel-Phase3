/*
 * Definitions for Phase 3 of the project (virtual memory).
 */
#ifndef _PHASE3_H
#define _PHASE3_H

#ifndef PHASE_3
#define PHASE_3
#endif

#include <usloss.h>
#include <stdio.h>
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>

/*
 * Maximum number of pager processes.
 */
#define P3_MAX_PAGERS   3

/*
 * Pager priority.
 */
#define P3_PAGER_PRIORITY   2

/*
 * Paging statistics
 */
typedef struct P3_VmStats {
    int pages;      /* Size of VM region, in pages */
    int frames;     /* Size of physical memory, in frames */
    int blocks;     /* Size of disk, in blocks (pages) */
    int freeFrames; /* # of frames that are not in-use */
    int freeBlocks; /* # of blocks that are not in-use */
    int switches;   /* # of context switches */
    int faults;     /* # of page faults */
    int new;        /* # faults caused by previously unused pages*/
    int pageIns;    /* # faults that required reading page from disk */
    int pageOuts;   /* # faults that required writing a page to disk */
    int replaced;   /* # pages replaced */
} P3_VmStats;

extern P3_VmStats P3_vmStats;

extern int  P3_VmInit(int mappings, int pages, int frames, int pagers);
extern void P3_VmDestroy(void);
extern void P3_Fork(int pid);
extern void P3_Switch(int old, int new);
extern void P3_Quit(int pid);

extern int  P4_Startup(void *);

#endif
