#include "disk.h"
#include "diskmanager.h"
#include "partitionmanager.h"
#include "filesystem.h"
#include <time.h>
#include <cstdlib>
#include <iostream>
using namespace std;



FileSystem::FileSystem(DiskManager *dm, char fileSystemName)
{
  myDM = dm;
  myfileSystemName = fileSystemName;
  DiskPartition* diskP = myDM->getDiskPartition(myfileSystemName);
  myfileSystemSize = diskP->partitionSize;
  myPM = new PartitionManager(myDM, myfileSystemName, myfileSystemSize);
  uniqueID = 1;
  uniqueLock = 1;
  uniqueFileDesc = 1;
}

int FileSystem::createFile(char *filename, int fnameLen)
{
	if (!validPath(filename, fnameLen))
		return -3;
	int directory;
	if (findFile(filename, fnameLen, directory) > 0) // file exists
		return -1;
	if (findDirectory(filename, fnameLen) > 0) // Directory with same name exists
		return -4;

	if (directory < 0)
		return -4; // Directory does not exist, cannot create a file here

	// Get disk block for file inode
	int fileInode = myPM->getFreeDiskBlock();
	if(fileInode < 0)
	{
		return -2; // couldn't get a free disk block
	}
	
	char buffer[64];
	for (int i = 0; i < 64; i++)
		buffer[i] = '#';
	// Write name, f, size and attributes to file i-node
	buffer[0] = filename[fnameLen - 1];
	buffer[1] = 'f';
	myDM->insertInt(0, 2, buffer); // Current file size, 0 blocks
	// Blocks will be in positions 6,10,14
	// Indirect block will be in position 18
	myDM->insertInt(uniqueID++, 22, buffer); // Unique ID attribute
	buffer[26] = 'a'; // Default type attribute

	if (myPM->writeDiskBlock(fileInode, buffer) != 0) // Failed writing
	{
		myPM->returnDiskBlock(fileInode);
		return -4;
	}

	bool search = true;
	while (search)
	{
		myPM->readDiskBlock(directory, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6*i] == '#')
			{
				buffer[6 * i] = filename[fnameLen - 1];
				buffer[6 * i + 1] = 'f';
				myDM->insertInt(fileInode, 6 * i + 2, buffer);
				myPM->writeDiskBlock(directory, buffer);
				search = false;
				break;
			}
		}
		if (search)
		{
			if (buffer[60] == '#') // need to extend directory
			{
				int newBlock = myPM->getFreeDiskBlock();
				if (newBlock < 0)
					return -2; // Not enough disk space

				myDM->insertInt(newBlock, 60, buffer);
				myPM->writeDiskBlock(directory, buffer);
				directory = newBlock;
			}
			else
				directory = myDM->readInt(60, buffer);
		}
	}
	return 0; // Successfully created file!
}

int FileSystem::createDirectory(char *dirname, int dnameLen)
{
	if (!validPath(dirname, dnameLen))
		return -3; // invalid path
	if (findDirectory(dirname, dnameLen) > 0) // directory already exists
		return -1;
	int dir;
	if (findFile(dirname, dnameLen, dir) > 0) // File with same name exists
		return -4;
	int dirBlock = myPM->getFreeDiskBlock();
	if (dirBlock < 0)
		return -2;// not enough disk space

	char buffer[64];
	for (int i = 0; i < 64; i++)
		buffer[i] = '#';
	
	char superDir[dnameLen - 2]; // find super directory
	for (int i = 0; i < dnameLen - 2; i++)
		superDir[i] = dirname[i];
	int superBlock = findDirectory(superDir, dnameLen - 2);
	if (superBlock < 0)
		return -4; // Super directory does not exist, cannot create a directory here
	
	bool search = true;
	while (search)
	{
		myPM->readDiskBlock(superBlock, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6 * i] == '#')
			{
				buffer[6 * i] = dirname[dnameLen - 1];
				buffer[6 * i + 1] = 'd';
				myDM->insertInt(dirBlock, 6 * i + 2, buffer);
				myPM->writeDiskBlock(superBlock, buffer);
				search = false;
				break;
			}
		}
		if (search)
		{
			if (buffer[60] == '#') // need to extend directory
			{
				int newBlock = myPM->getFreeDiskBlock();
				if (newBlock == -1)
					return -2; // Not enough disk space

				myDM->insertInt(newBlock, 60, buffer);
				myPM->writeDiskBlock(superBlock, buffer);
				superBlock = newBlock;
			}
			else
				superBlock = myDM->readInt(60, buffer);
		}
	}
	return 0;
}

