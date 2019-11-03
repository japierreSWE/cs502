/*
 * fileSystem.h
 *
 *  Created on: Oct 26, 2019
 *      Author: jean-philippe
 */

//intended to contain all API needed to do file
//system operations.

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

int formatDisk(int diskID);
int openDir(int diskID, char* directoryName);
void flushDiskContents();
int createDir(char* directoryName);
int createFile(char* directoryName);
int openFile(char* fileName);
int writeFile(int inode, int logicalBlock, char* writeBuffer);

int formattedDisk;

#endif /* FILESYSTEM_H_ */
