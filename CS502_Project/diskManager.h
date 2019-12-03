/*
 * diskManager.h
 *
 *  Created on: Aug 29, 2019
 *      Author: jean-philippe
 */
//intended to contain disk management operations.

#ifndef DISKMANAGER_H_
#define DISKMANAGER_H_
#include "moreGlobals.h"

void initDiskManager();
void writeToDisk(long diskID, long sector, char* writeBuffer);
void readFromDisk(long diskID, long sector, char* readBuffer);
void checkDisk(long diskID);
long getDiskStatus(long diskID);
void addToDiskQueue(long diskID, int currentlyUsing);
Process* removeFromDiskQueue(long diskID, int ignoreCurrentlyUsing);
int areEqual(char* buf1, char* buf2);

int diskQueueId;
#endif /* DISKMANAGER_H_ */
