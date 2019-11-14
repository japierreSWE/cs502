/*
 * processManager.c
 *
 *  Created on: Aug 24, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "processManager.h"
#include "diskManager.h"
#include "dispatcher.h"
#include "moreGlobals.h"

void storeProcess(Process* process);

#define MAX_PROCESSES 15

Process* processes; //a dynamically allocated array that will store created processes.

long currPidNumber = 1;
//the pid of a process is decided by a number sequence.
//we get the next number in the sequence by incrementing currPidNumber.

/**
 * This does all initial work needed for starting
 * the first process. This includes creating the first process,
 * creating the timer queue, creating other queues,
 * and allocating data for
 * possible other processes.
 */
void pcbInit(long address, long pageTable) {

	interruptPrints = 0;
	numProcesses = 0;
	schedulePrintLimit = 50;
	processes = (Process *)calloc(MAX_PROCESSES, sizeof(Process));
	processQueueID = QCreate("processQ");
	createTimerQueue();

	if(timerQueueID == -1) {
		aprintf("Couldn't create timerQueue\n");
		exit(0);
	}

	//to ensure all output for this test.
	if(address == (long)test7) {
		schedulePrintLimit = 100;
	}

	initDiskManager();
	initReadyQueue();
	initSuspendQueue();
	initMessageQueue();
	initMsgSuspendQueue();
	getNumProcessors();

	//if this is multiprocessed,
	//make the dispatcher.
	if(numProcessors > 1) {

		//initialize
		MEMORY_MAPPED_IO mmio;
		mmio.Mode = Z502InitializeContext;
		mmio.Field1 = 0;
		mmio.Field2 = (long)startMultidispatcher;
		mmio.Field3 = (long)calloc(2, NUMBER_VIRTUAL_PAGES );
		MEM_WRITE(Z502Context, &mmio);

		//start it up in the background.
		mmio.Mode = Z502StartContext;
		mmio.Field2 = START_NEW_CONTEXT_ONLY;
		MEM_WRITE(Z502Context, &mmio);

	}

	createInitialProcess(address, pageTable);

}

/**
 * This method facilitates the creation of the first
 * running process. It stores the process in memory.
 * Parameters:
 * address: the address the process starts at.
 * pageTable: the process's page table.
 */
void createInitialProcess(long address, long pageTable) {

	if(numProcesses == MAX_PROCESSES) {
		//return error value here
	}

	//make the process, then save it.
	Process* process = (Process*)malloc(sizeof(Process));
	process->name = ""; //current process's name is ""
	process->priority = 10;
	process->pid =  currPidNumber;
	process->startingAddress = address;
	process->pageTable = pageTable;

	//start the process by initializing then starting context.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long) process->startingAddress;
	mmio.Field3 = (long) process->pageTable;

	MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence

	//check that this call was successful
	if(mmio.Field4 != ERR_SUCCESS) {
		aprintf("Could not initialize context.\n");
		exit(0);
	}

	process->contextId = mmio.Field1;
	process->currentDirectorySector = -1;

	storeProcess(process);

	mmio.Mode = Z502StartContext;
	// Field1 contains the value of the context returned in the last call
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	MEM_WRITE(Z502Context, &mmio);     // Start up the context

	//check that this call was successful
	if(mmio.Field4 != ERR_SUCCESS) {
		aprintf("Could not start context.\n");
		exit(0);
	}

}

/**
 * Returns the pid of a process of a given name.
 *
 * name: the name of the process to find the pid of.
 *
 * returns the pid of the process found or -1 if not the process wasn't found.
 */
long getPid(char* name) {

	//search for the process, and return its pid if found.
	/*for(int i = 0; i<numProcesses; i++) {

		if(strcmp(name, processes[i].name) == 0) {

			return processes[i].pid;

		}

	}*/

	//case for current process getPid.
	if(strcmp(name, "") == 0) {
		Process* current = currentProcess();
		return current->pid;
	}

	processLock();
	int i = 0;
	Process* proc = (Process *)QWalk(processQueueID,i);
	do {

		if(strcmp(name, proc->name) == 0) {
			processUnlock();
			return proc->pid;
		}

		++i;
		proc = (Process *)QWalk(processQueueID,i);

	} while((int)proc != -1);
	processUnlock();

	//we didn't find the process. return error message.
	return -1;

}

/**
 * Retrieves the address of a process with the given pid.
 * Parameters:
 * pid: the pid of the process to find.
 * Returns the address of the given process, or -1 if
 * the process with the given pid wasn't found.
 */
Process* getProcess(long pid) {

	processLock();
	int i = 0;
	Process* proc = (Process *)QWalk(processQueueID,i);

	//iterate through the queue until we find the process.
	do {

		if(pid == proc->pid) {
			processUnlock();
			return proc;
		}

		++i;
		proc = (Process *)QWalk(processQueueID,i);

	} while((int)proc != -1);
	processUnlock();

	//process wasn't found. return -1;
	return (Process*)-1;

}

