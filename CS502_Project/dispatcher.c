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
#include "moreGlobals.h"

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
void addToReadyQueue(Process* process) {
	//TODO: add process based on priority.
	//QInsertOnTail(readyQueueId, &process);

	int i = 0;
	Process* current;

	do {

		current = QWalk(readyQueueId, 0);

		//if we haven't reached the end of the queue,
		//keep going until we find a process with a lower or equal priority.
		if((int)current != -1) {

			if(current->priority <= process->priority) {

				QInsert(readyQueueId,i,process);
				break;

			} else {
				++i;
			}
		//if we reached the end, add this process to the tail.
		} else {
			QInsertOnTail(readyQueueId,process);
		}

	} while((int)current != -1);

}

/**
 * Terminates a process by deleting it from the OS's memory.
 * Clears the process from all queues. OS shuts down if
 * all processes have been terminated.
 * Parameters:
 * pid: the pid of the process to terminate.
 * if equal to -1, terminates this process.
 * if equal to -2, terminates this process and all descendants.
 * Returns 0 if successful or -1 if there is an error.
 */
long terminateProcess(long pid) {

	if(pid == -1) {
		//shut down the current process.
		//remove it from ready queue and process queue.
		Process* current = currentProcess();
		QRemoveItem(readyQueueId,current);
		QRemoveItem(processQueueID,current);
		--numProcesses;

		//if there are no remaining processes, shut down.
		if(numProcesses == 0) {
			MEM_WRITE(Z502Halt, 0);
		}

		return 0;

	} else if(pid == -2) {
		//terminate the current process and all children.
		MEM_WRITE(Z502Halt, 0);
		return 0;
	} else {
		//terminate the process with the given pid.

		Process* process = getProcess(pid);

		//if the process doesn't exist, return error.
		if((int)process == -1) {
			return -1;
		}

		int processResult = (int)QRemoveItem(processQueueID,process);

		if((int)processResult == -1) {

			return -1;

		} else {
			//we successfully found it. remove it from all queues.
			QRemoveItem(readyQueueId,process);
			return 0;
		}

	}

}
