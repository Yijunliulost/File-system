
/* Driver 6*/

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "disk.h"
#include "diskmanager.h"
#include "partitionmanager.h"
#include "filesystem.h"
#include "client.h"

using namespace std;

/*
  This driver will test the getAttributes() and setAttributes()
  functions. You need to complete this driver according to the
  attributes you have implemented in your file system, before
  testing your program.
  
  
  Required tests:
  get and set on the fs1 on a file
    and on a file that doesn't exist
    and on a file in a directory in fs1
    and on a file that doesn't exist in a directory in fs1

 fs2, fs3
  on a file both get and set on both fs2 and fs3

  samples are provided below.  Use them and/or make up your own.


*/

int main()
{

  Disk *d = new Disk(300, 64, const_cast<char *>("DISK1"));
  DiskPartition *dp = new DiskPartition[3];

  dp[0].partitionName = 'A';
  dp[0].partitionSize = 100;
  dp[1].partitionName = 'B';
  dp[1].partitionSize = 75;
  dp[2].partitionName = 'C';
  dp[2].partitionSize = 105;

  DiskManager *dm = new DiskManager(d, 3, dp);
  FileSystem *fs1 = new FileSystem(dm, 'A');
  FileSystem *fs2 = new FileSystem(dm, 'B');
  FileSystem *fs3 = new FileSystem(dm, 'C');
  Client *c1 = new Client(fs1);
  Client *c2 = new Client(fs2);
  Client *c3 = new Client(fs3);
  Client *c4 = new Client(fs1);
  Client *c5 = new Client(fs2);



  int r;
  char c;
  int i;

//What every need to show your set and get Attributes functions work
  cout << "driver 6" << endl;
  r = c4->myFS->setAttributes(const_cast<char *>("/x"), 2, 'e', 100); 
  cout << "r from setting attributes of /x to e and 100 is " << r << (r==0?" correct": " fail") << endl;
  r = c1->myFS->getAttributes(const_cast<char *>("/x"), 2, c, i); 
  cout << "r from getting attributes of /x into d = " << c << " and 100 = " << i << " is " << r << (r==0?" correct": " fail") << endl;
  
  r = c1->myFS->setAttributes(const_cast<char *>("/e/f"), 4, 'c', 100);
  cout << "r from setting attributes of /e/f to c and 100 is " << r << (r==0?" correct": " fail") << endl;
  r = c4->myFS->setAttributes(const_cast<char *>("/e/b"), 4, 'f', 5);
  cout << "r from setting attributes of /e/b to f and 5 is " << r << (r==0?" correct": " fail") << endl;
  r = c1->myFS->getAttributes(const_cast<char *>("/e/f"), 4, c, i);
  cout << "r from getting attributes of /e/f to c = " << c << " and 100 = " << i << " is " << r << (r==0?" correct": " fail") << endl;
  r = c4->myFS->getAttributes(const_cast<char *>("/e/b"), 4, c, i);
  cout << "r from getting attributes of /e/f to f = " << c << " and 5 = " << i << " is " << r << (r==0?" correct": " fail") << endl;
  r = c1->myFS->getAttributes(const_cast<char *>("/p"), 2, c, i);  //should failed!
  cout << "r from getting attributes of /p into c and i is " << r << (r==-2?" correct, file doesn't exist": " fail") << endl;
  r = c4->myFS->setAttributes(const_cast<char *>("/p"), 2, 'c', 100);  //should failed!
  cout << "r from setting attributes of /p to c and 100 is " << r << (r==-2?" correct, file doesn't exist": " fail") << endl;
  r = c1->myFS->getAttributes(const_cast<char *>("/e/p"), 4, c, i);  //should failed!
  cout << "r from getting attributes of /p into c and i is " << r << (r == -2 ? " correct, file doesn't exist" : " fail") << endl;
  r = c4->myFS->setAttributes(const_cast<char *>("/e/p"), 4, 'f', 10);  //should failed!
  cout << "r from setting attributes of /p to f and 10 is " << r << (r == -2 ? " correct, file doesn't exist" : " fail") << endl;
  
  r = c2->myFS->setAttributes(const_cast<char *>("/f"), 2, 'w', 15);
  cout << "r from setting attributes of /f to w and 15 is " << r << (r==-3?" correct, w is out of bounds": " fail") << endl;
  r = c5->myFS->setAttributes(const_cast<char *>("/z"),2, 'c', 1000000000);
  cout << "r from setting attributes of /z to c and 1000000000 is " << r << (r==-3?" correct, 1000000000 is out of bounds": " fail")<< endl;
  r = c2->myFS->getAttributes(const_cast<char *>("f"), 1, c, i);
  cout << "r from getting attributes of f is " << r << (r==-1?" correct, invalid file": " fail") << endl;
  r = c5->myFS->getAttributes(const_cast<char *>("/z"), 2, c, i);
  cout << "r from getting attributes of /z to a = " << c << " and id = " << i << " is " << r << (r==0?" correct": " fail") << endl;

  r = c3->myFS->setAttributes(const_cast<char *>("/e/b/a"), 6, 'c', 354);
  cout << "r from setting attributes of /e/b/a to c and 354 is " << r << (r==0?" correct,": " fail")<< endl;
  r = c3->myFS->setAttributes(const_cast<char *>("/o/o/o/a/d"), 10, 'd', 999);
  cout << "r from setting attributes of /o/o/o/a/d to d and 999 is " << r << (r==-2?" correct": " fail")<< endl;
  r = c3->myFS->getAttributes(const_cast<char *>("/e/b/a"), 6, c, i);
  cout << "r from getting attributes of /e/b/a " << c << " = c and " << i << " = 354 is " << r << (r==0 ? " correct" : " fail") << endl;
  r = c3->myFS->getAttributes(const_cast<char *>("/o/o/o/a/d"), 10, c, i);
  cout << "r from getting attributes of o/o/o/a/d is " << r << (r==-2?" correct": " fail") << endl;

  cout << "end driver 6" << endl;
  return 0;
}
