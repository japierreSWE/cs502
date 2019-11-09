/*
 * moreGlobals.h
 *
 *  Created on: Sep 13, 2019
 *      Author: jean-philippe
 */
//moreGlobals.h and moreGlobals.c are intended to
//contain code that will be needed in more than
//one place in the program that aren't specific to
//process management, disk management, or dispatching.


#ifndef MOREGLOBALS_H_
#define MOREGLOBALS_H_

#define INTERRUPT_PRINTS_LIMIT 10

//Struct for a process.
//pid: the process ID.
//priority: the process's current priority.
//name: the name of the process.
//startingAddress: the address the process begins execution at.
//pageTable: the process's page table.
//contextId: the process's contextId
//currentDirectorySector: the sector of the disk containing the current directory.
//currentDisk: the diskID containing the current directory
struct Process {
	long pid;
	long priority;
	char* name;
	long startingAddress;
	long pageTable;
	long contextId;
	int currentDirectorySector;
	long currentDisk;
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

//struct for an IPC message.
//messageContent: the contents of the message.
//messageLength: how long this message is.
//from: the process this message is coming from.
//to: the process this message is being sent to.
//broadcast: boolean whether this is a broadcast.
struct Message {
	char* messageContent;
	long messageLength;
	long from;
	long to;
};

typedef struct Message Message;

struct DiskRequest {
	long diskID;
	Process* process;
};

typedef struct DiskRequest DiskRequest;

int timerQueueID;
int processQueueID;
int numProcesses; //the current number of processes.
int messageQueueID; //queue containing messages.
int msgSuspendQueueID; //queue containing processes waiting for a message.

int numMessages; //number of messages in the queue.

int interruptPrints;

void timerLock();
void timerUnlock();
void diskLock();
void diskUnlock();
void msgLock();
void msgUnlock();
void suspendLock();
void suspendUnlock();
void processLock();
void processUnlock();
void msgSuspendLock();
void msgSuspendUnlock();
void diskContentsLock();
void diskContentsUnlock();
void readyLock();
void readyUnlock();
long getTimeOfDay();
void createTimerQueue();
int addToTimerQueue(TimerRequest* request);
void interruptPrint(char msg[]);
void initMessageQueue();
void initMsgSuspendQueue();
long sendMessage(long targetPID, char* messageBuffer, long msgSendLength);
long receiveMessage(long sourcePID, char* receiveBuffer, long receiveLength, long* sendLength, long* senderPid);

#endif /* MOREGLOBALS_H_ */
