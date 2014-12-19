//*********************************************
// ntpproxy.c - NTP Proxy
//
// Program runs as proxy between NTP time source
// and another NTP client which in turn acts as  
// a server for other NTP clients.
// NTP Proxy modifies time stamps and 
// sets leap-indicator flag on the flow
// from NTP time source to client.
//
// 03.05.2013 ver. 1.0 R. Karbowski  
//  Initial version
//
// 20.01.2014 ver. 1.1 R. Karbowski  
//  Unbuffer stdout for logging
//
// 24.01.2014 ver. 1.2 R. Karbowski 
//  - print() with timestamp instead of printf()  
//  - logging facility displays client/server IP@-es 
//  - dump NTP packet in hex mode  
//  - allow only mode 03 client's queries
//
// 10.12.2014 ver. 1.3 R. Karbowski 
// - hex dump implemented in internal function
// - parameters parsing improvement
//
//*********************************************

#include <time.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <libnet.h>
#include <stdbool.h>
#include <sys/types.h>
#include <getopt.h>

#define JAN_1970  2208988800U  // 1970 - 1900 in seconds - shifts system time to NTP one
#define NRLENGTH 10     // Max number of digits for delay (incl. trailing null byte)

// Default values
time_t bmidnight=600;   // Number of seconds before midnight (leap second action)
int ls=1;               // 1: insert, -1: deduct leap second
bool verbose=false;     // Print additional information
char ntpserverip[25];   // IP of NTP source time server

void printNtp();
void pparam();
void usage();
void print(const char *format,...);
void hexdump(char buf[], int len);

// Number of days per month
int dpm[]={31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};


