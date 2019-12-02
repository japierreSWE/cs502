/*
 * FrameHandler.h
 *
 *  Created on: Nov 23, 2019
 *      Author: jean-philippe
 */

#ifndef FRAMEHANDLER_H_
#define FRAMEHANDLER_H_

int getFreeFrame();
void initMemoryManager();
void handlePageFault(int pageNumber);
int getSwapSpaceBlock(int pageNumber);
int getSwapSpaceBlockFromFrame(FrameData* frameData);

#endif /* FRAMEHANDLER_H_ */
