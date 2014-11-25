                        Leap Second Test tool
                        Latest update: 2 May 2014

Initial release:  Robert Karbowski <rkarbowski@amadeus.com>

Table of Contents
=================
1. Introduction
2. NTP and leap second
3. Compilation
4. Leap second test bed
    4.1 Direct LS simulation
        4.1.1 Environment setup
    4.2	Indirect LS simulation via NTP
        4.2.1 Environment setup
5. Useful links


1. Introduction
===============
Leap Second Test tool consists of two programs which can be used 
for OS/application leap second immunity verification. 
*Set Leap Second - local leap second simulation - only for Linux
*NTP Proxy       - indirect leap second simulation via NTP 
                   ntpproxy has to run on Linux yet it can be used 
                   to test other OS-es if they utilize NTP

Leap Second Test tool was tested on SLES11 however it should also 
run on any other Linux release. 

2. NTP and leap second
======================
Coordinated Universal Time (UTC) is the primary standard for civil time. 
UTC runs at the same frequency as International Atomic Time (TAI) however it 
differs from TAI by an integral number of seconds. This difference changes when
leap seconds (LS) occur. 
Network Time Protocol (NTP) is a networking protocol for clock synchronization
between computer systems. NTP provides UTC time. 
The International Earth Rotation and Reference System Service (IERS) watches
differences in the Earth's rotation. Every six months (January or July) IERS 
sends a bulletin message and reports whether or not a leap second will be added
in the end of the coming June or December. 
IERS arranges a leap second event to keep the time difference between 
atomic clocks and Earth’s rotation to below 0.9 seconds. Sometime prior to 
the day of leap second insertion the leap-indicator bits are set at 
the primary servers, presumably by manual means, and subsequently distributed 
via NTP throughout the synchronization subnet. Local NTP daemons then arm kernel
via adjtimex() function and leap second is inserted or deducted by kernel 
at UTC midnight without any other interaction with NTP. Kernel armament can be 
done at any time within 24 hours preceding LS event. On the day following 
insertion the bits are turned off at the primary servers.

LS can be also applied on the isolated computer systems with no access 
to the official NTP etalons. It is done via crafted file delivered by 
National Institutes of Standards and Technology (NIST) 
- see http://doc.ntp.org/4.2.6/ntpd.html#leap

3. Compilation 
==============
Compilation is simple like this:

cc -o ntpproxy ntpproxy.c 
cc -o sls sls.c

4. Leap second test bed
=======================
The leap second (LS) insertion is actually done by the kernel and NTP only 
plays the role of a messenger who triggers the action in advance. Therefore LS
could be simulated on OS directly, by local execution of adjtimex() function 
or indirectly by setting up the leap-indicator via NTP protocol.

4.1 Direct LS simulation
------------------------
LS kernel armament can be done via OS command adjtimex though it is cumbersome.
A dedicated tool instead was developed: SLS (Set Leap Second):

# ./sls -h
Usage: sls [-d seconds] [-l add|del] [-h]
-d      delay before leap second accomplishment. Default 600 seconds
-l      add: insert leap second, del: delete leap second. Default add
-h      help

SLS changes system time to be 600 seconds before midnight (see -d option 
for adjustment) and instructs kernel to apply the leap second at the end 
of the day.

4.1.1 Environment setup
- - - - - - - - - - - -
Because system time must be modified thus SLS has to be executed by root and
NTP service stopped in advance and re-started afterwards.

* Hints:
- Checking if kernel is armed:

# adjtimex -p | grep status | awk '{print "\nStatus:", $2; if(and($2, 16)) 
print "LS is INS"; else if(and($2, 32)) print "LS is DEL"; else print 
"LS is OFF"}'

Status: 80
LS is INS

- Reset kernel time status - useful if SLS has to be rerun without waiting 
for scheduled LS event:

# adjtimex -S 0

- When LS is applied, in /var/log/messages entries are logged:

May  2 23:59:59 tstsr604 kernel: [182259.586643] Clock: inserting leap second 
23:59:60 UTC

or

May  3 00:00:00 tstsr604 kernel: [182453.585459] Clock: deleting leap second 
23:59:59 UTC

4.2 Indirect LS simulation via NTP
----------------------------------
If there is a need to test many systems at the same time NTP Proxy could 
be used. It modifies passing NTP packets:

# ./ntpproxy -h
Usage: ntpproxy -s server_ip [-d seconds] [-l add|del] [-v] [-h]
-s      IP of NTP source time server
-d      delay before leap second accomplishment. Default 600 seconds
-l      add: insert leap second, del: delete leap second. Default add
-v      debug information
-h      help