int main(argc, argv)
int argc;
char *argv[];
{
unsigned char buf[1024], mode;

int csd; // Client socket descriptor
int ssd; // Server socket descriptor
int i, srin_tmp_len, year, days;
bool WasLsApplied=false; // Divide time befor leap second and after
bool leapyear=false; 
time_t now, t2midnight, timeoffset, lswindow;
long len, n;
struct sockaddr_in srin, srin_tmp, clin;
struct libnet_ntp_hdr ntp_hdr;
struct tm *ts;

// Unbuffer stdout
setbuf(stdout, NULL);

// Parse parameters
pparam(argc, argv);

// Check if caller is root
if(getuid() != 0)
{
 printf("Only \'root\' may launch this program\n\n");
 usage();
 exit(1);
}

// Print parameters
printf("Entry parameters:\n");
printf("- source time IP: %s,\n", ntpserverip);
printf("- delay:          %lds,\n", bmidnight);
printf("- leapsecond:     %s,\n", ls==1?"add":"delete");
printf("- verbose:        %s\n\n", verbose?"yes":"no");

// Calculate time offset to be added to current
// time to reach end of Jun or Dec 23:50

now=time(NULL); 

// Number of seconds to midnight
t2midnight=((now/86400)*86400 + 86400) - now;

ts=gmtime(&now);
year=1900+ts->tm_year;

// Is it leap year?
if((year%4 == 0) && ((year%100 != 0) || (year%400 == 0)))
 leapyear=true;

// Number of days to the next leap second window (end of Jun or Dec)
days=0;
if(ts->tm_mon < 6)
{
 for(i=ts->tm_mon; i<6; i++)
  if(dpm[i] == 1 && leapyear)
   days+=29;
  else
   days+=dpm[i];
}
else
 for(i=ts->tm_mon; i<12; i++)
  days+=dpm[i];

timeoffset=(days - ts->tm_mday)*86400 + t2midnight - bmidnight; 

// Time (in sec and NTP format) when leap second will be applied
lswindow=now + timeoffset + bmidnight + JAN_1970;

if(verbose)
 print("days=%d, tm_mday=%d, t2midnight=%lld, now=%d, timeoffset=%d, lswindow=%lld\n", days, ts->tm_mday, t2midnight, now, timeoffset, lswindow);

now+=timeoffset;
ts=gmtime(&now);

if(verbose)
 print("now + timeoffset=%ld\n", now);

if(verbose)
 print("%02d:%02d:%02d Day=%d, Month=%d, Year=%d\n", ts->tm_hour, ts->tm_min, ts->tm_sec, ts->tm_mday, ts->tm_mon, ts->tm_year);
//exit(0);

// We play as client and connect to NTP time source server
print("Connecting NTP source time server: %s\n", ntpserverip);

if ((csd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
{
  perror("csd = socket(AF_INET, SOCK_DGRAM, 0)");
  exit(2);
}

bzero(&clin,sizeof(clin));
clin.sin_family=AF_INET;
clin.sin_addr.s_addr=inet_addr(ntpserverip); 
clin.sin_port=htons(123); 

if(connect(csd, (struct sockaddr *) &clin, sizeof(clin)) == -1)
{
 perror("connect(csd, (struct sockaddr *) &clin, sizeof(clin))");
 exit(1);
}


// Listener part
if ((ssd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
{
  perror("ssd = socket(AF_INET, SOCK_DGRAM, 0)");
  exit(2);
}

bzero(&srin,sizeof(srin));
srin.sin_family=AF_INET;
srin.sin_addr.s_addr=INADDR_ANY;
srin.sin_port=htons(123); 

if(bind(ssd,(struct sockaddr *)&srin,sizeof(srin)) < 0)
{
 perror("bind(ssd,(struct sockaddr *)&srin,sizeof(srin))");
 exit(2);
}

print("Ready for NTP client query\n\n");

len=sizeof(ntp_hdr);

while(1)
{
 // Client's query
 srin_tmp_len=sizeof(srin_tmp);
 bzero((void *) &ntp_hdr, len);
 if((n=recvfrom(ssd,&ntp_hdr,len,0,(struct sockaddr *) &srin_tmp,&srin_tmp_len)) < 0)
 {
  perror("recvfrom(ssd)");
  exit(2);
 }
 print("Received %i B from client %s\n", n, inet_ntoa(srin_tmp.sin_addr));

 if(verbose)
  printNtp(&ntp_hdr);
 
 // Accept only Clinet's mode requests (3)
 mode=ntp_hdr.ntp_li_vn_mode & 0x7;
 if(mode != 3)
 { 
  print("Packet's mode (%02hi) is not supported. Ignoring request\n\n", mode);
  continue;
 }

 // Client's query forwarded to server
 if((n=write(csd,&ntp_hdr,n)) < 0)
 {
  perror("write(csd)");
  exit(2);
 }
 print("Client's query forwarded to server\n");

 // Server's response 
 bzero((void *) &ntp_hdr, len);
 if((n=read(csd,&ntp_hdr,len)) < 0)
 {
  perror("read(csd)");
  exit(2);
 }
 print("Received %i B from server %s\n", n, ntpserverip);
 if(verbose)
  printNtp(&ntp_hdr);

 // Modification of server's response
 if(!WasLsApplied)
 { // Part before leap second window
  if((ntohl(ntp_hdr.ntp_xmt_ts.integer) + timeoffset) >= lswindow)
  {
   WasLsApplied=true;
   goto afterLS;
  } 
  // Add leap second indicator 
  if(ls == 1)
   ntp_hdr.ntp_li_vn_mode+=64;  // Add LS
  else 
   ntp_hdr.ntp_li_vn_mode+=128; // Subtract LS

  ntp_hdr.ntp_ref_ts.integer=htonl(ntohl(ntp_hdr.ntp_ref_ts.integer) + timeoffset);
  ntp_hdr.ntp_rec_ts.integer=htonl(ntohl(ntp_hdr.ntp_rec_ts.integer) + timeoffset);
  ntp_hdr.ntp_xmt_ts.integer=htonl(ntohl(ntp_hdr.ntp_xmt_ts.integer) + timeoffset);
 }
 else // After leap second window subtract 1 second
afterLS: 
 {
  ntp_hdr.ntp_ref_ts.integer=htonl(ntohl(ntp_hdr.ntp_ref_ts.integer) + timeoffset - ls);
  ntp_hdr.ntp_rec_ts.integer=htonl(ntohl(ntp_hdr.ntp_rec_ts.integer) + timeoffset - ls);
  ntp_hdr.ntp_xmt_ts.integer=htonl(ntohl(ntp_hdr.ntp_xmt_ts.integer) + timeoffset - ls);
 }

 // Modified server's response forwarded to client
 if((n=sendto(ssd,&ntp_hdr,n,0,(struct sockaddr *) &srin_tmp,srin_tmp_len)) < 0)
 {
  perror("sendto(ssd)");
  exit(2);
 }

 print("Modified response (%i B) sent to client %s\n", n, inet_ntoa(srin_tmp.sin_addr));
 if(verbose)
  printNtp(&ntp_hdr);

 printf("\n");

}

close(ssd);
close(csd);
}


//*************************
//     Parse parameters
//*************************
void pparam(argc, argv)
int argc;
char *argv[];
{
int opt, len;
char strndelay[NRLENGTH];
static struct option longopts[]=
{
 {"source-time", required_argument, NULL, 's'},
 {"delay",       required_argument, NULL, 'd'},
 {"leapsecond",  required_argument, NULL, 'l'},
 {"verbose",     no_argument,       NULL, 'v'},
 {"help",        no_argument,       NULL, 'h'},
 {0, 0, 0, 0}
};

if(argc < 3)
{
 usage();
 exit(1);
}

while((opt=getopt_long(argc, argv, "s:d:l:vh", longopts, NULL)) != -1)
{
 switch(opt)
 {
 case 's': // NTP server IP
  len=strlen(optarg);
  if(len > 15 || len < 7) // IP size -> XXX.XXX.XXX.XXX - X.X.X.X 
  {
   printf("Wrong argument for option \'-s\'\n");
   usage();
   exit(1);
  }
  strncpy(ntpserverip,optarg,len);
  break;
 
 case 'd': // Set delay in seconds
  sscanf(optarg, "%ld", &bmidnight);
  snprintf(strndelay, NRLENGTH, "%ld", bmidnight);
  if(strcmp(optarg, strndelay) != 0)
  {
   memset(strndelay, '9', NRLENGTH-1);
   strndelay[NRLENGTH-1]=0;
   printf("Wrong argument for option \'-d\' or value out of range [-%.*s,%s]\n", (NRLENGTH-2), strndelay, strndelay);
   usage();
   exit(1);
  }

//  bmidnight=atoi(optarg);
  break;
 
 case 'l': // Insert/delete leap second 
  if(strncmp(optarg, "add", 3) == 0)
   {
   ls=1;
   break;
   }

  if(strncmp(optarg, "del", 3) == 0)
   {
   ls=-1;
   break;
   }
  
  printf("Wrong argument for option \'-l\'\n");
  usage();
  exit(1);
  break;
 
 case 'v': // Verbose
  verbose=true;
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
printf("Usage: ntpproxy -s server_ip [-d seconds] [-l add|del] [-v] [-h]\n");
printf("-s, --source-time IP of NTP source time server\n");
printf("-d, --delay       delay before leap second accomplishment. Default 600 seconds\n");
printf("-l, --leapsecond  add: insert leap second, del: delete leap second. Default add\n");
printf("-v, --verbose     debug information\n");
printf("-h, --help        display this help\n");
}


//***************************
// Parse NTP header and
// print values
//***************************
void printNtp(ntp_hdr)
struct libnet_ntp_hdr *ntp_hdr;
{
unsigned char li, version, mode;
register int f;
register float ff;
register u_int32_t luf;
register u_int32_t lf;
register float lff;

// leap second
li=ntp_hdr->ntp_li_vn_mode >> 6;

// version
version=(ntp_hdr->ntp_li_vn_mode >> 3) & 0x7;

// mode
mode=ntp_hdr->ntp_li_vn_mode & 0x7;

print("Leap=%02hi, Ver=%02hi, Mode=%02hi, Stratum=%02hi, ", li, version, mode, ntp_hdr->ntp_stratum);
printf("Poll=%03hi, Precision=%#02hx, ", ntp_hdr->ntp_poll, ntp_hdr->ntp_precision);
f=ntohs(ntp_hdr->ntp_delay.fraction);
ff=f / 65536.0;
f=ff * 1000000.0;
printf("Delay=%d.%06d, ", ntohs(ntp_hdr->ntp_delay.integer), f);

f=ntohs(ntp_hdr->ntp_dispersion.fraction);
ff=f / 65536.0;
f=ff * 1000000.0;
printf("Dispersion=%d.%06d, ", ntohs(ntp_hdr->ntp_dispersion.integer), f);

printf("ReferenceID=%#lx, ", ntp_hdr->ntp_reference_id);

luf=ntohl(ntp_hdr->ntp_ref_ts.fraction);
lff=luf;
lff=lff/4294967296.0;
lf=lff*1000000000.0;
printf("ReferenceTS=%u.%09d, ", ntohl(ntp_hdr->ntp_ref_ts.integer), lf);

luf=ntohl(ntp_hdr->ntp_orig_ts.fraction);
lff=luf;
lff=lff/4294967296.0;
lf=lff*1000000000.0;
printf("OriginateTS=%u.%09d, ", ntohl(ntp_hdr->ntp_orig_ts.integer), lf);

luf=ntohl(ntp_hdr->ntp_rec_ts.fraction);
lff=luf;
lff=lff/4294967296.0;
lf=lff*1000000000.0;
printf("ReceiveTS=%u.%09d, ", ntohl(ntp_hdr->ntp_rec_ts.integer), lf);

luf=ntohl(ntp_hdr->ntp_xmt_ts.fraction);
lff=luf;
lff=lff/4294967296.0;
lf=lff*1000000000.0;
printf("TransmitTS=%u.%09d\n", ntohl(ntp_hdr->ntp_xmt_ts.integer), lf);

// Dump hexadecimally NTP packet
print("NTP header hex dump:\n");
hexdump((char *) ntp_hdr, sizeof(struct libnet_ntp_hdr));
}

//***************************
// Print hexadecimally 
//***************************
void hexdump(char data[], int len)
{
int  caddr=0, rcount, i;
char buf[16];

while(1)
{
 printf("%05x  ", caddr);
 if((rcount=len-caddr) > 16)
  rcount=16;

 memcpy(buf, data+caddr, rcount);
 caddr+=rcount;

 for(i=0; i < rcount; i++)
  printf("%02x ", buf[i] & 0xff);

 for (; i < 16; i++) 
  printf("   ");
 printf("|");

 for(i=0; i < rcount; i++) 
  if(isgraph(buf[i])) 
   putchar(buf[i]);
  else
   putchar('.');

 printf("|\n");

 if(rcount < 16 || len <= caddr)
  break;
}
}


//***************************
// Print info string with
// timestamp
//***************************
void print(const char *format,...)
{
struct timeval tv;
struct tm *tm;
va_list arg;

if(gettimeofday(&tv, NULL) == -1)
{
 perror("gettimeofday()");
 exit(1);
}

if((tm=localtime(&tv.tv_sec)) == NULL)
{
 perror("localtime()");
 exit(1);
}
printf("%02d.%02d.%02d %02d:%02d:%02d.%06d ", tm->tm_mday, tm->tm_mon+1, tm->tm_year-100, tm->tm_hour, tm->tm_min, tm->tm_sec, tv.tv_usec);

va_start(arg, format);
vprintf(format, arg);
va_end(arg);

}

