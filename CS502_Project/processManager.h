/*
 * processManager.h
 *
 *  Created on: Aug 24, 2019
 *      Author: jean-philippe
 */

#ifndef PROCESSMANAGER_H_
#define PROCESSMANAGER_H_

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

void pcbInit(long address, long pageTable);
long getPid(char* name);
void startTimer(long timeAmount);
Process currentProcess();
void createInitialProcess(long address, long pageTable);
void idle();

int timerQueueID;

#endif /* PROCESSMANAGER_H_ */
