/*
 * dispatcher.h
 *
 *  Created on: Sep 2, 2019
 *      Author: jean-philippe
 */
//intended to contain routines involving dispatching and process state.
#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include "moreGlobals.h"

int readyQueueId;
int suspendQueueId;
int schedulePrintLimit;

void initReadyQueue();
void initSuspendQueue();
void dispatch();
int readyQueueIsEmpty();
void addToReadyQueue(Process* process);
long terminateProcess(long pid);
long suspendProcess(long pid);
long resumeProcess(long pid);
void multidispatcher();
void startMultidispatcher();

#endif /* DISPATCHER_H_ */
