#include "disk.h"
#include "diskmanager.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
using namespace std;

DiskManager::DiskManager(Disk *d, int partcount, DiskPartition *dp)
{
  myDisk = d;
  partCount= partcount;
  int r = myDisk->initDisk();
  char buffer[64];

  /* If needed, initialize the disk to keep partition information */
  if(r == 1) // need to initialize new partititons
  {
	  firstInit = true;
	  for (int i = 0; i < partCount; i++)
	  {
		  if (i == 0)
			  dp[i].partitionStart = 1;
		  else
			  dp[i].partitionStart = dp[i - 1].partitionStart + dp[i - 1].partitionSize;
	  }
	  
	for(int i = 0; i < 64; i++) buffer[i] = '.'; // Fill buffer with .'s
	  
	for(int i = 0; i < partCount; i++)
	{
		buffer[16*i] = dp[i].partitionName; // partition names at 0, 16, 32, 48, 1 char
		insertInt(dp[i].partitionStart, 16*i + 3, buffer); // partition start at 1, 17, 33, 49, 4 chars
		insertInt(dp[i].partitionSize, 16*i + 9, buffer); // partition size at 5, 21, 37, 53, 4 chars
	}
	diskP = dp;
	myDisk->writeDiskBlock(0, buffer); // write partition info to disk in block 0
  }
  /* else  read back the partition information from the DISK1 */
  else if (r == 0) // need to read partition info
  {
	  firstInit = false;
	  partCount = 0;
	  myDisk->readDiskBlock(0, buffer);

	  for (int i = 0; i < 4; i++)
	  {
		  if (buffer[16 * i] != '.')
			  partCount++;
	  }
	  diskP = new DiskPartition[partCount];
	  for (int i = 0; i < partCount; i++)
	  {
		  diskP[i].partitionName = buffer[16 * i];
		  diskP[i].partitionStart = readInt(16 * i + 3, buffer);
		  diskP[i].partitionSize = readInt(16 * i + 9, buffer);
		  /*cout << "Partition " << diskP[i].partitionName << " starts at " << diskP[i].partitionStart <<
			  " and is " << diskP[i].partitionSize << " blocks." << endl; */
	  }
  }
}

/*
 *   returns: 
 *   0, if the block is successfully read;
 *  -1, if disk can't be opened; (same as disk)
 *  -2, if blknum is out of bounds; (same as disk)
 *  -3 if partition doesn't exist
 */
int DiskManager::readDiskBlock(char partitionname, int blknum, char *blkdata)
{
  int offset = 1; //the first block in the disk is the superblock, we will need 
                  //this offset of one to find the find the appropriate block to read
  int index = -1; //used to store index of partition that needs to be read

  for(int i=0; i<partCount; i++){  //loop for finding the partition
    if(diskP[i].partitionName == partitionname){
      index = i;
      break;
    }
  }
  if(index == -1){  //partition couldnt be found, return -3
    return -3;
  }

  int partitionSize = getPartitionSize(partitionname);
  if((blknum>partitionSize) || (blknum < 0)){
    return -2;  //blknum is out of bounds
  }

  //adding the previous amount of blocks in previous partitions
  //to give us an accurate spot to start reading with the readDiskBlock function for our disk
  for(int i = 0; i < index; i++){                 
    offset += diskP[i].partitionSize;
  }

  return(myDisk -> readDiskBlock(offset+blknum, blkdata));

}


/*
 *   returns: 
 *   0, if the block is successfully written;
 *  -1, if disk can't be opened; (same as disk)
 *  -2, if blknum is out of bounds;  (same as disk)
 *  -3 if partition doesn't exist
 */
// nearly identical to readDiskBlock, main diference is "writeDiskBlock" is called at then end.
int DiskManager::writeDiskBlock(char partitionname, int blknum, char *blkdata)
{
  int offset = 1;
  int index = -1;

  for(int i = 0; i < partCount; i++){
    if(diskP[i].partitionName == partitionname){
      index = i;
      break;
    }
  }
  if(index == -1){
    return -3;
  }
  int partitionSize = getPartitionSize(partitionname);
  if((blknum>partitionSize) || (blknum < 0)){
    return -2;
  }

  for(int i = 0; i<index; i++){
    offset = offset + diskP[i].partitionSize;
  }
  return (myDisk -> writeDiskBlock(offset+blknum, blkdata));
}

/*
 * return size of partition
 * -1 if partition doesn't exist.
 */
int DiskManager::getPartitionSize(char partitionname)
{

/* write the code for returning partition size */
// if we get the right index which have the right size of that partition

 int index = partCount;

	while( diskP[index].partitionName != partitionname )
	{
		if ( index < 0 )
		{
			return -1;
		}
    index = index - 1;
	}	
	return diskP[index].partitionSize;
}


// Adds a number to a char array, from 0000 to 9999.
void DiskManager::insertInt(int num, int loc, char* buffer)
{
	char numChar[4];
	for (int i = 0; i < 4; i++)
	{
		int newNum = num % 10;

		switch (newNum)
		{
		case 0: numChar[3 - i] = '0'; break;
		case 1: numChar[3 - i] = '1'; break;
		case 2: numChar[3 - i] = '2'; break;
		case 3: numChar[3 - i] = '3'; break;
		case 4: numChar[3 - i] = '4'; break;
		case 5: numChar[3 - i] = '5'; break;
		case 6: numChar[3 - i] = '6'; break;
		case 7: numChar[3 - i] = '7'; break;
		case 8: numChar[3 - i] = '8'; break;
		case 9: numChar[3 - i] = '9'; break;
		}

		num = num / 10;
	}

	for (int i = 0; i < 4 && loc + i < 64; i++)
	{
		buffer[i + loc] = numChar[i];
	}
}

// Returns the integer value of the 4 chars after and including loc
int DiskManager::readInt(int loc, char* buffer)
{
	char numChar[5];
	numChar[4] = '\0';
	for (int i = 0; i < 4 && loc + i < 64; i++)
	{
		numChar[i] = buffer[loc + i];
	}

	return atoi(numChar);
}

DiskPartition* DiskManager::getDiskPartition(char name)
{
	for(int i = 0; i < partCount; i++)
	{
		if(diskP[i].partitionName == name)
			return &diskP[i];
	}
	return 0;
}

bool DiskManager::getFirstInit()
{
	return firstInit;
}