/*
 * starter_methods.h
 *
 *  Created on: Apr 17, 2016
 *      Author: chris
 */

#ifndef STARTER_METHODS_H_
#define STARTER_METHODS_H_

void starter_p3_quit(int pid);
void starter_FaultHandler(int type, void *arg);
int starter_Pager(void);
void starter_P3_Switch(old, new);
void starter_P3_Fork(pid);
void starter_P3_VmDestroy(void) ;
int starter_P3_VmInit(int mappings, int pages, int frames, int pagers);

#endif /* STARTER_METHODS_H_ */
