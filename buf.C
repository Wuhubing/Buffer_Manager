#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include "page.h"
#include "buf.h"

#define ASSERT(c)  { if (!(c)) { \
		       cerr << "At line " << __LINE__ << ":" << endl << "  "; \
                       cerr << "This condition should hold: " #c << endl; \
                       exit(1); \
		     } \
                   }

//----------------------------------------
// Constructor of the class BufMgr
//----------------------------------------

BufMgr::BufMgr(const int bufs)
{
    numBufs = bufs;

    bufTable = new BufDesc[bufs];
    memset(bufTable, 0, bufs * sizeof(BufDesc));
    for (int i = 0; i < bufs; i++) 
    {
        bufTable[i].frameNo = i;
        bufTable[i].valid = false;
    }

    bufPool = new Page[bufs];
    memset(bufPool, 0, bufs * sizeof(Page));

    int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
    hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

    clockHand = bufs - 1;
}


BufMgr::~BufMgr() {

    // flush out all unwritten pages
    for (int i = 0; i < numBufs; i++) 
    {
        BufDesc* tmpbuf = &bufTable[i];
        if (tmpbuf->valid == true && tmpbuf->dirty == true) {

#ifdef DEBUGBUF
            cout << "flushing page " << tmpbuf->pageNo
                 << " from frame " << i << endl;
#endif

            tmpbuf->file->writePage(tmpbuf->pageNo, &(bufPool[i]));
        }
    }

    delete [] bufTable;
    delete [] bufPool;
}


const Status BufMgr::allocBuf(int &frame)
{
    // Initialize a counter to track the number of buffer pool scans.
    int scanCounter = 0;
    Status operationStatus;

    // Continue scanning the buffer pool until a free frame is found or the buffer pool has been scanned twice.
    while (scanCounter < 2 * numBufs)
    {
        advanceClock(); // Move the clock hand to the next frame.
        BufDesc* currentFrameDesc = &bufTable[clockHand]; // Descriptor for the current frame.

        // If the frame is invalid, it can be used to allocate the new page.
        if (!currentFrameDesc->valid)
        {
            frame = clockHand;
            return OK;
        }

        // If the frame has been recently accessed, reset the refbit and continue scanning.
        if (currentFrameDesc->refbit)
        {
            currentFrameDesc->refbit = false;
            scanCounter++;
            continue;
        }

        // Skip frames that are currently pinned.
        if (currentFrameDesc->pinCnt > 0)
        {
            scanCounter++;
            continue;
        }

        // Write the dirty page to disk before using the frame.
        if (currentFrameDesc->dirty)
        {
            operationStatus = currentFrameDesc->file->writePage(currentFrameDesc->pageNo, &bufPool[clockHand]);
            if (operationStatus != OK) return UNIXERR;
            currentFrameDesc->dirty = false;
            bufStats.diskwrites++; // Update diskwrites statistics.
        }

        // Clear the frame for new usage and remove the corresponding entry from the hash table.
        frame = clockHand;
        hashTable->remove(currentFrameDesc->file, currentFrameDesc->pageNo);
        currentFrameDesc->Clear();
        return OK;
    }

    return BUFFEREXCEEDED; // All frames are pinned or no suitable frame found after two scans.
}


	
const Status BufMgr::readPage(File* file, const int pageNo, Page*& page)
{
    // Attempt to locate the page in the buffer pool.
    Status operationStatus=OK;
    int frameNumber = 0;
    operationStatus = hashTable->lookup(file, pageNo, frameNumber);
    
    // If the page is not already in memory, allocate a buffer frame and read the page into it.
    if (operationStatus != OK) {
        operationStatus = allocBuf(frameNumber);
        if (operationStatus != OK) return operationStatus;

        operationStatus = file->readPage(pageNo, &bufPool[frameNumber]);
        if (operationStatus != OK) return operationStatus;

        operationStatus = hashTable->insert(file, pageNo, frameNumber);
        if (operationStatus != OK) return operationStatus;

        bufTable[frameNumber].Set(file, pageNo);
        bufStats.diskreads++;
        bufStats.accesses++;
        page = &bufPool[frameNumber];
    } else {
        // If the page is already in memory, update its status and return a pointer to it.
        BufDesc* frameDesc = &bufTable[frameNumber];
        frameDesc->refbit = true;
        frameDesc->pinCnt++;
        bufStats.accesses++;
        page = &bufPool[frameNumber];
    }

    return OK; // The page is now accessible in memory.
}



