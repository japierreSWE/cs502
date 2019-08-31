/*
 * diskManager.h
 *
 *  Created on: Aug 29, 2019
 *      Author: jean-philippe
 */

#ifndef DISKMANAGER_H_
#define DISKMANAGER_H_

void initDiskManager();
void writeToDisk(long diskID, long sector, char* writeBuffer);
void readFromDisk(long diskID, long sector, char* readBuffer);
void checkDisk(long diskID);

int* diskQueueIds;
#endif /* DISKMANAGER_H_ */