int FileSystem::lockFile(char *filename, int fnameLen)
{
	if (!validPath(filename, fnameLen))
		return -4;
	
	int directory;
	int fileBlock = findFile(filename, fnameLen, directory);
	if (fileBlock < 0)
	{
		fileBlock = findDirectory(filename, fnameLen);
		if (fileBlock > 0)
			return -4; // file is actually a directory

		return -2; // File doesn't exist
	}
		

	int fileLock = isLocked(filename, fnameLen);
	if (fileLock > 0) // File is already locked
		return -1;

	if (isOpened(filename, fnameLen) != 0)
		return -3;

	Lock newLock;
	newLock.filename = filename;
	newLock.fnameLen = fnameLen;
	newLock.lockID = uniqueLock++;

	locks.push_back(newLock);
	return newLock.lockID;
}

int FileSystem::unlockFile(char *filename, int fnameLen, int lockId)
{
	if (!validPath(filename, fnameLen))
		return -2;

	int directory;
	int fileBlock = findFile(filename, fnameLen, directory);
	if (fileBlock < 0)
		return -2; // File doesn't exist

	int fileLock = isLocked(filename, fnameLen);
	if (fileLock == 0) // File is not locked
		return -2;

	if (lockId == fileLock)
	{
		vector<Lock>::iterator it = locks.begin();
		while (it != locks.end())
		{
			if (it->lockID == lockId)
				break;
		}
		locks.erase(it);
		return 0;
	}
	return -1;
}

int FileSystem::deleteFile(char *filename, int fnameLen)
{
	if (!validPath(filename, fnameLen)) {	//checking for a valid path
		return -3;
	}
	int dir;
	int fileINode = findFile(filename, fnameLen, dir);
	if (fileINode == -1) {	//if file doesnt exist
		if (findDirectory(filename, fnameLen) > 0)
			return -3; // file is actually a directory
		return -1;
	}
	if (fileINode < -1 || dir < 0) {
		return -3;
	}
	if (isLocked(filename, fnameLen) != 0 || isOpened(filename, fnameLen) != 0) {	//file is locked or opened
		return -2;
	}
	
	char buffer[64];
	char hashes[64];
	for (int i = 0; i<64; i++) {	//buffer will now be used to rewrite blocks;
		hashes[i] = '#';
	}
	myPM->readDiskBlock(fileINode, buffer);
	int eof = myDM->readInt(2, buffer); // Find the size of the file
	int fileBlock[19] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	if(buffer[6] != '#')
	{
		fileBlock[0] = myDM->readInt(6, buffer);
		myPM->writeDiskBlock(fileBlock[0], hashes);
		myPM->returnDiskBlock(fileBlock[0]);
	}
	if(buffer[10] != '#')
	{
		fileBlock[1] = myDM->readInt(10, buffer);
		myPM->writeDiskBlock(fileBlock[1], hashes);
		myPM->returnDiskBlock(fileBlock[1]);
	}
	if(buffer[14] != '#')
	{
		fileBlock[2] = myDM->readInt(14, buffer);
		myPM->writeDiskBlock(fileBlock[2], hashes);
		myPM->returnDiskBlock(fileBlock[2]);
	}

	int indBlock = -1;
	if(buffer[18] != '#')
		indBlock = myDM->readInt(18, buffer);
	
	if(indBlock != -1)
	{
		myPM->readDiskBlock(indBlock, buffer);
		for (int i = 0; i < 16; i++) // setup indirect file block
		{
			if (buffer[i * 4] != '#')
				fileBlock[i+3] = myDM->readInt(i * 4, buffer);
		}

		myPM->writeDiskBlock(indBlock, hashes);
		myPM->returnDiskBlock(indBlock);

		for (int i = 3; i<19; i++) {
			if (fileBlock[i] != -1) {
				myPM->writeDiskBlock(fileBlock[i], hashes);
				myPM->returnDiskBlock(fileBlock[i]);
			}
		}
	}
	
	// write over file inode block
	myPM->writeDiskBlock(fileINode, hashes);
	myPM->returnDiskBlock(fileINode);

	// Need to remove file info from directory
	bool search = true;
	int directory = dir;
	while (search)
	{
		myPM->readDiskBlock(directory, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6 * i] == filename[fnameLen - 1] && buffer[6*i + 1] == 'f')// found file in directory
			{
				for (int j = 0; j < 6; j++)
				{
					buffer[6 * i + j] = '#';
				}
				myPM->writeDiskBlock(directory, buffer);
				if(directory != dir)
					checkDirectory(dir, directory);
				search = false;
				break;
			}
		}
		if (search)
		{
			if (buffer[60] != '#')
			{
				directory = myDM->readInt(60, buffer);
			}
			else
				search = false;
		}
	}
	return 0;
}

