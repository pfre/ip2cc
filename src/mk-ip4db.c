
/*
mk-ip4db.c
ANSI C
GNU C Compiler-aware, for packed structures (see ip2cc.h)
(C) 2003-2011 Corebase, Easymatic, Cynergi, Pedro Freire

This script can be called with:
	[-#] <source-ip-to-country-data-file> [<dest-ip4db-file>]

where -# represents a number specifying the source data file format:
-1  "<ip-start>","<ip-end>","<iso-country>","...","..."  (default)
-2  "<ip-start>","<ip-end>","<iso-country>","..."
-3  "<...>","<...>","<ip-start>","<ip-end>","<iso-country>","...","..."
-4  "<...>","<...>","<ip-start>","<ip-end>","<iso-country>","..."

Calling it without arguments gives this help.

See comments at the top of ip2cc.c for more information.


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

	gcc -O2 -Os -s -Wall -DNDEBUG -DSECTOR_SIZE=512 mk-ip4db.c -o mk-ip4db.exe

and for GCC under UNIX (Linux, etc):

	gcc -O2 -Os -s -Wall -DNDEBUG -DSECTOR_SIZE=512 mk-ip4db.c -o mk-ip4db

PLEASE BEWARE THAT IF YOU COMPILE IP2CC AND MK-IP4DB IN DIFFERENT PLATFORMS,
THE FILE THAT THE LATTER CREATES MAY NOT WORK WITH THE FORMER, AS EACH
PLATFORM'S COMPILER MAY HAVE USED DIFFERENT SECTOR_SIZE VALUES! TO PREVENT
THAT MAKE SURE YOU COMPILER COMMAND LINE DEFINES COMMON SYMBOL SECTOR_SIZE,
AS IN THE ABOVE EXAMPLE.

To test the code, you may try IP number 194.65.14.75 which should result in
country 'pt' (Portugal) - at least in 2003.


TODO
----

Detect IP ranges reserved for LANs, and eliminate any ranges that fall into
those ranges.

*/


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>


#include "ip2cc.h"
#include "ip2cc-countries.h"


/* System return values:
*/
#define RV_OK			0
#define RV_ERROR		1


/* Low part of the range mask, not encoded in the database:
*/
#define	RANGE_LSB_MASK4		( (RANGE_SHIFT_MASK4 >> RANGE_SHIFT_SHIFT4) ^ ((unsigned16) 0x001F) )


/* scanf() format strings for data files
*/
const char *dfformats[] = {
	"\"%10lu\",\"%10lu\",\"%2c\",\"%*[^\"]\",\"%*[^\"]\"\n",
	"\"%10lu\",\"%10lu\",\"%2c\",\"%*[^\"]\"\n",
	"\"%*[^\"]\",\"%*[^\"]\",\"%10lu\",\"%10lu\",\"%2c\",\"%*[^\"]\",\"%*[^\"]\"\n",
	"\"%*[^\"]\",\"%*[^\"]\",\"%10lu\",\"%10lu\",\"%2c\",\"%*[^\"]\"\n" };


/* Buffer used to write a cluster into the file.
   As SECTOR_SIZE is always >= CLUSTER_SIZE, we add the
   maximum of SECTOR_SIZE bytes at the end of the real
   cluster structure to fill in the extra unused bytes
*/
struct s_sector4
	{
	struct s_cluster4 cluster4;
	char blank[ SECTOR_SIZE ];  /* initialized to all '\0' by C */
	}
	sector4;


/* Data type and pointers for internal lists and nodes that will
   be used in creating the final structure. Much of the additional
   data is repeated from the struct s_node# data type just to ease
   its use while creating the final clusters and nodes.
*/
struct s_list
	{
	struct s_node4 node;
	unsigned32 ip_start, ip_end;
	int cc, i, treelevel;
	long int cluster;
	struct s_list *pprev, *pnext, *treeleft, *treeright;
	}
	*pfirst = NULL, *plast = NULL, *treetop = NULL;


