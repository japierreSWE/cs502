/*
 * dispatcher.h
 *
 *  Created on: Sep 2, 2019
 *      Author: jean-philippe
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

#include "processManager.h"

int readyQueueId;

void initReadyQueue();
void dispatch();
int readyQueueIsEmpty();
void addToReadyQueue(Process process);

#endif /* DISPATCHER_H_ */