int FileSystem::deleteDirectory(char *dirname, int dnameLen)
{
	if (!validPath(dirname, dnameLen))
		return -3; // invalid path

	int directory = findDirectory(dirname, dnameLen);
	if (directory < 0) // directory doesn't exists
		return -1;

	char buffer[64];
	for (int i = 0; i < 64; i++)
		buffer[i] = '#';

	// Find all blocks used by directory
	bool search = true;
	int previousBlock;
	int dirBlock = directory;
	while(search)
	{
		myPM->readDiskBlock(dirBlock, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6 * i] != '#')
				return -2; // directory is not empty
		}
		if (buffer[60] != '#')
		{
			previousBlock = dirBlock;
			dirBlock = myDM->readInt(60, buffer);
			for(int i = 0; i < 4; i++)
				buffer[60+i] = '#';
			myPM->writeDiskBlock(previousBlock, buffer);
			myPM->returnDiskBlock(previousBlock);
		}
		else
		{
			myPM->returnDiskBlock(dirBlock);
			search = false;
		}
			
	}
	
	char superDir[dnameLen - 2];
	for (int i = 0; i < dnameLen - 2; i++)
		superDir[i] = dirname[i];
	
	// get block of superdirectory
	int dir = findDirectory(superDir, dnameLen - 2);
	
	// Remove directory info from superdirectory
	search = true;
	int superBlock = dir;
	while (search)
	{
		myPM->readDiskBlock(superBlock, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6 * i] == dirname[dnameLen - 1] && buffer[6*i + 1] == 'd')
			{
				for (int j = 0; j < 6; j++)
					buffer[6 * i + j] = '#';
				myPM->writeDiskBlock(superBlock, buffer);
				if(superBlock != dir)
					checkDirectory(dir, superBlock);
				search = false;
				break;
			}
		}
		if (search)
		{
			if (buffer[60] != '#')
			{
				superBlock = myDM->readInt(60, buffer);
			}
			else
				search = false;
		}
	}
	return 0;
}

int FileSystem::openFile(char *filename, int fnameLen, char mode, int lockId)
{
	int dir;
	int file = findFile(filename, fnameLen, dir);
	if (file < 0)
		return -1; // File does not exist

	if (!(mode == 'r' || mode == 'w' || mode == 'm'))
		return -2; // Mode is invalid

	int checkLock = isLocked(filename, fnameLen);
	if (checkLock != 0)
	{
		if (lockId != checkLock)
			return -3; // File is locked but lock id does not match
	}
	else
	{
		if (lockId != -1)
			return -3; // File is not locked and lock id is not -1
	}
		
	OpenFile openFile;
	openFile.fileDesc = uniqueFileDesc++;
	openFile.filename = filename;
	openFile.fnameLen = fnameLen;
	openFile.mode = mode;
	openFile.rw = 0;

	openFiles.push_back(openFile);
	return openFile.fileDesc;
}

int FileSystem::closeFile(int fileDesc)
{
	if(openFiles.empty())
		return -1;
		
	vector<OpenFile>::iterator it = openFiles.begin();
	while(it != openFiles.end())
	{
		if(it->fileDesc == fileDesc)
		{
			openFiles.erase(it);
			return 0;
		}
		it++;
	}
	return -1;
}

