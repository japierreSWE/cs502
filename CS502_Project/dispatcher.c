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
#include "diskManager.h"

void schedulePrint();
int inReadyQueue(long pid);
int inSuspendQueue(long pid);

int numSchedulePrints = 0;

/**
 * Sets up the ready queue for use.
 */
void initReadyQueue() {
	readyQueueId = QCreate("readyQueue");
}

/**
 * Sets up the suspend queue for use.
 */
void initSuspendQueue() {
	suspendQueueId = QCreate("susQueue");
}

/**
 * The following code plays the role of the scheduler
 * in a uniprocessor operating system.
 * It either starts the next ready process
 * with the greatest priority or waits if there
 * are no ready processes.
 */
void dispatch() {

	//pass time if there are no ready processes.
	while(readyQueueIsEmpty()) {
		CALL();
	}

	schedulePrint();

	//if we reach here, there is a ready process.
	//get next process off of queue and start it.
	readyLock();
	Process* nextProcess = QRemoveHead(readyQueueId);
	readyUnlock();

	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502StartContext;
	mmio.Field1 = nextProcess->contextId;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	mmio.Field3 = 0;
	mmio.Field4 = 0;

	MEM_WRITE(Z502Context, &mmio);

	//if there was an error, stop.
	if(mmio.Field4 == ERR_BAD_PARAM) {
		aprintf("Error starting another process from dispatch.\n");
		exit(0);
	}

}

/**
 * Conducts a call to the scheduler printing
 * mechanism.
 * Notes: if we've come from a terminated process,
 * the currently running PID is shown as -1.
 */
void schedulePrint() {

	if(numSchedulePrints >= 50) {
		return;
	}

	SP_INPUT_DATA* spData = malloc(sizeof(SP_INPUT_DATA));
	strcpy(spData->TargetAction, "Dispatch");

	if((int)currentProcess() != -1) {
		spData->CurrentlyRunningPID = (INT16)(currentProcess()->pid);
	} else {
		//this means that the PID doesn't exist.
		//ie. the process was deleted.
		spData->CurrentlyRunningPID = -1;
	}

	spData->NumberOfDiskSuspendedProcesses = 0;
	spData->NumberOfMessageSuspendedProcesses = 0;
	spData->NumberOfProcSuspendedProcesses = 0;
	spData->NumberOfReadyProcesses = 0;
	spData->NumberOfRunningProcesses = 0;
	spData->NumberOfTerminatedProcesses = 0;
	spData->NumberOfTimerSuspendedProcesses = 0;

	//give all arrays default values
	for(int i = 0; i<SP_MAX_NUMBER_OF_PIDS; i++) {

		spData->DiskSuspendedProcessPIDs[i] = 0;
		spData->MessageSuspendedProcessPIDs[i] = 0;
		spData->ProcSuspendedProcessPIDs[i] = 0;
		spData->ReadyProcessPIDs[i] = 0;
		spData->RunningProcessPIDs[i] = 0;
		spData->TerminatedProcessPIDs[i] = 0;
		spData->TimerSuspendedProcessPIDs[i] = 0;

	}

	int i = 0;
	Process* currReady = (Process*)QWalk(readyQueueId,i);
	spData->NumberOfReadyProcesses = 0;

	//count the # of ready processes, store their pids.
	while((int)currReady != -1) {

		++spData->NumberOfReadyProcesses;
		spData->ReadyProcessPIDs[i] = (INT16)currReady->pid;

		++i;
		currReady = (Process*)QWalk(readyQueueId,i);

	}

	//count the # of timer suspended processes, store their pids.
	i = 0;
	TimerRequest* currTimer = (TimerRequest*)QWalk(timerQueueID,i);
	spData->NumberOfTimerSuspendedProcesses = 0;

	while((int)currTimer != -1) {

		++spData->NumberOfTimerSuspendedProcesses;
		spData->TimerSuspendedProcessPIDs[i] = (INT16)currTimer->process->pid;

		++i;
		currTimer = (TimerRequest*)QWalk(timerQueueID, i);

	}

	//count the # of suspended processes, store their pids.
	i = 0;
	Process* currSuspend = (Process*)QWalk(suspendQueueId, i);
	spData->NumberOfProcSuspendedProcesses = 0;

	while((int)currSuspend != -1) {

		++spData->NumberOfProcSuspendedProcesses;
		spData->ProcSuspendedProcessPIDs[i] = (INT16)currSuspend->pid;

		++i;
		currSuspend = (Process*)QWalk(suspendQueueId, i);

	}

	//count msg suspended process, store their pids.
	i = 0;
	Process* currMsg = (Process*)QWalk(msgSuspendQueueID, i);
	spData->NumberOfMessageSuspendedProcesses = 0;

	while((int)currMsg != -1) {

		++spData->NumberOfMessageSuspendedProcesses;
		spData->MessageSuspendedProcessPIDs[i] = (INT16)currMsg->pid;

		++i;
		currMsg = (Process*)QWalk(msgSuspendQueueID, i);

	}

	//count disk suspended processes, store their pids.
	i = 0;
	DiskRequest* currDisk = (DiskRequest*)QWalk(diskQueueId, i);
	spData->NumberOfDiskSuspendedProcesses = 0;

	while((int)currDisk != -1) {

		++spData->NumberOfDiskSuspendedProcesses;
		spData->DiskSuspendedProcessPIDs[i] = (INT16)currDisk->process->pid;

		++i;
		currDisk = (DiskRequest*)QWalk(diskQueueId, i);

	}

	//print using the schedule printer.
	CALL(SPPrintLine(spData));

	//prevent memory leak.
	free(spData);
	++numSchedulePrints;
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
	//QInsertOnTail(readyQueueId, &process);
	readyLock();
	QInsert(readyQueueId, process->priority, process);
	readyUnlock();
}

