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
int readFromDisk(long diskID, long sector, char* readBuffer);
void checkDisk(long diskID);
long getDiskStatus(long diskID);
void addToDiskQueue(long diskID);
Process* removeFromDiskQueue(long diskID);

int diskQueueId;
#endif /* DISKMANAGER_H_ */