int FileSystem::readFile(int fileDesc, char *data, int len)
{
	if (len < 0)
		return -2;
	
	vector<OpenFile>::iterator openfile = openFiles.begin();
	while (openfile != openFiles.end())
	{
		if (openfile->fileDesc == fileDesc)
			break;
		openfile++;
	}
	if (openfile == openFiles.end())
		return -1; // File descriptor invalid

	if (openfile->mode == 'w') // File was opened for write only
		return -3;

	int directory;
	int fileINode = findFile(openfile->filename, openfile->fnameLen, directory);
	if (fileINode < 0) // File does not exist, should close it
	{
		cout << "Can't find file inode :(" << endl;
		closeFile(fileDesc);
		return -1;
	}

	char buffer[64];
	myPM->readDiskBlock(fileINode, buffer);
	int eof = myDM->readInt(2, buffer); // Find the size of the file
	int fileBlock[19] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	if(buffer[6] != '#')
		fileBlock[0] = myDM->readInt(6, buffer);
	if(buffer[10] != '#')
		fileBlock[1] = myDM->readInt(10, buffer);
	if(buffer[14] != '#')
		fileBlock[2] = myDM->readInt(14, buffer);
	int indBlock = -1;
	if(buffer[18] != '#')
		indBlock = myDM->readInt(18, buffer);
	
	if(indBlock != -1)
	{
		myPM->readDiskBlock(indBlock, buffer);
		for (int i = 0; i < 16; i++) // setup indirect file block
		{
			if (buffer[i * 4] != '#')
				fileBlock[i+3] = myDM->readInt(i * 4, buffer);
		}
	}

	bool changeBlocks = true;
	int numRead = 0;
	while (openfile->rw != eof && numRead < len)
	{
		if (changeBlocks)
		{
			int nextBlock = fileBlock[openfile->rw / 64];
			if (nextBlock == -1) // If there are no more blocks to read
				return numRead;

			myPM->readDiskBlock(nextBlock, buffer);
			changeBlocks = false;
		}

		data[numRead] = buffer[openfile->rw % 64];
		++numRead;
		++openfile->rw;
		if (openfile->rw % 64 == 0) // Need to move to the next block
			changeBlocks = true;
	}
  return numRead;
}

