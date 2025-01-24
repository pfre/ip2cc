
/*
ip2cc.h
ANSI C
WIN32-aware for default folders
GNU C Compiler-aware, for packed structures
recognizes POSIX.1 macro constant POSIX_REC_MIN_XFER_SIZE, if present
(C) 2003 Corebase, Easymatic, Cynergi, Pedro Freire

See comments at the top of ip2cc.c for more information.
*/


#ifndef _CCLANG_H_
#define _CCLANG_H_


#include <limits.h>
#include <stdio.h>


/* Compiler-specific "packed structure" attribute
   (using compiler-wide command-line options may break
   existing libraries' structures!)
   PACK_ATTR1 should precede the "struct" keyword;
   PACK_ATTR2 should follow the struct's closing brace,
   but precede the var name.
   You may define these as macros on the compiler's
   command-line.
*/
#if !defined(PACK_ATTR1)  &&  !defined(PACK_ATTR2)  &&  defined(__GNUC__)
#define PACK_ATTR1
#define PACK_ATTR2		__attribute__ ((__packed__))
#endif
#ifndef PACK_ATTR1
#define PACK_ATTR1
#endif
#ifndef PACK_ATTR2
#define PACK_ATTR2
#endif


/* 16-bit unsigned integer type
*/
#if USHRT_MAX == 0xFFFFU
typedef unsigned short int	unsigned16;
#elif UINT_MAX == 0xFFFFU
typedef unsigned int		unsigned16;
#elif ULONG_MAX == 0xFFFFUL
typedef unsigned long int	unsigned16;
#else
#error "Cannot find a 16-bit integer type"
#endif


/* 32-bit unsigned integer type
*/
#if USHRT_MAX == 0xFFFFFFFFU
typedef unsigned short int	unsigned32;
#elif UINT_MAX == 0xFFFFFFFFU
typedef unsigned int		unsigned32;
#elif ULONG_MAX == 0xFFFFFFFFUL
typedef unsigned long int	unsigned32;
#else
#error "Cannot find a 32-bit integer type"
#endif


/* Optimal disk reading block size
*/
#ifndef SECTOR_SIZE
#ifndef POSIX_REC_MIN_XFER_SIZE
#define SECTOR_SIZE		BUFSIZ  /* defined in <stdio.h> */
#else
#define SECTOR_SIZE		POSIX_REC_MIN_XFER_SIZE
#endif
#endif  /* SECTOR_SIZE */


/* Shift left positions to achieve multiplication by SECTOR_SIZE
   SECTOR_SIZE < 64 this would make NODES_PER_CLUSTER6 smaller than 1...
*/
#if     SECTOR_SIZE == 64
#define SECTOR_SIZE_SHIFT	6
#elif   SECTOR_SIZE == 128
#define SECTOR_SIZE_SHIFT	7
#elif   SECTOR_SIZE == 256
#define SECTOR_SIZE_SHIFT	8
#elif   SECTOR_SIZE == 512
#define SECTOR_SIZE_SHIFT	9
#elif   SECTOR_SIZE == 1024
#define SECTOR_SIZE_SHIFT	10
#elif   SECTOR_SIZE == 2048
#define SECTOR_SIZE_SHIFT	11
#elif   SECTOR_SIZE == 4096
#define SECTOR_SIZE_SHIFT	12
#elif   SECTOR_SIZE == 8192
#define SECTOR_SIZE_SHIFT	13
#elif   SECTOR_SIZE == 16384
#define SECTOR_SIZE_SHIFT	14
#elif   SECTOR_SIZE == 32768
#define SECTOR_SIZE_SHIFT	15
#elif   SECTOR_SIZE == 65536
#define SECTOR_SIZE_SHIFT	16
#else  /* not power of 2 in this range */
#undef  SECTOR_SIZE
#define SECTOR_SIZE		512
#define SECTOR_SIZE_SHIFT	9
#endif


/* Given SECTOR_SIZE which will be the maximum size for a cluster,
   how many nodes can we fit into a cluster?
*/
#define NODES_PER_CLUSTER4	((SECTOR_SIZE >> 3) - 1)  /* for IPv4 */
#define NODES_PER_CLUSTER6	((SECTOR_SIZE >> 4) - 1)  /* for IPv6 */


/* How many tree levels does that correspond to?
*/
#define TREELEVELS_PER_CLUSTER4	(SECTOR_SIZE_SHIFT - 3)  /* for IPv4 */
#define TREELEVELS_PER_CLUSTER6	(SECTOR_SIZE_SHIFT - 4)  /* for IPv6 */


/* What is the actual cluster size (used part of SECTOR_SIZE)?
*/
#define CLUSTER4_SIZE		sizeof(struct s_cluster4)
#define CLUSTER6_SIZE		sizeof(struct s_cluster6)


/* Verious masks and shift counts used to extract information from
   each cluster's nodes
*/
#define RANGE_MASK4		((unsigned16) 0x000F)  /* mask to leave out IP range */
#define LEAF_MASK6		((unsigned16) 0x0040)  /* mask to leave out leaf flag */
#define RANGE_SHIFT_MASK4	((unsigned16) 0x0070)  /* mask to leave out IP range shift */
#define RANGE_SHIFT_MASK6	((unsigned16) 0x003F)  /* mask to leave out IP range shift */
#define RANGE_SHIFT_SHIFT4	2  /* bits to shift right to get just the IP range shift */
#define CC_MASK4		((unsigned16) 0xFF80)  /* mask to leave out country code */
#define CC_MASK6		((unsigned16) 0xFF80)  /* mask to leave out country code */
#define CC_SHIFT4		7  /* bits to shift right to get just the country code */
#define CC_SHIFT6		7  /* bits to shift right to get just the country code */


/* Database filenames
*/
#ifdef WIN32
#define DBFILE4			"C:\\esx\\data\\ip4.db"
#define DBFILE6			"C:\\esx\\data\\ip6.db"
#else
#define DBFILE4			"/esx/data/ip4.db"
#define DBFILE6			"/esx/data/ip6.db"
#endif


/* Actual data structure for an IPv4 cluster
*/
PACK_ATTR1 struct s_cluster4
	{
	PACK_ATTR1 struct s_node4
		{
		unsigned32 ip;		/* IPv4 network address (all 1s for none (leaf)) */
		unsigned16 ccsz;	/* ISO2 country-code and IP range size */
			/* 9 bits (15-7): ISO2 country-code
			   3 bits  (6-4): amount to shift left the 4-bit range size, div by 4
			   4 bits  (3-0): range size minus 1 */
		}
		PACK_ATTR2 nodes[NODES_PER_CLUSTER4];
	unsigned16
	next[NODES_PER_CLUSTER4+1];	/* next cluster index for branch; 0x0000 if none (leaf) */
	} PACK_ATTR2;


/* Actual data structure for an IPv6 cluster
*/
PACK_ATTR1 struct s_cluster6
	{
	PACK_ATTR1 struct s_node6
		{
		unsigned32 ip[2];	/* upper 64 bits of IPv6 network address (lower 64 bits are MAC address) */
		unsigned16 iprange;	/* range size minus 1 */
		unsigned16 ccsz;	/* ISO2 country-code and IP range size */
			/* 9 bits (15-7): ISO2 country-code
			   1 bit     (6): 1 (true) if this a leaf
			   6 bits  (5-0): amount to shift left the 16-bit range size */
		}
		PACK_ATTR2 nodes[NODES_PER_CLUSTER6];
	unsigned32
	next[NODES_PER_CLUSTER6+1];	/* next cluster index for branch; 0x00000000 if none (leaf) */
	} PACK_ATTR2;


#endif
