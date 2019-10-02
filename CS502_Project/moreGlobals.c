/*
 * moreGlobals.c
 *
 *  Created on: Sep 17, 2019
 *      Author: jean-philippe
 */


#include "moreGlobals.h"
#include "global.h"
#include "protos.h"
#include "limits.h"
#include "processManager.h"
#include "dispatcher.h"
#include <string.h>
#include <stdlib.h>
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define INTERRUPT_PRINTS_LIMIT 20

int interruptPrints = 0;
unsigned int timerQueueHead = UINT_MAX;

Message* findMessage();

/**
 * Performs a hardware interlock.
 * It attempts to lock, suspending
 * until this thread holds the lock.
 */
void lock() {
	INT32 lockResult;
	READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_LOCK,SUSPEND_UNTIL_LOCKED,&lockResult);
}

/**
 * Performs a hardware interlock.
 * It attempts to unlock, suspending
 * until this thread holds the lock.
 */
void unlock() {
	INT32 lockResult;
	READ_MODIFY(MEMORY_INTERLOCK_BASE,DO_UNLOCK,SUSPEND_UNTIL_LOCKED,&lockResult);
}

/**
 * Retrieves the hardware time and
 * then returns it.
 */
long getTimeOfDay() {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502ReturnValue;
	mmio.Field1 = 0;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;
	MEM_READ(Z502Clock, &mmio);
	return mmio.Field1;
}

/**
 * Creates the timer queue and stores its ID.
 */
void createTimerQueue() {
	timerQueueID = QCreate("timerQ");
}

/**
 * Adds a timer request to the timer queue. This
 * method ensures that the timer requests are in
 * ascending order based on sleepUntil fields.
 * Returns 0 if the timer needs
 * to be restarted, or -1 if there is no
 * need to restart the timer.
 */
int addToTimerQueue(TimerRequest* request) {

	long sleepUntil = request->sleepUntil;

	//get the head before we insert.
	TimerRequest* head = (TimerRequest*)QNextItemInfo(timerQueueID);

	lock();
	QInsert(timerQueueID, sleepUntil, request);
	unlock();


	if((int)head != -1) {

		//if our sleep time ends before
		//the head's...
		if(head->sleepUntil > sleepUntil) {

			return 0;

		} else return -1;

	} else return 0;
	//start the timer if nothing is on the queue.

}

/**
 * A method the facilitates printing from the
 * interrupt handler. This method will only
 * print the initial number of times for
 * interrupts.
 * Parameters:
 * msg: the string to be printed.
 */
void interruptPrint(char msg[]) {

	if(interruptPrints < INTERRUPT_PRINTS_LIMIT) {
		aprintf(msg);
		++interruptPrints;
	}

}

/**
 * Prepares the message queue for use.
 */
void initMessageQueue() {
	numMessages = 0;
	messageQueueID = QCreate("msgQueue");
}

/**
 * Prepares the message suspend queue for use.
 */
void initMsgSuspendQueue() {
	msgSuspendQueueID = QCreate("msgSuspend");
}

/**
 * Sends a message from the currently running process.
 * Parameters:
 * targetPID: the pid to send the message to.
 * messageBuffer: the message to be sent.
 * msgSendLength: the length of the send buffer.
 * Returns 0 if successful, or -1 if unsuccessful.
 */
long sendMessage(long targetPID, char* messageBuffer, long msgSendLength) {

	Process* target = getProcess(targetPID);
	Process* sender = currentProcess();

	//OS must restrict the number of messages present.
	if(numMessages >= 20) {
		return -1;
	}

	//pid must exist.
	if((int)target == -1 && targetPID != -1) {
		return -1;
	}

	if(msgSendLength >= 1000) {
		return -1;
	}

	//send length can't be less than buffer size
	if(msgSendLength < strlen(messageBuffer) + 1) {
		return -1;
	}

	Message* msg = (Message *)malloc(sizeof(Message));
	msg->from = sender->pid;
	msg->to = targetPID;
	msg->messageLength = msgSendLength;
	msg->messageContent = malloc(sizeof(char) * msgSendLength);
	strcpy(msg->messageContent, messageBuffer);

	lock();
	QInsertOnTail(messageQueueID, msg);
	unlock();

	//if this is a broadcast, wake up all processes.
	//otherwise, only wake up the one we sent it to.
	if(targetPID == -1) {

		lock();
		Process* suspendedProc = QNextItemInfo(msgSuspendQueueID);
		while((int)suspendedProc != -1) {

			QRemoveItem(msgSuspendQueueID, suspendedProc);
			addToReadyQueue(suspendedProc);

		}
		unlock();

	} else {

		lock();
		if((int)QItemExists(msgSuspendQueueID, target) != -1) {

			QRemoveItem(msgSuspendQueueID, target);
			addToReadyQueue(target);

		}
		unlock();

	}
	++numMessages;
	return 0;

}

/**
 * Searches for an returns the first
 * message in the message queue that this
 * process can receive.
 * Returns the address of the first process found
 * that can be received by this process, or -1
 * if no such process exists.
 */
Message* findMessage(long sourcePid) {

	Process* current = currentProcess();

	int i = 0;
	Message* msg = QWalk(messageQueueID, i);

	while((int)msg != -1) {

		//we can receive the message if:
		//--it is to us, from the target.
		//--the target is -1 and it is to us.
		//--the target is -1 and it is a broadcast.
		if((msg->to == current->pid && msg->from == sourcePid)
				|| (msg->to == current->pid && sourcePid == -1)
				|| (sourcePid == -1 && msg->to == -1)) {

			return msg;

		}

		++i;
		msg = QWalk(messageQueueID, i);
	}

	return (Message*)-1;

}

/**
 * Receives a message.
 * Parameters:
 * sourcePID: the process to receive from
 * receiveBuffer: the buffer to store the message in.
 * receiveLength: how many characters should be received by the receive buffer.
 * sendLength: how many characters were actually sent.
 * senderPid: the pid that was actually received from.
 * Returns 0 if successful or -1 if an error occurred.
 */
long receiveMessage(long sourcePID, char* receiveBuffer, long receiveLength, long* sendLength, long* senderPid) {

	Message* msg = findMessage(sourcePID);
	Process* target = getProcess(sourcePID);

	//pid must exist
	if((int)target == -1 && sourcePID != -1) {
		return -1;
	}

	if(receiveLength >= 1000) {
		return -1;
	}

	while((int)msg == -1) {

		//this should be on the msg suspend queue if we have no message.
		Process* current = currentProcess();
		lock();
		QInsertOnTail(msgSuspendQueueID, current);
		unlock();

		dispatch();
		msg = findMessage(sourcePID);

	}

	if(receiveLength < strlen(msg->messageContent) + 1) {

		return -1;

	}

	int bufferLength = strlen(msg->messageContent) + 1;
	receiveBuffer = malloc(bufferLength * sizeof(char));

	strcpy(receiveBuffer, msg->messageContent);
	*sendLength = bufferLength;
	*senderPid = msg->from;
	return 0;

}
