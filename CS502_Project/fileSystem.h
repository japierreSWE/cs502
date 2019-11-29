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

#define SWAP_SIZE 4*0x80
#define SWAP_LOCATION 0x600

int formatDisk(int diskID);
int openDir(int diskID, char* directoryName);
void flushDiskContents();
int createDir(char* directoryName);
int createFile(char* directoryName);
int openFile(char* fileName);
int writeFile(long inode, int logicalBlock, char* writeBuffer);
int closeFile(long inode);
int readFile(long inode, int logicalBlock, char* readBuffer);
void dirContents();
void bufferCopy(unsigned char* src, unsigned char* dest);

int formattedDisk; //which disk is formatted?

#endif /* FILESYSTEM_H_ */
