/*
 * processManager.h
 *
 *  Created on: Aug 24, 2019
 *      Author: jean-philippe
 */

#ifndef PROCESSMANAGER_H_
#define PROCESSMANAGER_H_

void pcbInit(long address, long pageTable);
long getPid(char* name);

#endif /* PROCESSMANAGER_H_ */
