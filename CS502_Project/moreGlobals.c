/*
 * moreGlobals.c
 *
 *  Created on: Sep 17, 2019
 *      Author: jean-philippe
 */


#include "global.h"
#include "protos.h"
#define                  DO_LOCK                     1
#define                  DO_UNLOCK                   0
#define                  SUSPEND_UNTIL_LOCKED        TRUE
#define                  DO_NOT_SUSPEND              FALSE

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
