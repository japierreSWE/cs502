/*
 * processManager.h
 *
 *  Created on: Aug 24, 2019
 *      Author: jean-philippe
 */

#ifndef PROCESSMANAGER_H_
#define PROCESSMANAGER_H_

#include "moreGlobals.h"

void pcbInit(long address, long pageTable);
long getPid(char* name);
void startTimer(long timeAmount);
Process currentProcess();
void createInitialProcess(long address, long pageTable);
void idle();
long createProcess(char* processName, void* startingAddress, long initialPriority, long* pid);


int timerQueueID;
int processQueueID;


#endif /* PROCESSMANAGER_H_ */
