
/*
ip2cc.c
ANSI C
POSIX.1 / WIN32 (for lock only)
GNU C Compiler-aware, for packed structures (see ip2cc.h)
(C) 2003 Corebase, Easymatic, Cynergi, Pedro Freire

This script can be called with:
	[-hbc] [ [-uar46] <arg> ]...

-h	Show help
-b	Run a short benchmark (only available if NDEBUG not defined)
-c	CGI mode: look for an ACCEPT-LANGUAGE HTTP header string in standard
	input, and for REMOTE_SERVER and REMOTE_ADDR CGI environment strings in
	the environment, and output and HTTP redirect for the proper language file
-u	Signals to output all (following) country and language codes in UPPERCASE
	(default is lowercase)
-a	This next argument is an ACCEPT-LANGUAGE HTTP header string
-r	This next argument is a REMOTE_SERVER CGI environment string
-4	This next argument is an IPv4 address
-6	This next argument is an IPv6 address

Note:
* If none of -a, -r, -4 or -6 are used, there is some sort of auto-detection.
* -a, -r and -c are not yet implemented
* IPv6 (auto-detected or with -6) will ALWAYS return "??" (not found) as it
  is not yet implemented

The return value is one of:
	0 -> ok
	1 -> any error

For each <arg> supplied, one line is returned in stdout, in the same order as
the arguments on the command line, with the country's 2-letter ISO code
(note: "cz" is Czech Republic, not "cs", "tl" is East Timor, not "tp" and
"gb" is Great Britain / United Kingdom, not "uk"), plus some additional data
(none at this time - not yet implemented). If the country isn't found, a line
with just "??" is returned.

If you need only the ISO country code, be sure to read only the FIRST two
characters as there will be more information in each line, in the future.


Algorythm requirements
----------------------

What were we looking for? We needed an algorythm that performed a search on
a sortable population, returning information on the result. The population
is ranges of IP numbers, but as such ranges do not overlap (*), we can
sort this population. Sorting is important as it allows us to implement
binar search, one of the fastest search methods available.

I will not go into the details of binary search or binary trees. The reader
is assumed to know this, or the be able to get hold of documentation that
will explain this.

But now let's look at the problems of implementing a binary search algorythm
in this case. In memory we use pointers, and it is irrelevant where we need
to move to as we travel the tree. But our database will be a file with about
2Mb of data: though it can be loaded into memory, we shouldn't be wasting it
with something as non-crutial as determining the country of the caller.


Algorythm solution
------------------

We could replace pointers with offsets into the file, and load each node as
we travel it to find any data. This greatly reduces the amount of data to be
loaded at program startup (from 2Mb to zero), but we will need to be hopping
around the file which is somewhat time-consuming. On the other hand, due to
hard disk data structure and operating system optimizations, even though we
may only request some 10 bytes per node, the system will actually read
something between 512 to 4096 bytes for that node. And since the nodes will
be quite spaced as we travel through the binary tree, the extra data read
is just wasted. So we need to make that extra data useful (because there is
a nearly zero-time impact of reading more bytes, up 512 at least), while at
the same time reducing the number of hops around the file.

We do this by arranging groups of tree nodes into "clusters". Whenever we
are traveling through a tree, for each node, we will either stop at that node
(if we've found what we're looking for), or travel to its left or right
children (direct descendants). We will NEVER (in any future iteration of the
current tree travel) travel back up, or to ANY nodes that do NOT belong to
the current node's descendants. We can therefore group "n" consecutive levels
of a tree, starting at any node, and we know that if a search travel enters
that "cluster" of nodes, it can only leave by its local leafs, NEVER by any
other node.

The idea is to therefore group a tree into clusters similar to this:

                                 cluster 0
                                    (O)
                          _________/   \_________
                         /                       \
                      (O)                         (O)

                  /         \                 /         \

            cluster 4  |  cluster 3  |  cluster 2  |  cluster 1
               (O)     |     (O)     |     (O)     |     (O)
              /   \    |    /   \    |    /   \    |    /   \
             (O) (O)   |   (O) (O)   |   (O) (O)   |   (O) (O)

Clusters are stored sequentially on the database file, so they are numbered
from top to bottom (in terms of levels), and at each level range, from right
to left (higher IP ranges are more likelly to contain the IP we're looking
for), so that the earliest search steps are as close as possible to the
file's start, and are hence faster.

Now because we do not need to hop through the database file at *each*
iteration, but rather only when crossing clusters, the number of file seeks
is greatly reduced. And as we have more than one node at each cluster, the
file read no longer has as much wasted space.

The cluster size in bytes should be lower than, but as close as possible to
512 bytes, or whichever filesystem cluster size your operating system uses.
We must therefore maximize the number of nodes per cluster, now.

Because we need to have a complete balanced "sub-tree" in each cluster, and
such tree has always (2^n)-1 nodes (for "n" levels - see bellow in "facts
about binary trees"), and because we will need an array of pointers to the
"next cluster" of size 2^n, we only need now to determine the size of each
of these array's elements. This is explained in "clusters".

(*) In a very few cases, they *do* overlap, but those are obvious errors in
the RIRs' databases.


"Clusters"
----------

We have seen the need for clusters, how do we arrange tree nodes in clusters,
and how do we relate each cluster with another. Now how many nodes can we
place in each cluster?

Let's see. The list of pointers to the "next" node (see diagram bellow) is
simple. A real-world IPv4-to-country database has less than 60 thousand
entries, so even in the worst-case cenario with only one node per cluster,
you would need 16 bits to describe a cluster number, and hence two bytes
per "next" array entry. 8 bits per cluster number is too little: we would
need *at least* 256 nodes per cluster, which would mean 512/256=2 bytes per
node. Because we need *at least* the full start IP number described in the
node (4 bytes), this is impossible.

Now in each node we need:
	* Start IP address of the range
	* End IP address of the range, or at least the range mask or size
	* The country code we want to return in the range matches

Simple things first: we needn't have two characters to encode the country
code. For obvious reasons we'll just use an unsigned integer. There are
239 countries that can be returned, which is way too close to the 8-bit
limit of 256. We therefore need at least 8, but ideally more bits to encode
the return contry code value (more allows future expansion).

The start IP address of the range needs to be a full IP address (32 bits).
No compression is possible, because even though quite often the lowest-order
bits are at 0, this is *not* always the case.

Now for the end IP address... if we were to use an IP address we'd need 32
bits. This would make a total node size of at least 4+4+1=9 bytes, and 56
of these nodes would fit in a 512-byte disk sector. Because the number of
nodes we have in each cluster must be of the form (2^n)-1, the next smaller
number we have is 31 nodes plus 32 "next-cluster" entries. The total would
be 31*9 + 32*2 = 343 bytes. This leaves a lot of "leftover" from the 512-byte
sector.

Since our original number of nodes was 56, let's see if we could cram a
63-node sub-tree into a cluster. This would require a 64-entry "next-cluster"
array, taking 128 bytes, leaving 384 bytes for the nodes, meaning a maximum
of 6 bytes per node. This way we'd have 63*6 + 64*2 = 506 bytes which only
wastes 6 bytes from each sector. Because the essential node data (start IP
number and country code) take at most 4+1=5 bytes, we just might be lucky
with this kind of data structure if we could cram the end IP or range size
or mask into only 8 bits.

Well, the end IP address is impossible to place there. It requires 32 bits
(4 bytes), although many of those bits are shared with the start IP address.
Namelly most of the high-order bits will be the same, and some low-order bits
(which in the start IP address are 0) will here be 1. But that is either a
difference between these addresses (range size), or a mask, not an end IP
address.

A mask would be perfect - we would only need 5 bytes to specify the mask size
(0 to 31 inclusive), in bits. This mask size determines how many bits to the
left of the IP range remain the same, while the remaining bits range from
all 0s to all 1s. However, even though the RIRs do attribute IP ranges in
this fashion, the LIRs (Local Internet Registries) often do not, and they
often attribute such ranges to companies in different countries, so a mask
will not work.

But a range size might. The range size is (end IP)-(start IP)+1. If masks
worked, range size would always be a power of 2, i.e., it would be a single
1 bit followed by a number of 0 bits. Even though LIRs often do not attribute
IP ranges as masks, the ranges they do attribute are quite similar to masks
in the sense that the range size is a (short) set of 1s and 0s, followed by
a number of 0 bits. We could then just encode this initial bit pattern,
followed by a "scaling factor" that determines (aproximatelly) how many 0
bits follow that pattern.

Through experimentation from a real-world IPv4-to-country database, we
determined that, in order to make this fit into 7 bits (1 bit goes to the
country, making it 9 bits in size), we would use 3 bits for the scaling
factor (the value in these 3 bits is multiplied by 4 to yield the real
scaling factor), and 4 bits for the size pattern. If those 4 bits didn't
allow for the coding of the required range size, you could split the range
into several database entries, each having a start IP and range size that
did fit into these constraints.

On this real-world sample, this forced the increase of the database by
some 10%, but as redundancy checks on other database entries had already
eliminated over 25% of the database, and since this increase did not
increase the number of clusters, we dedicided to keep this layout.

Therefore, if we had only 3 nodes per cluster, each cluster would have a
set of complete balanced tree nodes in the following format:

                               nodes[1]
    nodes[3]   =              /        \
                      nodes[0]          nodes[2]
                     /       \         /       \
    next[3+1]  =  next[0]  next[1]  next[2]  next[3]

The "next[]" array holds the cluster numbers where you may find the
nodes the follow those branches of the tree. If the tree ended just
in the previous node, these cluster numbers hold 0 (as 0 is the first
cluster number, no next[] entry can have 0 as a valid "next" cluster).

If the tree ends before these "next[]" entries, we fill the "balanced"
tree of the cluster with "fake" nodes containing IP = 0xFFFFFFFF. For
instance if nodes[1] was a leaf (the end of the tree), nodes[0] and
nodes[2] would both have IP = 0xFFFFFFFF, and (obviously) all next[]
entries would also be 0.

This is to allow a standard binary search algorythm to scan the nodes[]
array without having to adapt to varying "cluster filled" rates.


Facts about binary trees
------------------------

Two interesting facts about binary trees are required to better understand
this code, and specially its macro constants.

First, some notions. In a balanced binary tree such as:

                                    (O)
                                   /   \
                                (O)     (O)
                               /   \   /   \
                              (O) (O) (O) (O)

you have 3 LEVELS: the single node on the top (also called ROOT node) is
the first level, its two nodes bellow it (its descendants) are the second
level, and the four nodes at the bottom (also called LEAFS) are the third
level.

In a BALANCED binary tree, the number of nodes bellow each node at the same
level is the same or differs by at most one. For instance:

                         (O)                   (O)
                        /   \                 /   \
                     (O)     (O)    and    (O)     (O)
                    /       /   \         /
                   (O)     (O) (O)       (O)

are balanced, but

                         (O)                   (O)
                        /   \                 /
                     (O)     (O)    and    (O)
                            /   \         /
                           (O) (O)       (O)

are not.

Balanced trees guarantee the fastest searches. For more information of binary
trees, please lookup other documentation.

In this document, we will however also often refer to "balanced trees" in the
extended sense of trees where all its leafs are at the same level, and all
nodes except leafs have two descendants. We will call these "complete balanced
trees". The first tree diagram shown here and "clusters" of trees, as we use
them here are such "complete balanced trees". If leafs were to end at the
previous level, filler nodes (*after* the leafs) are added to the "clusters".

Now for the facts of binary trees.

1) A complete balanced binary tree with a total of "n" levels, has (2^n)-1
nodes in it (including root and leafs).

2) If you added a new level to this tree (that prior to the addition of the
level has "n" levels), with two new leafs for each of the previous leaves,
there would be 2^n new leafs in that new level.


Implementation notes
--------------------

It is a BAD idea to try to have different database files to skip a few
initial steps. This is because when looking at some of the high bits of the
searched IP number to find the file, we might be taking into consideration
bits that belong to the host part, not the network part, and will therefore
throw the search into the wrong database file.


IPv6
----

IPv6 addresses are encoded as unsigned32[4] as

	unsigned32[3]  unsigned32[2]  unsigned32[1]  unsigned32[0]

which means array index 0 has the lowest-significant word and
array index 3 has the highest-significant word. IPv4 addresses encoded
within an IPv6 address have array indexes 3, 2 and 1 at zero (0).
In such case, search is referred to the IPv4 address space.

In the database, we will only use array indexes 3 and 2. This is because
under the IPv6 spec, array indexes 1 and 0 hold a globally-unique medium
ID, such as a MAC. It is indexes 3 and 2 that hold the RIR attributed
range mask (high 48 bits) and the user/LIR subnet ID (low 16 bits).
You only need these to figure out the user's country of origin.


Compile and test
----------------

This code *MUST* be compiled using compiler options that ensure that C
structs s_cluster4 and s_cluster6 will *NOT* have holes in them. C allows
the compiler to add "holes" to structures (structs) so that an array of
such structure elements has all its items aligned on some boundary that
makes overall access faster. We need this disabled to make sure the structs
we define are only as big as we define them, and not bigger (so that they
fit on the expected sector and cluster sizes).

Doing this code-wide may not be a good idea as it often breaks the current
compiler's libraries structures. So our code has two macros built-in
(they're defined as empty if you do not define them in the compiler's
command line, unless the code detects GNU C Compiler, in which case it
adds the proper macros for this compiler). These macros are:

	PACK_ATTR1: code that should precede the "struct" keyword;
	PACK_ATTR2: code that should follow the struct's closing brace,
		    but precede the variable name.

Optimization for space (smaller code) would be best as the algorythm doesn't
need to be much sped up. For example, the line to compile this for the GNU
C Compiler (GCC) under Windows, is:

	gcc -O2 -Os -s -Wall -DNDEBUG -DSECTOR_SIZE=512 ip2cc.c -o ip2cc.exe

and for GCC under UNIX (Linux, etc):

	gcc -O2 -Os -s -Wall -DNDEBUG -DSECTOR_SIZE=512 ip2cc.c -o ip2cc

PLEASE BEWARE THAT IF YOU COMPILE IP2CC AND MK-IP4DB IN DIFFERENT PLATFORMS,
THE FILE THAT THE LATTER CREATES MAY NOT WORK WITH THE FORMER, AS EACH
PLATFORM'S COMPILER MAY HAVE USED DIFFERENT SECTOR_SIZE VALUES! TO PREVENT
THAT MAKE SURE YOU COMPILER COMMAND LINE DEFINES COMMON SYMBOL SECTOR_SIZE,
AS IN THE ABOVE EXAMPLE.

To test the code, you may try IP number 194.65.14.75 which should result in
country 'pt' (Portugal) - at least in 2003.

Note: With "all" warnings displayed, some compilers (like GCC) may report
that "ip4 may be used uninitialized". Ignore this error. The only way to
silence this error in ANSI C make the code slightly slower, so there is
really no point.


Speed
-----

What does all this mean? Well... after a real-world IPv4-to-country database
has been converted into this format, it takes about the same size as the
same database in text format (about 2Mb). This is due to overhead on the
last-level clusters that are mostly empty. However, this database will only
require a MAXIMUM of 3 cluster iterations (3 cluster levels) before finding
a result, which means the code will load from those 2Mb a maximum of 3*512 =
1.5kb! And look at the function that performs that search: 45 lines
(including comments) of a very simple integer-based algorythm.

An informal benchmark (compiled here if NDEBUG is *not* defined), results
in speeds of 14 thousand to 50 thousand queries per second, this on a 1GHz
Pentium-III processor of an under-optimized laptop. The lowest bechmark was
for the first run of the benchmark (file not cached), and the highest for
the following runs. This is *very impressive* for a piece of code that has
a memory footprint of only around 20kb! Of course that loading the entire
database into memory would give it more speed, but if want to use this in
a shared environment, actual physical memory is a very precious resource...

If you take into account that you can run in parallel 100 of these programs
where one similar program that loads the entire database into memory runs,
(when comparing memory usage) then you get an adjusted benchmark of
100*50000 = 5 million queries per second!

*/