int FileSystem::writeFile(int fileDesc, char *data, int len)
{
	if (len < 0)
		return -2;

	vector<OpenFile>::iterator openfile = openFiles.begin();
	while (openfile != openFiles.end())
	{
		if (openfile->fileDesc == fileDesc)
			break;
		openfile++;
	}
	if (openfile == openFiles.end())
		return -1; // File descriptor invalid

	if (openfile->mode == 'r') // File was opened for read only
		return -3;

	int directory;
	int fileINode = findFile(openfile->filename, openfile->fnameLen, directory);
	if (fileINode < 0) // File does not exist, should close it
	{
		closeFile(fileDesc);
		return -1;
	}

	char buffer[64];
	myPM->readDiskBlock(fileINode, buffer);
	int eof = myDM->readInt(2, buffer); // Find the size of the file
	if(openfile->rw  + len > 19*64)
		return -3; // Past max file size

	int fileBlock[19] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	if(buffer[6] != '#')
		fileBlock[0] = myDM->readInt(6, buffer);
	if(buffer[10] != '#')
		fileBlock[1] = myDM->readInt(10, buffer);
	if(buffer[14] != '#')
		fileBlock[2] = myDM->readInt(14, buffer);
	int indBlock = -1;
	if(buffer[14] != '#')
		indBlock = myDM->readInt(18, buffer);

	if(indBlock != -1)
	{
		if(myPM->readDiskBlock(indBlock, buffer) <0) return -4;
		for (int i = 0; i < 16; i++) // setup indirect file block
		{
			if (buffer[i * 4] != '#')
			{
				fileBlock[i + 3] = myDM->readInt(i * 4, buffer);
			}
		}
	}

	int numWritten = 0;
	int nextBlock; // = fileBlock[openfile->rw / 64];
	bool changeBlocks = true;
	while (numWritten < len && openfile->rw < 64 * 19) // less than max size
	{
		if(openfile->rw/64 > 2 && indBlock == -1)
		{
			indBlock = myPM->getFreeDiskBlock();
			if (indBlock < 0)
				return numWritten;
		}

		if (changeBlocks)
		{
			nextBlock = fileBlock[openfile->rw / 64];

			if (nextBlock == -1)
			{
				nextBlock = myPM->getFreeDiskBlock();
				if(nextBlock == -1)
					return -3; // No more room
				
				fileBlock[openfile->rw / 64] = nextBlock;
			}
			myPM->readDiskBlock(nextBlock, buffer);
			changeBlocks = false;
		}
		//Write byte from data to buffer
		buffer[openfile->rw % 64] = data[numWritten];
		
		if (openfile->rw == eof)
			++eof;
		++openfile->rw;
		++numWritten;

		if (openfile->rw % 64 == 0)
		{
			//Write buffer to disk
			if(myPM->writeDiskBlock(nextBlock, buffer) < 0)
				return -4;
			changeBlocks = true;
		}
	}
	// Make sure buffer gets written to disk
	myPM->writeDiskBlock(nextBlock, buffer);

	myPM->readDiskBlock(fileINode, buffer);
	myDM->insertInt(eof, 2, buffer);
	if(fileBlock[0] != -1)
		myDM->insertInt(fileBlock[0], 6, buffer);
	if(fileBlock[1] != -1)
		myDM->insertInt(fileBlock[1], 10, buffer);
	if(fileBlock[2] != -1)
		myDM->insertInt(fileBlock[2], 14, buffer);
	if(indBlock != -1)
		myDM->insertInt(indBlock, 18, buffer);
	myPM->writeDiskBlock(fileINode, buffer);

	// Write out all new block info
	for (int i = 0; i < 64; i++)
		buffer[i] = '#';
	if(indBlock != -1)
	{
		for (int i = 0; i < 16; i++)
		{
			if (fileBlock[i + 3] != -1)
			{
				myDM->insertInt(fileBlock[i+3], i * 4, buffer);
			}
		}
		myPM->writeDiskBlock(indBlock, buffer);
	}
  return numWritten;
}

int FileSystem::appendFile(int fileDesc, char *data, int len)
{
	if (len < 0)
		return -2;

	vector<OpenFile>::iterator openfile = openFiles.begin();
	while (openfile != openFiles.end())
	{
		if (openfile->fileDesc == fileDesc)
			break;
		openfile++;
	}
	if (openfile == openFiles.end())
		return -1; // File descriptor invalid

	if (openfile->mode == 'r') // File was opened for read only
		return -3;

	int directory;
	int fileINode = findFile(openfile->filename, openfile->fnameLen, directory);
	if (fileINode <= 0) // File does not exist, should close it
	{
		closeFile(fileDesc);
		return -1;
	}

	char buffer[64];
	myPM->readDiskBlock(fileINode, buffer);
	int eof = myDM->readInt(2, buffer); // Find the size of the file
	openfile->rw = eof;
	if(eof + len > 19*64)
		return -3; // Past max file size
	return writeFile(fileDesc, data, len);
}

int FileSystem::seekFile(int fileDesc, int offset, int flag)
{
	vector<OpenFile>::iterator openfile = openFiles.begin();
	while (openfile != openFiles.end())
	{
		if (openfile->fileDesc == fileDesc)
			break;
		openfile++;
	}
	if (openfile == openFiles.end())
		return -1; // File descriptor invalid

	char buffer[64];
	int dir;
	int fileINode = findFile(openfile->filename, openfile->fnameLen, dir);
	myPM->readDiskBlock(fileINode, buffer);
	int eof = myDM->readInt(2, buffer); // Find the size of the file

	if(flag == 0)
	{
		if(openfile->rw + offset < 0 || openfile->rw + offset > eof)
			return -2;
		else
		{
			openfile->rw = openfile->rw + offset;
			return 0;
		}
	}
	else
	{
		if(offset < 0 || offset > eof)
			return -1;
		else
		{
			openfile->rw = offset;
			return 0;
		}
	}
	return -1;
}

