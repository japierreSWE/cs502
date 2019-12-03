/*
 * FrameHandler.c
 *
 *  Created on: Nov 23, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "moreGlobals.h"
#include "memoryManager.h"
#include "processManager.h"
#include "fileSystem.h"
#include "dispatcher.h"
#include "diskManager.h"


FrameData* frameTable; //frameTable[i] is the page number for the ith frame
int* swapBlockAvailability; //if swapBlockAvailability[i] == 1, that swap space block is available

//each process should have swap table.
//frame table should contain swap block, process that put it there, and page number.
//

/**
 * Sets up the memory manager by initializing
 * swap space and frame table.
 */
void initMemoryManager() {

	formattedDisk = -1;
	frameTable = calloc(NUMBER_PHYSICAL_PAGES, sizeof(FrameData));
	swapBlockAvailability = calloc(SWAP_SIZE, sizeof(int));

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {
		//frameTable[i] == -1 means the ith frame is free.
		frameTable[i].pageNumber = -1;
	}

	/*for(int i = 0; i<1024; i++) {
		swapTable[i] = -1;
	}*/

	for(int i = 0; i<SWAP_SIZE; i++) {
		swapBlockAvailability[i] = 1;
	}

	MPData = (MP_INPUT_DATA*)malloc(sizeof(MP_INPUT_DATA));
	memoryPrints = 0;
}

/**
 * Finds a free frame for a given page for this process.
 * Returns the frame number corresponding to the 1st
 * free frame found, or -1 if there are no free frames.
 * Also sets the frame found as used in frame table.
 * Parameters:
 * pageNumber: the page we're getting a frame for.
 * Returns the frame number that's been allocated, or -1
 * if no frame was found.
 */
int getFreeFrame(int pageNumber) {

	int freeFrame = -1;

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {

		if(frameTable[i].pageNumber == -1) {
			freeFrame = i;
			break;
		}

	}

	if(freeFrame != -1) {

		frameTable[freeFrame].pageNumber = pageNumber;
		frameTable[freeFrame].pid = currentProcess()->pid;

	}

	return freeFrame;

}

/**
 * Finds a victim frame for page replacement.
 * Does this by finding a frame number whose page has no referenced bit.
 * Returns the victim frame
 */
int getVictimFrame() {

	for(int i = 0; i < NUMBER_PHYSICAL_PAGES; i++) {

		int pageNumber = frameTable[i].pageNumber;
		Process* userOfFrame = getProcess(frameTable[i].pid);

		if((int)userOfFrame != -1) {

			UINT16* pageTable = userOfFrame->pageTable;

			if( (pageTable[pageNumber] & PTBL_REFERENCED_BIT) == 0) {

				return i;

			} else {
				pageTable[pageNumber] &= (~PTBL_REFERENCED_BIT);
			}

		} else {
			return i;
		}


	}

	//we didn't find any non-referenced pages.
	//return the 1st, then
	return 0;

}

/**
 * Code that handles page faults. Updates the page table,
 * does page replacement, etc.
 */
void handlePageFault(int pageNumber) {

	//TODO: put locks on getting frames.

	memLock();
	int freeFrame = getFreeFrame(pageNumber);
	memUnlock();
	UINT16* thisPageTable = (UINT16*)currentProcess()->pageTable;

	if(freeFrame == -1) {
		/*
		 * 1. Find a victim frame (a frame whose page has no reference bit)
		 * 2. If the page has been modified(check modify bit), write the victim frame
		 * to swap space. (find its swap space block and but it there)
		 * 3. Read desired page from swap space and put it into victim frame
		 * 4. Update page table and frame table
		 */

		memLock();
		int victimFrameNumber = getVictimFrame();
		memUnlock();

		FrameData victimFrame = frameTable[victimFrameNumber];
		Process* victimFrameUser = getProcess(victimFrame.pid);


		if((int)victimFrameUser != -1 &&
				(victimFrameUser->pageTable[victimFrame.pageNumber] & PTBL_MODIFIED_BIT) != 0) {

			writeToSwapSpace(&frameTable[victimFrameNumber]);

		}

		char* currentPageData = readFromSwapSpace(pageNumber);

		Z502WritePhysicalMemory(victimFrameNumber, currentPageData);

		//the current page should have the victim frame.
		thisPageTable[pageNumber] = victimFrameNumber | PTBL_VALID_BIT;

		//only do work on the process losing the
		//frame if it still exists.
		if((int)victimFrameUser != -1) {

			UINT16 pageOfVictimFrame = victimFrameUser->pageTable[victimFrame.pageNumber];

			//the page that was using the victim frame can't use it anymore.
			//not valid.
			victimFrameUser->pageTable[victimFrame.pageNumber] = pageOfVictimFrame & (~PTBL_VALID_BIT);

		}

		//the victim frame is being used by this page now.
		frameTable[victimFrameNumber].pageNumber = pageNumber;
		frameTable[victimFrameNumber].pid = currentProcess()->pid;


		//TODO: memory printing needed here

		addToReadyQueue(currentProcess());
		dispatch();
		return;
	}

	thisPageTable[pageNumber] = freeFrame;
	thisPageTable[pageNumber] = thisPageTable[pageNumber] | PTBL_VALID_BIT;

	frameTable[freeFrame].pid = currentProcess()->pid;
	frameTable[freeFrame].pageNumber = pageNumber;

	MPData->frames[freeFrame].InUse = 1;
	MPData->frames[freeFrame].LogicalPage = pageNumber;
	MPData->frames[freeFrame].Pid = currentProcess()->pid;

	memoryPrint();

	addToReadyQueue(currentProcess());
	dispatch();

}


/**
 * Finds a block in swap space for a given
 * frame to use, allocating one if necessary.
 * Parameters:
 * frameData: the frame to find a swap block for.
 * Returns the sector corresponding to the swap block.
 */
int getSwapSpaceBlockFromFrame(FrameData* frameData) {

	Process* process = getProcess(frameData->pid);
	int pageNumber = frameData->pageNumber;
	int* swapTable = process->swapTable;

	if(swapTable[pageNumber] != -1) {
		return swapTable[pageNumber];
	}

	int blockUsed = -1;

	for(int i = 0; i<SWAP_SIZE; i++) {
		if(swapBlockAvailability[i] == 1) {
			swapBlockAvailability[i] = 0;
			blockUsed = i;
			break;
		}
	}

	if(blockUsed == -1) {
		aprintf("Need more swap space.\n");
		exit(0);
	}

	swapTable[pageNumber] = SWAP_LOCATION + blockUsed;
	return swapTable[pageNumber];

}

/**
 * Finds a block in swap space for the current process's given
 * page to use, allocating one if necessary.
 * Parameters:
 * pageNumber: the page to find a swap block for.
 * Returns the sector corresponding to the swap block.
 */
int getSwapSpaceBlock(int pageNumber) {

	Process* process = currentProcess();
	int* swapTable = process->swapTable;

	if(swapTable[pageNumber] != -1) {
		return swapTable[pageNumber];
	}

	int blockUsed = -1;

	for(int i = 0; i<SWAP_SIZE; i++) {
		if(swapBlockAvailability[i] == 1) {
			swapBlockAvailability[i] = 0;
			blockUsed = i;
			break;
		}
	}

	if(blockUsed == -1) {
		aprintf("Need more swap space.\n");
		exit(0);
	}

	swapTable[pageNumber] = SWAP_LOCATION + blockUsed;
	return swapTable[pageNumber];

}