#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#ifndef NDEBUG
#include <time.h>
#include <stdlib.h>
#endif
/* for lock code: */
#ifdef WIN32
#include <windows.h>  /* req by winbase.h */
#include <winbase.h>
#else
#include <time.h>
#include <sys/stat.h>
#endif


#include "ip2cc.h"
#include "ip2cc-countries.h"


/* System return values:
*/
#define RV_OK			0
#define RV_ERROR		1


/* Sorry - preprocessor doesn't like casts...
*
#if INT_MAX < (CC_MASK4 >> CC_SHIFT4)
#error "Cannot return country code as an int: please review find_ip4_country() return value"
#endif
*
#if INT_MAX < (CC_MASK6 >> CC_SHIFT6)
#error "Cannot return country code as an int: please review find_ip6_country() return value"
#endif
*/


/* Function prototypes
*/
int find_ip4_country( unsigned32 ip4, FILE *fp );
int find_ip6_country( unsigned32 ip6[4], FILE *fp );


/* Main
*/
int main( int argc, char *argv[] )
{
	int opt_uppercase = 0;  /* default: return ISO2 code in lower-case */
	int opt_next_ip_v = 0;  /* next argument on command line is an IP version # number (0=auto-detect) */

#ifdef WIN32
	static char lockfile[] = "D;]fty]ebub]topp{f/mph";
		/* every char is advanced by 1 in ASCII, so the above string
		   represents: "C:\\esx\\data\\snooze.log" */
	WIN32_FILE_ATTRIBUTE_DATA bufattr;
	SYSTEMTIME locktime;
	FILETIME ft;
#else
	static char lockfile[] = "0fty0ebub0topp{f/mph";
		/* every char is advanced by 1 in ASCII, so the above string
		   represents: "/esx/data/snooze.log" */
	struct stat bufstat;
	struct tm locktime;
#endif
	FILE *fp4, *fp6;
	unsigned32 ip4;
	unsigned32 ip6[4];
	unsigned int ipp[8];  /* IP address part (up to 8 on IPv6) */
	char *ps, *pexe;
	int i, cc;
#ifndef NDEBUG
	clock_t t0, t1;
	long int ti;
#endif

	/* check if we are running in the right server, otherwise
	   cripple this software */
	/* decode the "lockfile" string: */
	for( ps=lockfile;  *ps;  ps++ )
		(*ps)--;
#ifdef WIN32
	/* set "locktime" to date "2002-03-18 11:23:03": */
	locktime.wYear = 2002;		/* 2002 */
	locktime.wMonth = 3;		/* March (03) */
	locktime.wDay = 18;		/* 18th of March */
	locktime.wHour = 11;		/* 11h AM */
	locktime.wMinute = 23;		/* 23 minutes */
	locktime.wSecond = 3;		/* 3 seconds */
	locktime.wMilliseconds = 0;	/* 0 ms */
	SystemTimeToFileTime( &locktime, &ft );
	i = (int) GetFileAttributesEx( lockfile, GetFileExInfoStandard, &bufattr );
	if( i == 0									||
	    bufattr.dwFileAttributes != (FILE_ATTRIBUTE_READONLY)			||
	    CompareFileTime(&bufattr.ftLastWriteTime, &ft) != 0				||
	    bufattr.nFileSizeHigh             != (DWORD) 0				||
	    bufattr.nFileSizeLow % (DWORD) 17 != (DWORD) 0 )
#else
	/* set "locktime" to date "2002-03-18 11:23:03": */
	locktime.tm_year = 2002-1900;	/* 2002 */
	locktime.tm_mon = 3-1;		/* March (03) */
	locktime.tm_mday = 18;		/* 18th of March */
	locktime.tm_hour = 11;		/* 11h AM */
	locktime.tm_min = 23;		/* 23 minutes */
	locktime.tm_sec = 3;		/* 3 seconds ("touch" command cannot set this) */
	locktime.tm_isdst = 0;		/* DST *NOT* in effect */
	/* test the lock file: */
	i = lstat( lockfile, &bufstat );
	if( i != 0									||
	    !S_ISREG(bufstat.st_mode)							||
	    bufstat.st_nlink != 1							||
	    bufstat.st_uid != 0								||
	    bufstat.st_gid != 0								||
	    (bufstat.st_mode & (S_IRUSR | S_IRGRP)) != (S_IRUSR | S_IRGRP)		||
	    (bufstat.st_mode & (S_IWUSR | S_IXUSR | S_IWGRP | S_IXGRP | S_IRWXO)) != 0	||
	    bufstat.st_mtime != mktime(&locktime)					||
	    bufstat.st_size % 17 != 0 )
#endif
		{
		fputs( "Bad IPv4 number or bad argument.\n", stderr );  /* report unrelated error to confuse hackers */
		return RV_ERROR;
		}
	/* ok, so, in order to be able to run this software,
	   we must have a REGULAR file:
		* named as described in "lockfile";
		* with no other hard links to it (not checked under Windows);
		* owned by root.root (ids 0) (not checked under Windows);
		* with "ug=r,o=" permissions (read-only in Windows);
		* with size smaller than 4Gb, but a multiple of 17 (regardless of contents);
		* with modification time of "2002-03-18 11:23:03";
	*/

#ifndef NDEBUG
	if( sizeof(struct s_cluster4) > SECTOR_SIZE )
		{
		fprintf( stderr, "Internal error: IPv4 cluster data (%i) is greater than expected (%i).\n"
			 "sizeof(nodes[])=%i, sizeof(next[])=%i, NODES_PER_CLUSTER4=%i\n"
			 "Make sure you call your compiler with options to eliminate holes in structures\n"
			 "(for instance, in GCC, you must call it with 'gcc -fpack-struct')\n",
			 sizeof(struct s_cluster4), SECTOR_SIZE,
			 sizeof(struct s_node4[NODES_PER_CLUSTER4]), sizeof(unsigned16[NODES_PER_CLUSTER4+1]), NODES_PER_CLUSTER4 );
		return RV_ERROR;
		}
	if( sizeof(struct s_cluster6) > SECTOR_SIZE )
		{
		fprintf( stderr, "Internal error: IPv6 cluster data (%i) is greater than expected (%i).\n"
			 "sizeof(nodes[])=%i, sizeof(next[])=%i, NODES_PER_CLUSTER6=%i\n"
			 "Make sure you call your compiler with options to eliminate holes in structures\n"
			 "(for instance, in GCC, you must call it with 'gcc -fpack-struct')\n",
			 sizeof(struct s_cluster6), SECTOR_SIZE,
			 sizeof(struct s_node6[NODES_PER_CLUSTER6]), sizeof(unsigned16[NODES_PER_CLUSTER6+1]), NODES_PER_CLUSTER6 );
		return RV_ERROR;
		}
#endif

	fp4 = fp6 = NULL;  /* signal neither has been opened */

	/* process each option and IP number on the command line: */
	opt_next_ip_v = 0;  /* 0 => auto-detect */
	for( pexe = *argv++;  (ps = *argv++); )
		{
		if( *ps == '-'  ||  *ps == '/' )
			{
			while( (cc = *++ps) )
				{
				switch( cc )
					{
#ifndef NDEBUG
					case 'b':
						/* benchmark */
						puts( "Starting benchmark... (takes from 1s to 15s)" );
						fp4 = fopen( DBFILE4, "rb" );
						if( fp4 == NULL )
							{
							fputs( "Cannot open IPv4-to-country database.\n", stderr );
							return RV_ERROR;
							}
						setbuf( fp4, NULL );  /* turn off buffering */
						t0 = clock();
						srand( 5 );
						for( ti = 1L;  ti <= 50000L;  ti++ )
							{
							ip4 = (((unsigned32) rand() & 0xFF) << 24) |
							      (((unsigned32) rand() & 0xFF) << 16) |
							      (((unsigned32) rand() & 0xFF) << 8)  |
							       ((unsigned32) rand() & 0xFF);
							find_ip4_country( ip4, fp4 );
							}
						t1 = clock();
						printf( "Speed is %.2f lookups per second.\n", ((double) ti)/( ((double) t1-t0)/CLOCKS_PER_SEC ) );
						break;
#endif
					case 'h':
						fprintf( stderr, "\n"
#ifndef NDEBUG
								 "Usage: %s [-hb] [ [-u46] <arg> ]...\n"
								 "-h  Show this help\n"
								 "-b  Run a short benchmark\n"
#else
								 "Usage: %s [-h] [ [-u46] <arg> ]...\n"
								 "-h  Show this help\n"
#endif
								 "-u  Signals to output all (following) country and language codes in UPPERCASE\n"
								 "    (default is lowercase)\n"
								 "-4  This next argument is an IPv4 address\n"
								 "-6  This next argument is an IPv6 address\n"
								 "\n"
								 "(C) 2003 Corebase, Easymatic\n"
								 "         www.easymatic.com\n"
								 "\n",
								 pexe );
						break;
					case 'u':
						opt_uppercase = 1;  /* true */
						break;
					case '4':
						opt_next_ip_v = 4;
						break;
					case '6':
						opt_next_ip_v = 6;
						break;
					default:
						fprintf( stderr, "Bad option. Use \"%s -h\" for help.\n", pexe );
						return RV_ERROR;
					}
				}
			continue;
			}

		/* if you do not know what to expect next on the command line,
		   try some auto-detection */
		if( !opt_next_ip_v )
			{
			if( strchr(ps, '.') )
				opt_next_ip_v = 4;
			else
				opt_next_ip_v = 6;
			}

		/* first let's try to parse an IPv6, as we may have to fall back to
		   IPv4 later if all 96 high-order IPv6 bits are 0 */
		if( opt_next_ip_v == 6 )
			{
			if( sscanf(ps, "%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x", &ipp[7], &ipp[6], &ipp[5], &ipp[4], &ipp[3], &ipp[2], &ipp[1], &ipp[0]) != 8  ||
			    ipp[7] > 0xFFFFU  ||  ipp[6] > 0xFFFFU  || ipp[5] > 0xFFFFU  || ipp[4] > 0xFFFFU  ||
			    ipp[3] > 0xFFFFU  ||  ipp[2] > 0xFFFFU  || ipp[1] > 0xFFFFU  || ipp[0] > 0xFFFFU )
				{
				fputs( "Bad IPv6 number or bad argument.\n", stderr );
				return RV_ERROR;
				}
			ip6[3] = (((unsigned32) ipp[7]) << 16) | ((unsigned32) ipp[6]);
			ip6[2] = (((unsigned32) ipp[5]) << 16) | ((unsigned32) ipp[4]);
			ip6[1] = (((unsigned32) ipp[3]) << 16) | ((unsigned32) ipp[2]);
			ip6[0] = (((unsigned32) ipp[1]) << 16) | ((unsigned32) ipp[0]);
			if( !(ip6[3] | ip6[2] | ip6[1]) )
				{
				/* ok, this is an IPv4 within an IPv6 */
				opt_next_ip_v = 4;
				ip4 = ip6[0];
				}
			}
		else  /* else, we were expecting an IPv4 number already */
			{
			if( sscanf(ps, "%3u.%3u.%3u.%3u", &ipp[3], &ipp[2], &ipp[1], &ipp[0]) != 4  ||
			    ipp[3] > 255U  ||  ipp[2] > 255U  || ipp[1] > 255U  || ipp[0] > 255U )
				{
				fputs( "Bad IPv4 number or bad argument.\n", stderr );
				return RV_ERROR;
				}
			ip4 = (((unsigned32) ipp[3]) << 24) |
			      (((unsigned32) ipp[2]) << 16) |
			      (((unsigned32) ipp[1]) << 8)  |
			       ((unsigned32) ipp[0]);
			}

		/* now that the numbers are parsed, just open the database file(s)
		   (if no open already) and run the search */
		if( opt_next_ip_v == 6 )
			{
			if( fp6 == NULL )
				{
				fp6 = fopen( DBFILE6, "rb" );
				if( fp6 == NULL )
					{
					fputs( "Cannot open IPv6-to-country database.\n", stderr );
					return RV_ERROR;
					}
				setbuf( fp6, NULL );  /* turn off buffering */
				}
			cc = find_ip6_country( ip6, fp6 );
			}
		else
			{
			if( fp4 == NULL )
				{
				fp4 = fopen( DBFILE4, "rb" );
				if( fp4 == NULL )
					{
					fputs( "Cannot open IPv4-to-country database.\n", stderr );
					return RV_ERROR;
					}
				setbuf( fp4, NULL );  /* turn off buffering */
				}
			cc = find_ip4_country( ip4, fp4 );
			}

		/* ouput the proper result to stdout */
		if( cc < 0  ||  cc >= (int) sizeof(cname_up)/sizeof(cname_up[0]) )
			puts( "??" );
		else
			puts( opt_uppercase ? cname_up[cc] : cname_low[cc] );

		/* make sure we reset the next argument type */
		opt_next_ip_v = 0;  /* 0 => auto-detect */
		}

	/* after all of the command line is parsed, just close any open
	   files and return */
	if( fp6 != NULL )
		fclose( fp6 );
	if( fp4 != NULL )
		fclose( fp4 );
	return RV_OK;
}


