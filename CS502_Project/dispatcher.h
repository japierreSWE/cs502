/*
 * dispatcher.h
 *
 *  Created on: Sep 2, 2019
 *      Author: jean-philippe
 */

#ifndef DISPATCHER_H_
#define DISPATCHER_H_

int readyQueueId;

void initReadyQueue();
void dispatch();
int readyQueueIsEmpty();

#endif /* DISPATCHER_H_ */
