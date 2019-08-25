/*
 * processManager.c
 *
 *  Created on: Aug 24, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 15

//function prototypes
void createInitialProcess(void);

//Struct for a process.
//pid: the process ID.
//priority: the process's current priority.
//name: the name of the process.
//startingAddress: the address the process begins execution at.
struct Process {
	long pid;
	long priority;
	char* name;
	void* startingAddress;
};

typedef struct Process Process;

Process* processes; //a dynamically allocated array that will store created processes.
int numProcesses = 0; //the current number of processes.

long currPidNumber = 1;
//the pid of a process is decided by a number sequence.
//we get the next number in the sequence by incrementing currPidNumber.

/**
 * This does all initial work needed at the start of running
 * the first process. This includes creating the first process
 * and allocating data for possible other processes.
 */
void pcbInit(void) {

	processes = (Process *)calloc(MAX_PROCESSES, sizeof(Process));
	createInitialProcess();

}

/**
 * This method facilitates the creation of the currently
 * running process. It stores the process in memory.
 */
void createInitialProcess(void) {

	if(numProcesses == MAX_PROCESSES) {
		//return error value here
	}

	//make the process, then save it.
	Process process;
	process.name = ""; //current process's name is ""
	process.priority = 0;
	process.pid =  currPidNumber;

	processes[numProcesses] = process;

	//update number of processes and the next pid in the sequence.
	++currPidNumber;
	++numProcesses;

}

/**
 * Returns the pid of a process of a given name.
 *
 * name: the name of the process to find the pid of.
 *
 * returns the pid of the process found or -1 if not the process wasn't found.
 */
long getPid(char* name) {

	//search for the process.
	for(int i = 0; i<numProcesses; i++) {

		if(strcmp(name, processes[i].name) == 0) {

			return processes[i].pid;

		}

	}

	//we didn't find the process. return error message.
	return -1;

}