Located between NTP time source and NTP clients it adds a leap second 
indicator to the responses received by the clients. The time stamps are also 
adjusted so NTP clients are told that time is 600 seconds (see -d option for 
tuning) before midnight of the next available leap second window day. 
For example if current date is from the first half of year then modified NTP 
time shows 30th of June. If current date from the second half of the year 
then it shows 31st of December.  

NTP Proxy supports only one NTP client but other clients can be cascaded 
like it is shown on the figure blow. 
 

               ----------------
              |    Any real    |
              |   NTP server   |
               ----------------
                      ^
                      |
                      v
               ----------------
              |   NTP Proxy    | <- Source of LS for the first client
               ----------------
                      ^
                      |
                      v
               ----------------
              |First NTP client| <- Source of LS for all other clients
               ----------------
                      ^
                      |
                      v
               --------------
              |  All other   |-
              | NTP clients  | |-
               --------------  | |
                 --------------  |
                   --------------

4.2.1 Environment setup
- - - - - - - - - - - -
NTP Proxy can be placed anywhere in the NTP chain, after stratum '1' server
however, minding security and stability of stratum '1' servers it is reasonable
setting up proxy at least after stratum '2' servers. 
Proxy uses NTP port 123 (UDP) therefore has to be executed by root and 
NTP service stopped in advance. Also NTP daemons on the servers undergoing 
LS verification have to be properly tuned to take the time from 
'NTP Proxy' or 'First NTP client'. 

* Remark: 
Time on the server hosting proxy is not modified, still it is also 
unsynchronized (NTP has to be stopped) hence there is no sense of doing 
any LS OS/application tests on that node.

* Hints:
- Kernel is armed with a leap second action only when NTP time is synchronized:

# ntpq -p
     remote           refid      st t when poll reach   delay   offset  jitter
==============================================================================
*tstsr604        1XX.XX.XXX.XXX   2 u   35   64  377    0.608    0.021   0.022

Asterisk (*) means that time is synchronized - it is also reflected in 
/var/log/messages:

Jun 30 22:25:23 tstsr001 ntpd[901102]: synchronized to 1XX.XX.XX.XX, stratum 2

After start of NTP client it takes a few minutes to synchronize clock.

- Checking NTP leap second status:

# ntpq -c rl
assID=0 status=4644 leap_add_sec, sync_ntp, 4 events, event_peer/strat_chg,
version="ntpd 4.2.4p8@1.1612-o Tue Dec 20 21:31:16 UTC 2011 (1)",
processor="x86_64", system="Linux/2.6.32.59-0.7-default", leap=01,
stratum=3, precision=-20, rootdelay=0.898, rootdispersion=228.551,
peer=29664, refid=1XX.XX.XX.XX,
reftime=d57b2f51.179d8f07  Sun, Jun 30 2013 22:27:29.092, poll=6,
clock=d57b3140.8864d8d8  Sun, Jun 30 2013 22:35:44.532, state=4,
offset=0.021, frequency=23.985, jitter=0.024, noise=0.006,
stability=0.001, tai=0

Meaning of 'leap=' field:
00 - No warning
01 - Last minute has 61 seconds
10 - Last minute has 59 seconds
11 - Clock is not synchronized

After midnight i.e. after applying LS, NTP Proxy stops sending leap-indicator
however it takes a few minutes till NTP/kernel removes LS flags. 

- Checking if kernel is armed:

# adjtimex -p | grep status | awk '{print "\nStatus:", $2; if(and($2, 16)) 
print "LS is INS"; else if(and($2, 32)) print "LS is DEL"; else 
print "LS is OFF"}'

Status: 80
LS is INS

- Reset kernel time status - useful if NTP PROXY has to be rerun without 
waiting for scheduled LS event:

# adjtimex -S 0

- When LS is applied, in /var/log/messages entries are logged:

Jun 30 23:59:59 tstsr604 kernel: [182259.586643] Clock: inserting leap second 
23:59:60 UTC

or

Jul  1 00:00:00 tstsr604 kernel: [182453.585459] Clock: deleting leap second 
23:59:59 UTC



5. Useful links
===============

NTP Timescale and Leap Seconds:
http://doc.ntp.org/4.1.0/leap.htm

Leap Seconds:
http://maia.usno.navy.mil/leapsec.html

IERS Bulletin C - leap second announcement:
http://hpiers.obspm.fr/iers/bul/bulc/bulletinc.dat
or
http://www.iers.org/nn_10970/IERS/EN/DataProducts/EarthOrientationData/
__Function/generischeTabelle__ID16.html?__nnn=true

IERS Bulletin C subscription:
http://hpiers.obspm.fr/eop-pc/products/bulletins/bulletin_registration.html

RFC 1129: Internet Time Synchronization: the Network Time Protocol 
http://tools.ietf.org/pdf/rfc1129

RFC 5905: Network Time Protocol Version 4: Protocol and Algorithms 
Specification
http://tools.ietf.org/html/rfc5905