/**
 * Starts the hardware timer and
 * adds the currently running
 * process to the timer queue.
 *
 * Parameters:
 * timeAmount- the amount of time units until the process receives a timer interrupt.
 */
void startTimer(long timeAmount) {

	//start making timer request.
	TimerRequest* request = (TimerRequest*)malloc(sizeof(TimerRequest));
	Process* curr = currentProcess();
	request->process = curr;
	request->sleepUntil = getTimeOfDay() + timeAmount;

	//place on timer queue.
	int result = addToTimerQueue(request);

	//start the timer - if we need to
	if(result == 0) {

		//start the hardware timer.
		MEMORY_MAPPED_IO mmio;
		mmio.Mode = Z502Start;
		mmio.Field1 = timeAmount;
		mmio.Field2 = 0;
		mmio.Field3 = 0;
		mmio.Field4 = 0;
		MEM_WRITE(Z502Timer, &mmio);

		//error handling for timer
		if(mmio.Field4 != ERR_SUCCESS) {
			aprintf("Error when starting timer, %ld\n", mmio.Field4);
			exit(0);
		}

	}

}

/**
 * Returns the process struct of the currently
 * running process. Returns -1 if not found.
 */
Process* currentProcess() {

	//first, we get the current context id.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = 0;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;

	MEM_WRITE(Z502Context, &mmio);
	long contextId = mmio.Field1;

	//find the process with this context by iterating through
	//process queue.
	processLock();
	int i =0;
	Process* proc;
	do {

		proc = (Process *)QWalk(processQueueID,i);

		if((int)proc == -1) break;

		if(proc->contextId == contextId) {
			processUnlock();
			return proc;
		}

		++i;

	} while((int)proc != -1);
	processUnlock();

	return (Process*)-1;

}

/**
 * Stores a process in the processes array and
 * updates the pid sequence and current
 * number of processes.
 */
void storeProcess(Process* process) {
	//processes[numProcesses] = process;

	//update number of processes and the next pid in the sequence.
	++currPidNumber;

	processLock();
	++numProcesses;
	QInsertOnTail(processQueueID,process);
	processUnlock();
}

/**
 * Creates a process and places it on the ready queue.
 * Parameters:
 * processName: the name of the process to be created.
 * startingAddress: the address that this process should start at.
 * initialPriority: the priority that this process starts with.
 * pid: the address pointing to this process's pid.
 * Returns 0 if creating the process was successful.
 * Returns -1 if an error occurs.
 */
long createProcess(char* processName, void* startingAddress, long initialPriority, long* pid) {

	//process can't have an illegal priority
	if(initialPriority < 0) {
		return -1;
	}

	//process can't have the same name as another process.
	if(getPid(processName) != -1) {
		return -1;
	}

	//we can't have more than the maximum amount of processes.
	if(numProcesses == MAX_PROCESSES) {
		return -1;
	}

	Process* process = (Process*)malloc(sizeof(Process));
	process->name = calloc(strlen(processName),sizeof(char));
	strcpy(process->name,processName);
	process->startingAddress = (long)startingAddress;
	process->priority = initialPriority;
	process->pid = currPidNumber;

	void *pageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES );
	process->pageTable = (long)pageTable;

	//start the process by initializing then starting context.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long) process->startingAddress;
	mmio.Field3 = process->pageTable;

	MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence

	//check that this call was successful
	if(mmio.Field4 != ERR_SUCCESS) {
		aprintf("Create Process ERROR: Could not initialize context.\n");
		exit(0);
	}

	process->currentDirectorySector = -1;
	process->contextId = mmio.Field1;

	storeProcess(process);

	addToReadyQueue(process);
	return process->pid;
}

/**
 * Changes the priority of a certain process
 * to a new priority.
 * Parameters:
 * pid: the pid of the process whose priority should be changed.
 * newPriority: the new value of its priority.
 * Returns -1 if an error occurs or 0 if successful.
 */
long changePriority(long pid, long newPriority) {

	//case for changing this process's
	//priority.
	if(pid == -1) {

		if(newPriority < 0) return -1;

		Process* current = currentProcess();
		current->priority = newPriority;
		return 0;
	}

	Process* process = getProcess(pid);

	//non-existent process or illegal priority.
	if((int)process == -1 || newPriority < 0) {
		return -1;
	}

	process->priority = newPriority;

	readyLock();
	//the process must go to a new position in the ready queue
	//since it has a new priority.
	if((int)QItemExists(readyQueueId,process) != -1) {


		QRemoveItem(readyQueueId,process);


		addToReadyQueue(process);

	}
	readyUnlock();

	return 0;
}

/**
 * Suspends the process currently running by idling the processor.
 */
void idle() {
	//write to Z502Idle to suspend the process.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502Action;
	mmio.Field1 = 0;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;
	MEM_WRITE(Z502Idle, &mmio);
}