/* These hold the most shallow and deepest leaf levels found
   while building the balanced binary tree; in a true balanced
   binary tree, these may differ by only 1...
*/
int treelevel_min = INT_MAX;  /* set after running treenode() */
int treelevel_max = 0;        /* set after running treenode() */


/* Function prototypes
*/
struct s_list *treenode( struct s_list *pleft, struct s_list *pright,
			 long int entries, int level, long int *pnumnodes );
void treecluster( struct s_list *pnode, long int cluster, int i, int step );
void free_all( void );


/* Main
*/
int main( int argc, char *argv[] )
{
	static const char *dfformat;
	FILE *fp;
	unsigned long int ip_start, ip_end, range, range2, rmask;
	long int line, lines, lines_saved, lines_added, lines_reorder,
		lines_overlap, lines_overlap_del;
	long int cluster, clusters, cluster_old;
	int levelmin, levelmax;
	char ccstr[] = "??";
	struct s_list *pl, *pln, **ppl;
		/* pointer to list, pointer to list new,
		   pointer to pointer to list */
	char *ps;
	int i, i2, cc;

	/* Parse command-line help and data file format
	*/
	if( argc < 2  ||  argc > 4 )
		{
		fprintf( stderr, "\n"
				 "Usage: %s [-#] <source-ip-to-country-data-file> [<dest-ip4db-file>]\n"
				 "where -# specifies the source file format:\n"
				 "-1  \"<ip-start>\",\"<ip-end>\",\"<iso-country>\",\"...\",\"...\"  (default)\n"
				 "-2  \"<ip-start>\",\"<ip-end>\",\"<iso-country>\",\"...\"\n"
				 "-3  \"<...>\",\"<...>\",\"<ip-start>\",\"<ip-end>\",\"<iso-country>\",\"...\",\"...\"\n"
				 "-4  \"<...>\",\"<...>\",\"<ip-start>\",\"<ip-end>\",\"<iso-country>\",\"...\"\n"
				 "\n"
				 "(C) 2003-2011 Corebase, Easymatic\n"
				 "         www.easymatic.com\n"
				 "\n",
				 argv[0] );
		return RV_ERROR;
		}
	i = 0;  /* default format */
	if( *argv[1] == '-' )
		{
		cc = *(argv[1]+1);
		if( cc >= '1'  &&  cc <= '0'+sizeof(dfformats)/sizeof(dfformats[0])  &&  *(argv[1]+2) == '\0' )
			i = cc - '1';
		else
			{
			fprintf( stderr, "Bad source file format specifier.\n"
					 "Run %s without arguments for help.\n",
					 argv[0] );
			return RV_ERROR;
			}
		argv++;
		}
	argv++;
	dfformat = dfformats[i];

	/* Internal check to make sure binary search algorythm for
	   finding country codes is working properly
	*/
	puts( "Internal tests..." );
	if( sizeof(struct s_cluster4) > SECTOR_SIZE )
		{
		fprintf( stderr, "Internal error: cluster data (%li) is greater than expected (%i).\n"
			 "sizeof(nodes[])=%li, sizeof(next[])=%li, NODES_PER_CLUSTER4=%i\n"
			 "Make sure you call your compiler with options to eliminate holes in structures\n"
			 "(for instance, in GCC, you must call it with 'gcc -fpack-struct')\n",
			 (long int) sizeof(struct s_cluster4), SECTOR_SIZE,
			 (long int) sizeof(struct s_node4[NODES_PER_CLUSTER4]), (long int) sizeof(unsigned16[NODES_PER_CLUSTER4+1]), NODES_PER_CLUSTER4 );
		return RV_ERROR;
		}
	for( i = 0;  i < CNAME_SIZE;  i++ )
		{
		if( find_cc(cname_up[i]) != i )
			{
			fputs( "Internal error: cannot find some country codes.\n", stderr );
			return RV_ERROR;
			}
		}

	/* Open and read all the input data file into memory
	*/
	puts( "Reading source IP-to-country data file..." );
	fp = fopen( argv[0], "r" );
	if( fp == NULL )
		{
		fprintf( stderr, "Cannot open source IP-to-country data file (%s).\n", argv[0] );
		return RV_ERROR;
		}
	lines_reorder = lines_overlap = lines_overlap_del = lines = 0L;
	for( line = 1L;  !feof(fp);  line++ )
		{
		if( line % 10000L == 0L )
			printf( "Read %li lines so far...\n", line );
		if( fscanf(fp, dfformat, &ip_start, &ip_end, ccstr) != 3 )
			{
			fclose( fp );
			free_all();
			fprintf( stderr, "Error reading line %li of source of IPv4-to-country data file.\n", line );
			return RV_ERROR;
			}
		pln = malloc( sizeof(struct s_list) );
		if( pln == NULL )
			{
			fclose( fp );
			free_all();
			fprintf( stderr, "Not enough memory reading line %li of source IPv4-to-country data file.\n", line );
			return RV_ERROR;
			}
		pln->cluster = -1L;  /* "unknown" */
		pln->pprev = pln->pnext = pln->treeleft = pln->treeright = NULL;
		pln->treelevel = pln->i = -1;  /* "unset" */
		pln->ip_start = pln->node.ip = ip_start;
		pln->ip_end = ip_end;
		/* replace old ISO2 codes */
		if( !strcmp(ccstr, "CS")  ||  !strcmp(ccstr, "cs") )
			strcpy( ccstr, "cz" );
		else if( !strcmp(ccstr, "TP")  ||  !strcmp(ccstr, "tp") )
			strcpy( ccstr, "tl" );
		else if( !strcmp(ccstr, "UK")  ||  !strcmp(ccstr, "uk") )
			strcpy( ccstr, "gb" );
		pln->cc = find_cc( ccstr );
		if( ip_end < ip_start  ||  pln->cc < 0 )
			{
			free( pln );
			if( ip_end < ip_start )
				fprintf( stderr, "Bad IP range (start IP > end IP) reading line %li of source IPv4-to-country data file.\nSkipping line.\n", line );
			else
				fprintf( stderr, "Bad country code '%s' reading line %li of source IPv4-to-country data file.\nSkipping line.\n", ccstr, line );
			continue;
			}
		/* add this line to the list, sorted */
		lines++;
		for( ppl = &plast;  (pl=*ppl);  ppl = &(pl->pprev) )
			{
			if( pl->ip_start <= ip_start )
				break;
			if( pl->ip_start <= ip_end )
				{
				lines_overlap++;
				pln->ip_end = ip_end = pl->ip_start - 1U;
					/* just eliminate the overlapped part;
					   if the overlapped ranges refer to
					   the same country, later redundancy
					   check will optimize these ranges */
				if( ip_end < ip_start )
					{
					/* if the current node described a range of
					   only 1 IP, the above expression may have
					   invalidated the current node's range */
					lines_overlap_del++;
					free( pln );
					continue;
					}
				}
			}
		if( pl != NULL )
			{
			if( ip_start <= pl->ip_end )
				{
				/* this condition includes the case
				   ip_start == pl->ip_start */
				/* well, we know at least that ip_end does NOT
				   overlap with any of the following nodes */
				lines_overlap++;
				pln->ip_start = pln->node.ip = ip_start = pl->ip_end + 1U;
				if( ip_end < ip_start )
					{
					lines_overlap_del++;
					free( pln );
					continue;
					}
				}
			pln->pprev = pl;
			pln->pnext = pl->pnext;
			if( pln->pnext != NULL )
				lines_reorder++;
			pl->pnext = pln;
			}
		else
			pfirst = pln;
		*ppl = pln;
		}
	/* lines = line-1L; can't do this, some lines aren't added */
		/* last line wan't read, it was EOF => "lines" has NUMBER of lines */
	fclose( fp );
	printf( "Read all %li lines of source IPv4-to-country data file.\n", lines );
	printf( "%li lines had to be reordered.\n", lines_reorder );
	printf( "%li overlapped IP ranges were fixed as possible (%li lines were deleted).\n", lines_overlap, lines_overlap_del );
	if( pfirst == NULL  ||  plast == NULL )
		{
		fputs( "Nothing to do.\n", stderr );
		return RV_ERROR;
		}

	/* Verify adjacent redundant lines
	*/
	puts( "Finding redundancy and ranges..." );
	lines_saved = lines_added = 0L;
	for( pl = pfirst;  pl;  pl = pl->pnext )
		{
		while( (pln=pl->pnext) != NULL  &&  (pl->ip_end + (unsigned32) 1U) == pln->ip_start  &&  pl->cc == pln->cc )
			{
			lines_saved++;
			pl->ip_end = pln->ip_end;
			pl->pnext = pln->pnext;
			if( pl->pnext != NULL )
				pl->pnext->pprev = pl;
			else
				plast = pl;
			free( pln );
			}
		range = pl->ip_end - pl->ip_start + 1UL;
		for( i = 0;  (range & 1UL) == 0UL;  i++ )
			range >>= 1;
		range <<= ( i & RANGE_LSB_MASK4 );
		i &= ( RANGE_SHIFT_MASK4 >> RANGE_SHIFT_SHIFT4 );
		ip_start = pl->ip_start;
		for(;;)  /*forever*/
			{
			range2 = range;
			i2 = i;
			rmask = RANGE_MASK4;
			while( ((range2-1UL) & ~RANGE_MASK4) != 0UL )
				{
				range2 >>= (RANGE_LSB_MASK4 + 1);
				rmask  <<= (RANGE_LSB_MASK4 + 1);
				i2 +=      (RANGE_LSB_MASK4 + 1);
				}
			pl->node.ccsz = (((unsigned16) pl->cc) << CC_SHIFT4) | (((unsigned16) i2) << RANGE_SHIFT_SHIFT4) | ((unsigned16) range2-1UL);
			range &= ~(rmask | (rmask<<1));  /* make sure pattern 10000 (RANGE_MASK4+1) is fully deleted */
			if( !range )
				{
				if( ip_start + (range2 << i2) - 1U  !=  pl->ip_end )
					{
					free_all();
					fprintf( stderr, "Internal error: bad range; is %lu, should be %lu.\n", (unsigned long int) ip_start+(range2<<i2)-1U, (unsigned long int) pl->ip_end );
					return RV_ERROR;
					}
				break;
				}
			pln = malloc( sizeof(struct s_list) );
			if( pln == NULL )
				{
				free_all();
				fputs( "Not enough memory for new database entry.\n", stderr );
				return RV_ERROR;
				}
			memcpy( pln, pl, sizeof(struct s_list) );
			ip_start = ( pln->node.ip += ( range2 << i2 ) );
			pln->pprev = pl;
			pln->pnext = pl->pnext;
			if( pln->pnext != NULL )
				pln->pnext->pprev = pln;
			else
				plast = pln;
			pl->pnext = pln;
			pl = pln;
			lines_added++;
			}
		}
	lines = lines - lines_saved + lines_added;
	printf( "There were %li redundant lines removed.\n"
		"There were %li entries (lines) added due to database range limitations.\n"
		"Total entries (lines) = %lu\n",
		lines_saved, lines_added, lines );

	/* Verify internal bi-direccional linked list
	*/
	puts( "Verifying internal linked lists..." );
	line = 0L;
	for( pl = pfirst;  pl;  pl = pl->pnext )
		{
		line++;
		pl->ip_start = pl->node.ip;
		pl->ip_end   = pl->node.ip + ( ((unsigned32) (pl->node.ccsz & RANGE_MASK4) + (unsigned32) 1U) << ((pl->node.ccsz & RANGE_SHIFT_MASK4) >> RANGE_SHIFT_SHIFT4) ) - 1U;
		if( (pl->pnext != NULL  &&  pl->pnext->node.ip <= pl->ip_end) )
			{
			free_all();
			fprintf( stderr, "Internal error: list entry %lu range overlap by %lu IPs.\n", line, (unsigned long int) pl->ip_end - pl->pnext->node.ip - 1U );
			return RV_ERROR;
			}
		}
	if( line != lines )
		{
		free_all();
		fprintf( stderr, "Internal error: forward linked list is %lu, not as expected (%lu).\n", line, lines );
		return RV_ERROR;
		}
	line = 0L;
	for( pl = plast;  pl;  pl = pl->pprev )
		line++;
	if( line != lines )
		{
		free_all();
		fprintf( stderr, "Internal error: backward linked list is %lu, not as expected (%lu).\n", line, lines );
		return RV_ERROR;
		}

	/* Build tree
	*/
	puts( "Building balanced binary tree..." );
	treetop = treenode( plast, pfirst, lines, 0, NULL );
	printf( "There are %i levels in the tree.\n", treelevel_max );

	/* Verify tree
	*/
	puts( "Verifying balanced binary tree..." );
	if( treelevel_max < treelevel_min  ||  treelevel_max-treelevel_min > 1 )
		{
		free_all();
		fputs( "Internal error: tree leafs are more than one level appart!\n", stderr );
		return RV_ERROR;
		}
	for( pl = pfirst;  pl;  pl = pl->pnext )
		{
		if( pl->treelevel < 0 )
			{
			free_all();
			fputs( "Internal error: some of the list was not turned into a tree node!\n", stderr );
			return RV_ERROR;
			}
		}

	/* Creating clusters
	*/
	puts( "Creating clusters and cluster indexes..." );
	treecluster( treetop, 0L, 0, 0 );
	/* now renumber the clusters to make clusters closer to the top
	   of the tree (and at the same level range, those closer to the right)
	   have smaller numbers, and hence be closer to the start of the
	   database file */
	line = -1L;  /* "line" = first cluster not full of nodes; -1 if none yet */
	cluster = -1L;
	for( levelmin = 0, levelmax=TREELEVELS_PER_CLUSTER4-1;
	     levelmin <= treelevel_max;
	     (levelmin += TREELEVELS_PER_CLUSTER4), (levelmax += TREELEVELS_PER_CLUSTER4) )
		{
		cluster_old = 0L;  /* "none" */
		cc = 0;
		for( pl = plast;  pl;  pl = pl->pprev )
			{
			if( pl->treelevel < levelmin  ||  levelmax < pl->treelevel )
				continue;
			if( cluster_old == 0L  ||  pl->cluster > cluster_old )
				{
				if( cluster_old != 0L  &&  cc != NODES_PER_CLUSTER4 )
					{
					if( levelmax < treelevel_max-1  ||  cc > NODES_PER_CLUSTER4 )
						{
						free_all();
						fputs( "Internal error: clusters not of expected number/size!\n", stderr );
						return RV_ERROR;
						}
					if( line < 0L )
						line = cluster;
					}
				cluster_old = pl->cluster;
				cluster++;
				cc = 0;
				}
			if( pl->cluster < cluster_old )
				{
				cluster = pl->cluster;
				free_all();
				fprintf( stderr, "Internal error: cluster number %li not smaller than %li, as expected!\n", cluster, cluster_old );
				return RV_ERROR;
				}
			pl->cluster = cluster;
			cc++;
			}
		}
	clusters = cluster + 1L;
	if( line < 0L )
		line = clusters;  /* one more than the last cluster number */
	printf( "There are %lu clusters in the database file.\n", clusters );

	/* Verifying clusters
	*/
	puts( "Verifying clusters..." );
	for( pl = pfirst;  pl;  pl = pl->pnext )
		{
		if( pl->cluster < 0L )
			{
			free_all();
			fputs( "Internal error: some of the tree was not clustered!\n", stderr );
			return RV_ERROR;
			}
		if( pl->i < 0L  ||  pl->i >= NODES_PER_CLUSTER4 )
			{
			free_all();
			fputs( "Internal error: cluster's 'i' index is unset or out of range!\n", stderr );
			return RV_ERROR;
			}
		}

	/* Creating target file
	*/
	puts( "Creating target database..." );
	ps = argv[1] != NULL ? argv[1] : DBFILE4;
	fp = fopen( ps, "wb" );
	if( fp == NULL )
		{
		free_all();
		fprintf( stderr, "Cannot create new empty IPv4-to-country database (%s).\n", ps );
		return RV_ERROR;
		}
	for( cluster = 0L;  cluster < clusters;  cluster++ )
		{
		if( cluster > 0L  &&  cluster % 100L == 0L )
			printf( "Written %li clusters so far...\n", cluster );
		/* mark entire cluster for "leaf nodes" */
		for( i = 0;  i < NODES_PER_CLUSTER4;  i++ )
			{
			sector4.cluster4.nodes[i].ip   = (unsigned32) 0xFFFFFFFFU;
			sector4.cluster4.nodes[i].ccsz = (unsigned16) 0xFFFFU;
			sector4.cluster4.next[i]       = (unsigned16) 0x0000U;
			}
		sector4.cluster4.next[i] = (unsigned16) 0x0000U;  /* next[] has one more element */
		cc = 0;
		for( pl = pfirst;  pl;  pl = pl->pnext )
			{
			if( pl->cluster == cluster )
				{
				i = pl->i;
				if( sector4.cluster4.nodes[i].ip != (unsigned32) 0xFFFFFFFFU )
					{
					free_all();
					fclose( fp );
					fprintf( stderr, "Internal error: cluster %li has more than one 'i' index with same value!\n", cluster );
					return RV_ERROR;
					}
				sector4.cluster4.nodes[i].ip   = pl->node.ip;
				sector4.cluster4.nodes[i].ccsz = pl->node.ccsz;
				if( (i & 1) == 0 )
					{
					if( pl->treeleft != NULL )
						sector4.cluster4.next[i]   = pl->treeleft->cluster;
					if( pl->treeright != NULL )
						sector4.cluster4.next[i+1] = pl->treeright->cluster;
					}
				cc++;
				}
			}
		for( i = 0;  i < NODES_PER_CLUSTER4+1; i++ )
			{
			if( sector4.cluster4.next[i] != 0  &&  sector4.cluster4.next[i] <= cluster )
				{
				free_all();
				fclose( fp );
				fprintf( stderr, "Internal error: cluster %li has 'next[]' pointers that loop back!\n", cluster );
				return RV_ERROR;
				}
			}
		if( cluster < line  &&  cc != NODES_PER_CLUSTER4 )
			{
			free_all();
			fclose( fp );
			fprintf( stderr, "Internal error: cluster %li was not filled with all its nodes!\n", cluster );
			return RV_ERROR;
			}
		if( fwrite(&sector4, SECTOR_SIZE, 1, fp) != 1 )
			{
			free_all();
			fclose( fp );
			fputs( "Error writing to database file.\n", stderr );
			return RV_ERROR;
			}
		}

	/* Done
	*/
	fclose( fp );
	free_all();
	puts( "All done!" );
	return RV_OK;
}


