//*********************************************
// sls.c - Set Leap Second
//
// Program modifies system time and triggers 
// execution of 'leap second' by kernel  
//
// 02.05.2013 ver. 1.0 R. Karbowski 
//  Initial version
//
// 25.11.2014 ver. 1.1 R. Karbowski 
//  Parameters parsing and diagnostic improvement
//
// 09.12.2014 ver. 1.2 R. Karbowski 
//  Parameters parsing and diagnostic improvement- cont.
//
// 11.12.2014 ver. 1.2.1 R. Karbowski 
//  Fix of bmidnight var. type issue
//
// 01.05.2015 ver. 1.3 R. Karbowski 
//  Clear time state before applying LS
//
//*********************************************

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/types.h>
#include <getopt.h>

// Default values
time_t bmidnight=600; // Number of seconds before midnight (leap second action)
int ls=STA_INS;       // Insert leap second
bool STATUS=false;    // Print LS status only

#define NRLENGTH 10   // Max number of digits for delay (incl. trailing null byte)

void pstatus();
void pparam(int, char **);
void usage();

int main(argc, argv)
int argc;
char *argv[];
{
struct timeval tv;
struct timex tx;

// Parse parameters
pparam(argc, argv);

// Print leap second flag status and exit
if(STATUS)
{
 pstatus();
 exit(0);
}

// Check if caller is root
if(getuid() != 0)
{
 printf("Only \'root\' may modify OS parameters\n");
 printf("Everyone can use \'-s\' option\n\n");
 usage();
 exit(1);
}

// Print parameters
printf("Entry parameters: delay %lds, LS %s\n\n", bmidnight, ls==STA_INS?"add":"delete");

// Current time
gettimeofday(&tv, NULL);

// Next leap second
tv.tv_sec +=86400 - tv.tv_sec % 86400;

// Set the time to be 'bmidnight' seconds before midnight
tv.tv_sec -=bmidnight;
settimeofday(&tv, NULL);

// Clear time state before applying LS
tx.modes=ADJ_STATUS;
tx.status=STA_PLL;
if(adjtimex(&tx) == -1)
{
 perror("adjtimex(1)");
 exit(1);
}

tx.modes=ADJ_STATUS;
tx.status=0x0;
if(adjtimex(&tx) == -1)
{
 perror("adjtimex(2)");
 exit(1);
}

// Set leap second flag 
tx.modes=ADJ_STATUS;
tx.status=ls;
if(adjtimex(&tx) == -1)
{
 perror("adjtimex(3)");
 exit(1);
}
printf("Finished: ");
pstatus();
exit(0);
}

//*************************
//  Print LS flag status
//*************************
void pstatus()
{
struct timex tx;

tx.modes=0;
if(adjtimex(&tx) == -1)
{
 perror("adjtimex(4)");
 exit(1);
}

printf("Kernel leap second flag: ");
if(tx.status & STA_INS)
 printf("add\n");
else 
 if(tx.status & STA_DEL)
  printf("delete\n"); 
 else
  printf("not set\n");
}

//*************************
//     Parse parameters
//*************************
void pparam(argc, argv)
int argc;
char *argv[];
{
int opt;
char strndelay[NRLENGTH];
bool RMOD=false;      // Request OS modification (if option -l or -d was used)
static struct option longopts[]= 
{
 {"delay",      required_argument, NULL, 'd'},
 {"leapsecond", required_argument, NULL, 'l'},
 {"status",     no_argument,       NULL, 's'},
 {"help",       no_argument,       NULL, 'h'},
 {0, 0, 0, 0}
};

while((opt=getopt_long(argc, argv, "d:l:sh", longopts, NULL)) != -1)
{
 switch(opt)
 {
 case 'd': // Set delay in seconds
  RMOD=true;
  sscanf(optarg, "%ld", &bmidnight);
  bmidnight=abs(bmidnight);
  snprintf(strndelay, NRLENGTH, "%ld", bmidnight);
  if(strcmp(optarg, strndelay) != 0)
  {
   memset(strndelay, '9', NRLENGTH-1);
   strndelay[NRLENGTH-1]=0;
   printf("Wrong argument for option \'-d\' or value out of range [0-%s]\n", strndelay);
   usage();
   exit(1);
  }
  break;
 
 case 'l': // Insert/delete leap second 
  RMOD=true;
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
 
 case 's': // LS flag status only
  STATUS=true;
  break;

 case 'h':
 default:
  usage();
  exit(1);

 }
}

if(STATUS && RMOD)
{
 printf("Options -l/-d and -s are mutually exclusive\n\n");
 usage();
 exit(1);
}
}

//*************************
//  Print usage message 
//*************************
void usage()
{
printf("Usage: sls [[[-d seconds] [-l add|del]] | [-s] | [-h]]\n");
printf("-d, --delay       delay before leap second accomplishment. Default 600 seconds\n");
printf("-l, --leapsecond  add: insert leap second, del: delete leap second. Default add\n");
printf("-s, --status      leap second flag status\n");
printf("-h, --help        display this help\n");
}