/*
Returns the country code if found, or
-1 for not found, -2 for looped cluster indexes, -3 for file access error
*/
int find_ip4_country( unsigned32 ip4, FILE *fp )
{
	struct s_cluster4 cluster4;	/* buffer where you'll read each cluster into */
	int ci, i, step;		/* cluster and node index, loop step */
	struct s_node4 *pn;		/* pointer to current node */

	i = 0;
	do	{  /* loops for each cluster */
		ci = i;
		if( fseek(fp, ((long int) ci) << SECTOR_SIZE_SHIFT, SEEK_SET)  ||
		    fread( &cluster4, (size_t) CLUSTER4_SIZE, (size_t) 1, fp) != 1 )
			return -3;  /* file access error */
		i = NODES_PER_CLUSTER4 >> 1;
		step = (NODES_PER_CLUSTER4 >> 2) + 1;
		for(;;)  /*forever*/  /* loops for each node in a cluster */
			{
			pn = &cluster4.nodes[i];
			if( pn->ip >= (unsigned32) 0xFFFFFFFFU )
				return -1;  /* not found */
			if( ip4 < pn->ip )
				i -= step;
			else if( ip4 >= pn->ip + ( ((unsigned32) (pn->ccsz & RANGE_MASK4) + (unsigned32) 1U) << ((pn->ccsz & RANGE_SHIFT_MASK4) >> RANGE_SHIFT_SHIFT4) ) )
				i += step;
			else
				return (int) (pn->ccsz & CC_MASK4) >> CC_SHIFT4;
			if( !step )
				break;
			step >>= 1;
			}
		/* at this point, i is an even number from
		   0 to NODES_PER_CLUSTER4-1 inclusive: all odd numbers
		   could ONLY have been visited during the previous
		   iterations (starts at an odd number and all "step"s are
		   even numbers, except the last that is always 1) */
		if( ip4 < pn->ip )
			i = cluster4.next[ i ];
		else  /* it's only here if not in range, so no need to check upper boundary */
			i = cluster4.next[ i | 1 ];
		}
		while( ci < i );
		/* make sure we don't get into an endless loop with bad
		   cluster indexes */
	return i == 0 ? -1 : -2;  /* not found, or looped cluster indexes */
}


/*
Returns the country code if found, or
-1 for not found, -2 for looped cluster indexes, -3 for file access error
*/
int find_ip6_country( unsigned32 ip6[4], FILE *fp )
{
	/* !!! not implemented yet
	struct s_cluster6 cluster6;*/	/* buffer where you'll read each cluster into */

	return -1;  /* not found */
}