/* Creates a balanced tree from the sorted list read from the file.
   One of "pright" or "pleft" can be NULL, meaning there are no blocks going that way
   (i.e., you should count blocks on the pointer NOT null); "entries" states how many
   entries or "lines" there are pointed to by one of the pointers, and "level" has the
   current tree level (0 for root node, 1 for its two descendants, etc.).
   Returns the root node for the block being provided.
*/
struct s_list *treenode( struct s_list *pleft, struct s_list *pright,
			 long int entries, int level, long int *pnumnodes )
{
	struct s_list *pl;
	long int i, eleft, eright, nodesleft, nodesright;

	if( entries == 0L  ||  (pleft == NULL  &&  pright == NULL ) )
		{
		if( level < treelevel_min )
			treelevel_min = level;
		if( treelevel_max < level )
			treelevel_max = level;
		return NULL;
		}
	if( entries < 0L )
		{
		fputs( "Internal error: incorrect number of entries!\n", stderr );
		return NULL;
		}
	i = (entries >> 1) - ((entries & 1L) ^ 1L);
		/* rounding down makes nodes gather closer to the middle of
		   the IP range, which is fine as the edges have special
		   meanings */
	if( pleft != NULL )
		{
		/* "counting by the fingers" the "eleft" and "eright" vars
		   here and bellow makes sure this is always accurate, despite
		   the precise expression we use to calculate "i" */
		eleft  = entries - 1L;
		eright = 0L;
		for( pl = pleft;  i--;  pl = pl->pprev )
			{
			if( pl == NULL )
				{
				fputs( "Internal error: found unexpected tree leaf!\n", stderr );
				return NULL;
				}
			eleft--;
			eright++;
			}
		}
	else /* pright != NULL */
		{
		eleft  = 0L;
		eright = entries - 1L;
		for( pl = pright;  i--;  pl = pl->pnext )
			{
			if( pl == NULL )
				{
				fputs( "Internal error: found unexpected tree leaf!\n", stderr );
				return NULL;
				}
			eleft++;
			eright--;
			}
		}
	if( pl->treelevel >= 0 )
		{
		fputs( "Internal error: re-visited a tree node!\n", stderr );
		return NULL;
		}
	nodesleft = nodesright = 0L;
	pl->treelevel = level;
	pl->treeleft  = treenode( pl->pprev, NULL, eleft,  level+1, &nodesleft  );
	pl->treeright = treenode( NULL, pl->pnext, eright, level+1, &nodesright );
	if( (nodesleft < nodesright  &&  nodesright-nodesleft > 1L)  ||
	    (nodesright < nodesleft  &&  nodesleft-nodesright > 1L) )
		{
		fputs( "Internal error: tree is not balanced!\n", stderr );
		return NULL;
		}
	if( pnumnodes != NULL )
		(*pnumnodes)++;
	return pl;
}


