/*
 * dispatcher.c
 *
 *  Created on: Sep 2, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "dispatcher.h"
#include "processManager.h"

/**
 * Sets up the ready queue for use.
 */
void initReadyQueue() {
	readyQueueId = QCreate("readyQueue");
}

/**
 * The following code plays the role of the scheduler
 * in a uniprocessor operating system. It either starts
 * the next ready process with the greatest priority
 * or waits if there are no ready processes.
 */
void dispatch() {

	//pass time if there are no ready processes.
	while(readyQueueIsEmpty()) {
		CALL();
	}

	//if we reach here, there is a ready process.
	//TODO: get next process off of queue and start it.

}

/**
 * Returns whether the ready queue is empty in the
 * form of a boolean.
 */
int readyQueueIsEmpty() {
	return (int)QNextItemInfo(readyQueueId) == -1;
}

/**
 * Adds a process to the ready queue.
 * Parameters: process: the process to be added.
 */
void addToReadyQueue(Process process) {
	//TODO: add process based on priority.
	//QInsertOnTail(readyQueueId, &process);

	int i = 0;
	Process* current;

	do {

		current = QWalk(readyQueueId, 0);

		//if we haven't reached the end of the queue,
		//keep going until we find a process with a lower or equal priority.
		if((int)current != -1) {

			if(current->priority <= process.priority) {

				QInsert(readyQueueId,i,&process);
				break;

			} else {
				++i;
			}
		//if we reached the end, add this process to the tail.
		} else {
			QInsertOnTail(readyQueueId,&process);
		}

	} while((int)current != -1);

}
