#include "disk.h"
#include "diskmanager.h"
#include "partitionmanager.h"
#include "bitvector.h"
#include <iostream>
#include <vector>
#include <stdio.h>
using namespace std;


PartitionManager::PartitionManager(DiskManager *dm, char partitionname, int partitionsize)
{
  myDM = dm;
  myPartitionName = partitionname;
  myPartitionSize = partitionsize;
  //we should declare a bitvector first to do the recording

  bitV =  new BitVector(myPartitionSize);
  
  char buffer[64];
  int checkBV = readDiskBlock(0, buffer);
  
  if(checkBV == 0)
  {
	  bitV->setBitVector((unsigned int*)buffer);
          
	  if(myDM->getFirstInit() || buffer[0] == '#' || bitV->testBit(0) == 0)
      {
		  //if empty, we declare the bitvector
  	  	for(int i = 0; i < myPartitionSize; i++)
			    bitV->resetBit(i);
      
      	bitV -> setBit(0);
        bitV -> setBit(1);
	    }
	  available = myPartitionSize;
	  for (int i = 0; i < myPartitionSize; i++)
		  if (bitV->testBit(i))
			  available--;
      
      bitV->getBitVector((unsigned int*)buffer);
      if(writeDiskBlock(0, buffer) < 0) {cout << "Disk write Error" << endl;}  //write buffer to patition block 0
      
  }
  else
  {
	  cout<< "Disk read error" << endl;
  }
}


//Here is the destructor
PartitionManager::~PartitionManager()
{
}

/*
 * return blocknum, -1 otherwise
 */
int PartitionManager::getFreeDiskBlock()
{
  char buffer1[64];
  //get bitvector from block 0
  if(readDiskBlock(0,buffer1) < 0) return -1;
  //set the buffer1 as the bitvector
  bitV -> setBitVector((unsigned int *) buffer1);
  /* write the code for allocating a partition block */
  for(int i = 0; i < myPartitionSize; i++)
	{
	 if(bitV -> testBit(i) == 0)
	 {
		 bitV -> setBit(i);
		 bitV -> getBitVector((unsigned int *)buffer1);
		 if(writeDiskBlock(0, buffer1) < 0)
			 return -1;

    	return i;
	}
	}
	return -1;
}

/*
 * return 0 for sucess, -1 otherwise
 */
int PartitionManager::returnDiskBlock(int blknum)
{
  
  char buffer2[64];
  //remove garbage information
    for(int i=0; i<64; i++){
    buffer2[i] = 'c';
    }
    //get bitvector from block 0
  if(readDiskBlock(0,buffer2) < 0) return -1;
    //set the buffer2 as the bitvector
  bitV -> setBitVector((unsigned int *) buffer2);

  if(bitV -> testBit(blknum) != 0)
  {
    // write blank buffer to disk
    bitV -> resetBit(blknum);
    bitV -> getBitVector((unsigned int*)buffer2);
    if(writeDiskBlock(0, buffer2) < 0) return -1;

    return 0;
  }
  
  return -1;
  
}


int PartitionManager::readDiskBlock(int blknum, char *blkdata)
{
  return myDM->readDiskBlock(myPartitionName, blknum, blkdata);
}

int PartitionManager::writeDiskBlock(int blknum, char *blkdata)
{
  return myDM->writeDiskBlock(myPartitionName, blknum, blkdata);
}

int PartitionManager::getBlockSize() 
{
  return myDM->getBlockSize();
}

void PartitionManager::print()
{
	char buffer[64];
	readDiskBlock(0, buffer);
	bitV->setBitVector((unsigned int*)buffer);
	cout << "Partition " << myPartitionName << ": { " << bitV->testBit(0);
	for (int i = 1; i < myPartitionSize; i++)
	{
		cout << ", " << bitV->testBit(i);
	}
	cout << " }" << endl;
}