/**
 * Terminates a process by removing it from the OS's memory.
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
		readyLock();
		QRemoveItem(readyQueueId,current);
		readyUnlock();

		processLock();
		QRemoveItem(processQueueID,current);
		processUnlock();

		processLock();
		--numProcesses;
		processUnlock();

		//if there are no remaining processes, shut down.
		if(numProcesses == 0) {
			MEM_WRITE(Z502Halt, 0);
		}

		//we just terminated ourselves.
		//so, we don't return.
		//instead, we start another process.
		dispatch();
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

		processLock();
		int processResult = (int)QRemoveItem(processQueueID,process);
		processUnlock();

		if((int)processResult == -1) {

			return -1;

		} else {
			//we successfully found it. remove it from all queues.
			readyLock();
			QRemoveItem(readyQueueId,process);
			readyUnlock();

			timerLock();
			QRemoveItem(timerQueueID, process);
			timerUnlock();

			suspendLock();
			QRemoveItem(suspendQueueId, process);
			suspendUnlock();

			--numProcesses;
			return 0;
		}

	}

}

/**
 * Determines whether a process with
 * a given pid can be found in the
 * ready queue.
 * Parameters:
 * pid: the pid of the process to find in the ready queue.
 * Returns true if the process is in the ready queue and
 * false otherwise.
 */
int inReadyQueue(long pid) {

	readyLock();
	int i = 0;
	Process* curr = QWalk(readyQueueId, i);

	//iterate through the queue until
	//we find the pid.
	while((int)curr != -1) {

		if(curr->pid == pid) {
			readyUnlock();
			return 1;
		}

		++i;
		curr = QWalk(readyQueueId, i);

	}
	readyUnlock();

	//we didn't find it. return false.
	return 0;

}

/**
 * Determines whether a process with
 * a given pid is in the suspend queue.
 * Parameters:
 * pid: the pid of the process to find in the
 * suspend queue.
 * Returns true if the process is in the suspend queue
 * and false otherwise.
 */
int inSuspendQueue(long pid) {

	int i = 0;
	Process* curr = QWalk(suspendQueueId, i);

	//iterate through the queue until
	//we find the pid.
	while((int)curr != -1) {

		if(curr->pid == pid) {
			return 1;
		}

		++i;
		curr = QWalk(suspendQueueId, i);

	}

	//we didn't find it. return false.
	return 0;

}

/**
 * Suspends a process by removing the
 * process with the given pid from
 * the ready queue and placing it
 * on the suspend queue.
 * Parameters:
 * pid: the pid of the process to suspend.
 * Returns -1 if there is an error or 0 if successful.
 */
long suspendProcess(long pid) {

	//we can't suspend a process
	//not in the ready queue.
	if(!inReadyQueue(pid)) {
		return -1;
	}

	//we can't suspend a process
	//that's already suspended.
	if(inSuspendQueue(pid)) {
		return -1;
	}

	//we can't suspend ourselves.
	if(pid == currentProcess()->pid) {
		return -1;
	}

	readyLock();
	int i = 0;
	Process* curr = (Process*)QWalk(readyQueueId, i);

	//iterate until we find the pid we're looking for.
	while(curr->pid != pid) {
		++i;
		curr = (Process*)QWalk(readyQueueId, i);
	}
	readyUnlock();

	//remove from ready queue, then add to suspend queue.
	readyLock();
	QRemoveItem(readyQueueId,curr);
	readyUnlock();

	suspendLock();
	QInsertOnTail(suspendQueueId,curr);
	suspendUnlock();

	return 0;

}

/**
 * Resumes a process by removing
 * it from the suspend queue and
 * placing it on the ready queue.
 * Parameters:
 * pid: the pid of the process to resume.
 * Returns 0 if successful or -1 if there
 * is an error.
 */
long resumeProcess(long pid) {

	//cannot resume processes
	//that aren't suspended.
	if(!inSuspendQueue(pid)) {
		return -1;
	}

	//cannot resume process that's
	//currently ready.
	if(inReadyQueue(pid)) {
		return -1;
	}

	//cannot resume ourselves.
	if(pid == currentProcess()->pid) {
		return -1;
	}

	suspendLock();
	int i = 0;
	Process* curr = (Process*)QWalk(suspendQueueId,i);

	//iterate through suspend queue until we find the process.
	while(curr->pid != pid) {
		++i;
		curr = (Process*)QWalk(suspendQueueId,i);
	}
	suspendUnlock();

	//remove from suspend queue and add to ready queue.
	suspendLock();
	QRemoveItem(suspendQueueId, curr);
	suspendUnlock();

	addToReadyQueue(curr);

	return 0;

}