int FileSystem::renameFile(char *filename1, int fnameLen1, char *filename2, int fnameLen2)
{ 
	
   char buffer[64];
//first, we need to check whether the filename1 and filename2 is valid
   if (!validPath(filename1, fnameLen1)){return -1;}
   if (!validPath(filename2, fnameLen2)){return -1;}
//we also need to check whether the file exist 
   bool file = true;
   int directory1;
   int directory2;
   int fileINode = findFile(filename1, fnameLen1, directory1);
   if (fileINode < 0) // File doesn't exist
   {
	   file = false;
	   fileINode = findDirectory(filename1, fnameLen1);
	   char superDir[fnameLen1 - 2];
	   for (int i = 0; i < fnameLen1 - 2; i++)
		   superDir[i] = filename1[i];
	   directory1 = findDirectory(superDir, fnameLen1 - 2);
	   if(fileINode < 0) // doesn't exist as a directory either
			return -2;
   }
   if (findFile(filename2, fnameLen2, directory2) > 0){return -3;}
   if (findDirectory(filename2, fnameLen2) > 0) { return -3; }
//we also need to check whether the file is locked or opened, and these should be revised according to my teammates
   if(isLocked(filename1, fnameLen1)!= 0){return -4;}
   if(isOpened(filename1, fnameLen1)!= 0){return -4;}
//then we get the name and change the name
  char fname1 = filename1[fnameLen1 - 1];
  char fname2 = filename2[fnameLen2 - 1];
  if (file)
  {
	  myPM->readDiskBlock(fileINode, buffer);
	  buffer[0] = fname2;
	  myPM->writeDiskBlock(fileINode, buffer);
  }
//TODO: Write new file name to directory
  bool search = true;
  while (search)
  {
	  myPM->readDiskBlock(directory1, buffer);
	  for (int i = 0; i < 10; i++)
	  {
		  if (buffer[6 * i] == fname1)// found file in directory
		  {
			  buffer[6 * i] = fname2;
			  myPM->writeDiskBlock(directory1, buffer);
			  search = false;
			  break;
		  }
	  }
	  if (buffer[60] != '#')
	  {
		  directory1 = myDM->readInt(60, buffer);
	  }
	  else
		  search = false;
  }

  // Cycle through directory, find old file name, replace with new file name
  return 0;
}

int FileSystem::getAttributes(char *filename, int fnameLen, char &attr1, int &attr2)
{
   // return -1, if the filename not valid
   // return -2 if file does not exist
   char buffer[64];
//first, we need to check whether the filename1 and filename2 is valid
  if (!validPath(filename, fnameLen)){return -1;}
  int directory;
  int fileInode = findFile(filename, fnameLen, directory);
  if (fileInode < 0){return -2;}
  myPM -> readDiskBlock(fileInode, buffer); 
  attr1 = buffer[26];
  attr2 = myDM -> readInt(22, buffer);
  return 0;
}

int FileSystem::setAttributes(char *filename, int fnameLen, char add1, int add2  )
{
   // return -1, if the filename not valid
   // return -2 if file does not exist
   // return -3 if the attributes are out of bounds

   if(add1 < 'a' || add1 > 'f')
   	return -3;
  if(add2 < 1 || add2 > 9999)
	return -3;
  char buffer[64];
  int k = 0;
  //first, we need to check whether the filename1 and filename2 is valid
  if (!validPath(filename, fnameLen)){return -1;}
  int directory;
  int fileInode = findFile(filename, fnameLen, directory);
  if (fileInode < 0){return -2;}
  myPM -> readDiskBlock(fileInode, buffer);
  buffer[26] = add1;
  myDM -> insertInt(add2, 22, buffer);
  myPM -> writeDiskBlock(fileInode, buffer);
  return 0;  // setAttribute successfully 
}

// Returns true if the path is valid, false otherwise
bool FileSystem::validPath(char * filename, int fnameLen)
{
	if (fnameLen % 2 != 0)
		return false;

	for (int i = 0; i < fnameLen; i++)
	{
		if (i % 2 == 0)
		{
			if (filename[i] != '/') // Invalid name: Missing /
				return false;
		}
		else
		{
			if (!((filename[i] >= 'a' && filename[i] <= 'z') || (filename[i] >= 'A' && filename[i] <= 'Z'))) // Invalid name: not a-z or A-Z
				return false;
		}
	}

	return true;
}