const Status BufMgr::unPinPage(File* file, const int pageNo, const bool dirtyFlag)
{
    // Look for the page in the buffer pool.
    int frameIndex;
    Status operationStatus = hashTable->lookup(file, pageNo, frameIndex);

    // If the page is not found, indicate this status.
    if (operationStatus == HASHNOTFOUND) return HASHNOTFOUND;

    // Retrieve the buffer frame descriptor.
    BufDesc* frameDesc = &bufTable[frameIndex];

    // If the page is not pinned, return an error.
    if (frameDesc->pinCnt == 0) return PAGENOTPINNED;

    frameDesc->pinCnt--; // Decrement the pin count
    if (dirtyFlag) frameDesc->dirty = true;

    return OK; // The page has been successfully unpinned.
}


const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page) 
{
    // Allocate a new page in the file.
    Status operationStatus = file->allocatePage(pageNo);
    if (operationStatus != OK) return operationStatus;

    // Allocate a buffer frame for the new page.
    int bufferFrameNumber;
    operationStatus = allocBuf(bufferFrameNumber);
    if (operationStatus != OK) {
        file->disposePage(pageNo);
        return (operationStatus == BUFFEREXCEEDED) ? BUFFEREXCEEDED : UNIXERR;
    }

    // Get a pointer to the BufDesc for easier access
    BufDesc* frameDesc = &bufTable[bufferFrameNumber];
    // Insert the new page into the hash table and set up its frame descriptor.
    operationStatus = hashTable->insert(file, pageNo, bufferFrameNumber);
    if (operationStatus != OK) {
        // If the insert fails, clear the frame and rollback the page allocation
        frameDesc->Clear();
        file->disposePage(pageNo);
        return HASHTBLERROR;
    }

    // Prepare the buffer frame to be used by the caller.
    frameDesc->Set(file, pageNo);
    page = &bufPool[bufferFrameNumber];

    return OK; // The new page is now allocated and accessible in memory.
}



const Status BufMgr::disposePage(File* file, const int pageNo) 
{
    // see if it is in the buffer pool
    Status status = OK;
    int frameNo = 0;
    status = hashTable->lookup(file, pageNo, frameNo);
    if (status == OK)
    {
        // clear the page
        bufTable[frameNo].Clear();
    }
    status = hashTable->remove(file, pageNo);

    // deallocate it in the file
    return file->disposePage(pageNo);
}

const Status BufMgr::flushFile(const File* file) 
{
  Status status;

  for (int i = 0; i < numBufs; i++) {
    BufDesc* tmpbuf = &(bufTable[i]);
    if (tmpbuf->valid == true && tmpbuf->file == file) {

      if (tmpbuf->pinCnt > 0)
	  return PAGEPINNED;

      if (tmpbuf->dirty == true) {
#ifdef DEBUGBUF
	cout << "flushing page " << tmpbuf->pageNo
             << " from frame " << i << endl;
#endif
	if ((status = tmpbuf->file->writePage(tmpbuf->pageNo,
					      &(bufPool[i]))) != OK)
	  return status;

	tmpbuf->dirty = false;
      }

      hashTable->remove(file,tmpbuf->pageNo);

      tmpbuf->file = NULL;
      tmpbuf->pageNo = -1;
      tmpbuf->valid = false;
    }

    else if (tmpbuf->valid == false && tmpbuf->file == file)
      return BADBUFFER;
  }
  
  return OK;
}


void BufMgr::printSelf(void) 
{
    BufDesc* tmpbuf;
  
    cout << endl << "Print buffer...\n";
    for (int i=0; i<numBufs; i++) {
        tmpbuf = &(bufTable[i]);
        cout << i << "\t" << (char*)(&bufPool[i]) 
             << "\tpinCnt: " << tmpbuf->pinCnt;
    
        if (tmpbuf->valid == true)
            cout << "\tvalid\n";
        cout << endl;
    };
}


