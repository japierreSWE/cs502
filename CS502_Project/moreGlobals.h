/*
 * moreGlobals.h
 *
 *  Created on: Sep 13, 2019
 *      Author: jean-philippe
 */

#ifndef MOREGLOBALS_H_
#define MOREGLOBALS_H_

//Struct for a process.
//pid: the process ID.
//priority: the process's current priority.
//name: the name of the process.
//startingAddress: the address the process begins execution at.
//pageTable: the process's page table.
//contextId: the process's contextId
struct Process {
	long pid;
	long priority;
	char* name;
	long startingAddress;
	long pageTable;
	long contextId;
};

typedef struct Process Process;

int timerQueueID;
int processQueueID;
int numProcesses; //the current number of processes.

#endif /* MOREGLOBALS_H_ */
