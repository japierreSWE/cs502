/*
 * moreGlobals.h
 *
 *  Created on: Sep 13, 2019
 *      Author: jean-philippe
 */

#ifndef MOREGLOBALS_H_
#define MOREGLOBALS_H_

#define INTERRUPT_PRINTS_LIMIT 20

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

//struct for a timer request.
//process: the process requesting the sleep.
//sleepUntil: the hardware time that the process should sleep until.
struct TimerRequest {
	Process* process;
	long sleepUntil;
};

typedef struct TimerRequest TimerRequest;

int timerQueueID;
int processQueueID;
int numProcesses; //the current number of processes.

void lock();
void unlock();
long getTimeOfDay();
void createTimerQueue();
int addToTimerQueue(TimerRequest* request);
void interruptPrint(char msg[]);

#endif /* MOREGLOBALS_H_ */
