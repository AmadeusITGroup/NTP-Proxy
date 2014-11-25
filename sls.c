//*********************************************
// sls.c - Set Leap Second
//
// Program modifies system time and triggers 
// execution of 'leap second' by kernel  
//
// 02.05.2013 R. Karbowski - Initial version
//
//*********************************************

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <unistd.h>
#include <string.h>

// Default values
time_t bmidnight=600; // Number of seconds before midnight (leap second action)
int ls=STA_INS;       // Insert leap second

void pparam();
void usage();

int main(argc, argv)
int argc;
char *argv[];
{
struct timeval tv;
struct timex tx;
int rc;

// Parse parameters
pparam(argc, argv);

// Current time
gettimeofday(&tv, NULL);

// Next leap second
tv.tv_sec += 86400 - tv.tv_sec % 86400;

// Set the time to be 'bmidnight' seconds before midnight
tv.tv_sec -= bmidnight;
settimeofday(&tv, NULL);

// Set leap second flag 
tx.modes = ADJ_STATUS;
tx.status = ls;
if((rc=adjtimex(&tx)) == -1)
{
 perror("adjtimex()");
 exit(1);
}
else
{
 printf("Program finished successfully:\n");
 switch(rc)
 {
 case TIME_OK:
  printf("clock synchronized\n");
  break;

 case TIME_INS:
  printf("insert leap second\n");
  break;

 case TIME_DEL:
  printf("delete leap second\n");
  break;

 case TIME_OOP:
  printf("leap second in progress\n");
  break;

 case TIME_WAIT:
  printf("leap second has occurred\n");
  break;

 case TIME_BAD:
  printf("clock not synchronized\n");
  break;

 default:
  printf("adjtimex(): unexpected return code value however not an error\n");
  break;
 }
}

exit(0);
}

//*************************
//     Parse parameters
//*************************
void pparam(argc, argv)
int argc;
char *argv[];
{
int opt;

while((opt=getopt(argc, argv, "d:l:h")) != -1)
{
 switch(opt)
 {
 case 'd': // Set delay in seconds
  bmidnight=atoi(optarg);
  break;
 
 case 'l': // Insert/delete leap second 
  if(strncmp(optarg, "add", 3) == 0)
   {
   ls=STA_INS;
   break;
   }

  if(strncmp(optarg, "del", 3) == 0)
   {
   ls=STA_DEL;
   break;
   }
  
  printf("Wrong argument for option \'-l\'\n");
  usage();
  exit(1);
  break;
 
 case 'h':
 default:
  usage();
  exit(1);

 }
}

}

//*************************
//  Print usage message 
//*************************
void usage()
{
printf("Usage: sls [-d seconds] [-l add|del] [-h]\n");
printf("-d\tdelay before leap second accomplishment. Default 600 seconds\n");
printf("-l\tadd: insert leap second, del: delete leap second. Default add\n");
printf("-h\thelp\n");
}