// Returns block of file or directory if exists, -2 if invalid filename, -1 if it doesn't exist
int FileSystem::searchBlock(int block, char * filename, int fnameLen, char type)
{
	if (fnameLen != 2)
		return -2;
	if (!validPath(filename, fnameLen))
		return -2;

	char buffer[64];
	int nextBlock = block;
	bool searching = true;

	while (searching)
	{
		myPM->readDiskBlock(nextBlock, buffer);
		for (int i = 0; i < 10; i++)
		{
			if (buffer[6 * i] == filename[1] && buffer[6 * i + 1] == type) // found it!
				return myDM->readInt(6 * i + 2, buffer); // Return block of file/directory
		}
		if (buffer[60] == ' ' || buffer[60] == '#') // no more blocks to check
			searching = false;
		else
			nextBlock = myDM->readInt(60, buffer); // search the next extended block
	}
	return -1;
}

// Returns the directory i-node, -2 for invalid file, -1 if directory does not exist
int FileSystem::findDirectory(char * filename, int fnameLen)
{
	if (!validPath(filename, fnameLen))
		return -2;

	int next = 1; // Start at root directory
	char buffer[64];
	for (int i = 0; i < fnameLen / 2; i++)
	{
		char dir[2];
		dir[0] = filename[2 * i]; // search for sebsequent directories
		dir[1] = filename[2 * i + 1];
		
		next = searchBlock(next, dir, 2, 'd');
		if (next == -1 || next == -2)
			return -1;
	}
	return next;
}

// Returns the file i-node, -3 if the directory does not exist, -2 for invalid file, -1 if file does not exist
int FileSystem::findFile(char* filename, int fnameLen, int & directory)
{
	if (!validPath(filename, fnameLen))
		return -2;

	char dir[fnameLen - 2];
	for (int i = 0; i < fnameLen - 2; i++)
	{
		dir[i] = filename[i];
	}
	

	directory = findDirectory(dir, fnameLen - 2);
	if (directory == -2 || directory == -1)
	{
		return -3;}

	char file[2];
	file[0] = filename[fnameLen - 2];
	file[1] = filename[fnameLen - 1];
	
	int block = searchBlock(directory, file, 2, 'f');
	
	return block;
}

// Returns 0 if the file is not locked, otherwise returns the lockID
int FileSystem::isLocked(char* filename, int fnameLen)
{
	vector<Lock>::iterator it = locks.begin();
	while (it != locks.end())
	{
		bool match = true;
		if (it->fnameLen == fnameLen)
		{
			for (int i = 0; i < it->fnameLen; i++)
			{
				if (it->filename[i] != filename[i])
				{
					match = false;
					break;
				}
			}
			if (match)
				return it->lockID;
		}
		it++;
	}
	return 0;
}

// Returns 1 if the file is locked, 0 if not
int FileSystem::isOpened(char* filename, int fnameLen)
{
	vector<OpenFile>::iterator it = openFiles.begin();
	while (it != openFiles.end())
	{
		bool match = true;
		if (it->fnameLen == fnameLen)
		{
			for (int i = 0; i < it->fnameLen; i++)
			{
				if (it->filename[i] != filename[i])
				{
					match = false;
					break;
				}
			}
			if (match)
				return 1;
		}
		it++;
	}
	return 0;
}

void FileSystem::checkDirectory(int superDir, int dirBlock)
{
	bool empty = true;
	char buffer[64];
	myPM->readDiskBlock(dirBlock, buffer);
	for (int i = 0; i <= 10; i++)
	{
		if (buffer[6 * i] != '#')
		{
			empty = false;
			break;
		}
	}

	if (empty)
	{
		myPM->returnDiskBlock(dirBlock);
		bool search = true;
		while (search)
		{
			myPM->readDiskBlock(superDir, buffer);
			if (buffer[60] != '#')
			{
				if (myDM->readInt(60, buffer) == dirBlock)
				{
					for (int j = 0; j < 4; j++)
						buffer[60 + j] = '#';
					myPM->writeDiskBlock(superDir, buffer);
					search = false;
					break;
				}
			}
			else
				search = false;
		}
	}
}