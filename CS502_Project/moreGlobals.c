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
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE
#define INTERRUPT_PRINTS_LIMIT 20

int interruptPrints = 0;
unsigned int timerQueueHead = UINT_MAX;

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