/* Sets "cluster" number and "i" index number to use in clusters,
   for each node.
   On first call, use 0L for "cluster", and whichever values you see fit
   for "i" and "step". This fill an the tree's "cluster" and "i" values;
   "cluster" values need to be renumbered later for optimization, as it
   would be too hard for a recursive algorithm to make the numbering
   we want.
*/
void treecluster( struct s_list *pnode, long int cluster, int i, int step )
{
	static long int next_cluster = -2L;  /* grows backwards from -2L */

	if( pnode == NULL )
		return;
	if( pnode->treelevel % TREELEVELS_PER_CLUSTER4 == 0 )
		{
		/* this is root node of a cluster */
		cluster = next_cluster--;
		i = NODES_PER_CLUSTER4 >> 1;
		step = (NODES_PER_CLUSTER4 >> 2) + 1;
		}
	if( step <= 0  &&
	    ((pnode->treeleft  != NULL  &&  pnode->treeleft->cluster  == cluster)  ||
	     (pnode->treeright != NULL  &&  pnode->treeright->cluster == cluster)) )
		{
		fprintf( stderr, "Internal error: on cluster %li, step reached zero!\n", cluster );
		return;
		}
	if( pnode->cluster != -1L  ||  pnode->i >= 0 )
		{
		fputs( "Internal error: re-visited a tree node!\n", stderr );
		return;
		}
	pnode->cluster = cluster;
	pnode->i = i;
	treecluster( pnode->treeleft,  cluster, i - step, step >> 1 );
	treecluster( pnode->treeright, cluster, i + step, step >> 1 );
}


/* Releases memory from all nodes in memory
   and empties list pointers
*/
void free_all( void )
{
	struct s_list *pl, *pln;

	for( pl = pfirst;  pl;  pl = pln )
		{
		pln = pl->pnext;
		free( pl );
		}
	pfirst = plast = treetop = NULL;
}
