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

#define MAX_PROCESSES 15

Process* processes; //a dynamically allocated array that will store created processes.
int numProcesses = 0; //the current number of processes.

long currPidNumber = 1;
//the pid of a process is decided by a number sequence.
//we get the next number in the sequence by incrementing currPidNumber.

int timerQueueID;

/**
 * This does all initial work needed for starting
 * the first process. This includes creating the first process,
 * creating the timer queue, and allocating data for
 * possible other processes.
 */
void pcbInit(long address, long pageTable) {

	processes = (Process *)calloc(MAX_PROCESSES, sizeof(Process));
	timerQueueID = QCreate("timerQ");

	if(timerQueueID == -1) {
		aprintf("Couldn't create timerQueue\n");
		exit(0);
	}

	initDiskManager();
	initReadyQueue();
	createInitialProcess(address, pageTable);

}

/**
 * This method facilitates the creation of the currently
 * running process. It stores the process in memory.
 */
void createInitialProcess(long address, long pageTable) {

	if(numProcesses == MAX_PROCESSES) {
		//return error value here
	}

	//make the process, then save it.
	Process process;
	process.name = ""; //current process's name is ""
	process.priority = 0;
	process.pid =  currPidNumber;
	process.startingAddress = address;
	process.pageTable = pageTable;

	//start the process by initializing then starting context.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long) process.startingAddress;
	mmio.Field3 = (long) process.pageTable;

	MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence

	//check that this call was successful
	if(mmio.Field4 != ERR_SUCCESS) {
		aprintf("Could not initialize context.\n");
		exit(0);
	}

	process.contextId = mmio.Field1;

	processes[numProcesses] = process;

	//update number of processes and the next pid in the sequence.
	++currPidNumber;
	++numProcesses;

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
	for(int i = 0; i<numProcesses; i++) {

		if(strcmp(name, processes[i].name) == 0) {

			return processes[i].pid;

		}

	}

	//we didn't find the process. return error message.
	return -1;

}

/**
 * Starts the hardware timer for this process and
 * adds the process to the timer queue.
 *
 * Parameters:
 * timeAmount- the amount of time units until the process receives a timer interrupt.
 */
void startTimer(long timeAmount) {

	//place on timer queue.
	Process curr = currentProcess();
	QInsertOnTail(timerQueueID, &curr);

	//TODO:wait until timer is free.

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

/**
 * Returns the process struct of the currently
 * running process. Returns a process
 * with a contextId of -1 if none is found.
 */
Process currentProcess() {

	//first, we get
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502GetCurrentContext;
	mmio.Field1 = 0;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;

	MEM_WRITE(Z502Context, &mmio);
	long contextId = mmio.Field1;

	//find the process with this context.
	for(int i = 0; i<numProcesses; i++) {

		if(processes[i].contextId == contextId) {
			return processes[i];
		}

	}

	Process errProc;
	errProc.contextId = -1;
	return errProc;

}

/**
 * Suspends the process currently running.
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

