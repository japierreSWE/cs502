/*
 * moreGlobals.c
 *
 *  Created on: Sep 17, 2019
 *      Author: jean-philippe
 */


#include "moreGlobals.h"
#include "global.h"
#include "protos.h"
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE

int interruptPrints = 0;

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

	TimerRequest* head = (TimerRequest*)QNextItemInfo(timerQueueID);

	//the queue is empty. add this request
	//and then start the timer.
	if((int)head == -1) {
		lock();
		QInsertOnTail(timerQueueID,request);
		unlock();
		return 0;
	} else {
		//the queue isn't empty.

		//if our delay ends after the head's delay,
		//we have to put this request in the right order.
		if(head->sleepUntil <= sleepUntil) {

			int i = 1;
			TimerRequest* current = (TimerRequest*)QWalk(timerQueueID, i);

			//iterate over the queue
			//until we find a request
			//whose delay is after this one's.
			//insert it in that position.
			//no need to start timer.

			while((int)current != -1) {

				if(request->sleepUntil <= current->sleepUntil) {

					lock();
					QInsert(timerQueueID,i,request);
					unlock();
					return -1;

				}

			}

			//if we iterated through the whole
			//queue, add this request to the
			//end. no need to start timer.
			lock();
			QInsertOnTail(timerQueueID,request);
			unlock();
			return -1;

		} else {
			//our delay ends before the head's delay.
			//add request to head and start timer.
			lock();
			QInsert(timerQueueID,0,request);
			unlock();
			return 0;

		}

	}

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
