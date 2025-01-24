
/*
ip2cc-countries.h
ANSI C
(C) 2003 CYNERGI, Pedro Freire
*/

#ifndef _IP2CC_COUNTRIES_H_
#define _IP2CC_COUNTRIES_H_

#include <ctype.h>

const char *cname_up[] = {
	"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AQ", "AR",
	"AS", "AT", "AU", "AW", "AZ", "BA", "BB", "BD", "BE", "BF", "BG",
	"BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT", "BV", "BW",
	"BY", "BZ", "CA", "CC", "CD", "CF", "CG", "CH", "CI", "CK", "CL",
	"CM", "CN", "CO", "CR", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ",
	"DK", "DM", "DO", "DZ", "EC", "EE", "EG", "EH", "ER", "ES", "ET",
	"FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF",
	"GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU",
	"GW", "GY", "HK", "HM", "HN", "HR", "HT", "HU", "ID", "IE", "IL",
	"IN", "IO", "IQ", "IR", "IS", "IT", "JM", "JO", "JP", "KE", "KG",
	"KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB",
	"LC", "LI", "LK", "LR", "LS", "LT", "LU", "LV", "LY", "MA", "MC",
	"MD", "MG", "MH", "MK", "ML", "MM", "MN", "MO", "MP", "MQ", "MR",
	"MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE",
	"NF", "NG", "NI", "NL", "NO", "NP", "NR", "NU", "NZ", "OM", "PA",
	"PE", "PF", "PG", "PH", "PK", "PL", "PM", "PN", "PR", "PS", "PT",
	"PW", "PY", "QA", "RE", "RO", "RU", "RW", "SA", "SB", "SC", "SD",
	"SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR",
	"ST", "SV", "SY", "SZ", "TC", "TD", "TF", "TG", "TH", "TJ", "TK",
	"TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA", "UG",
	"UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU",
	"WF", "WS", "YE", "YT", "YU", "ZA", "ZM", "ZW"
	/* TODO: "RS", "ME"  /+ Serbia and Montenegro, 2006 */ };

const char *cname_low[] = {
	"ad", "ae", "af", "ag", "ai", "al", "am", "an", "ao", "aq", "ar",
	"as", "at", "au", "aw", "az", "ba", "bb", "bd", "be", "bf", "bg",
	"bh", "bi", "bj", "bm", "bn", "bo", "br", "bs", "bt", "bv", "bw",
	"by", "bz", "ca", "cc", "cd", "cf", "cg", "ch", "ci", "ck", "cl",
	"cm", "cn", "co", "cr", "cu", "cv", "cx", "cy", "cz", "de", "dj",
	"dk", "dm", "do", "dz", "ec", "ee", "eg", "eh", "er", "es", "et",
	"fi", "fj", "fk", "fm", "fo", "fr", "ga", "gb", "gd", "ge", "gf",
	"gh", "gi", "gl", "gm", "gn", "gp", "gq", "gr", "gs", "gt", "gu",
	"gw", "gy", "hk", "hm", "hn", "hr", "ht", "hu", "id", "ie", "il",
	"in", "io", "iq", "ir", "is", "it", "jm", "jo", "jp", "ke", "kg",
	"kh", "ki", "km", "kn", "kp", "kr", "kw", "ky", "kz", "la", "lb",
	"lc", "li", "lk", "lr", "ls", "lt", "lu", "lv", "ly", "ma", "mc",
	"md", "mg", "mh", "mk", "ml", "mm", "mn", "mo", "mp", "mq", "mr",
	"ms", "mt", "mu", "mv", "mw", "mx", "my", "mz", "na", "nc", "ne",
	"nf", "ng", "ni", "nl", "no", "np", "nr", "nu", "nz", "om", "pa",
	"pe", "pf", "pg", "ph", "pk", "pl", "pm", "pn", "pr", "ps", "pt",
	"pw", "py", "qa", "re", "ro", "ru", "rw", "sa", "sb", "sc", "sd",
	"se", "sg", "sh", "si", "sj", "sk", "sl", "sm", "sn", "so", "sr",
	"st", "sv", "sy", "sz", "tc", "td", "tf", "tg", "th", "tj", "tk",
	"tl", "tm", "tn", "to", "tr", "tt", "tv", "tw", "tz", "ua", "ug",
	"um", "us", "uy", "uz", "va", "vc", "ve", "vg", "vi", "vn", "vu",
	"wf", "ws", "ye", "yt", "yu", "za", "zm", "zw"
	/* TODO: "rs", "me"  /+ Serbia and Montenegro, 2006 */ };


#define CNAME_SIZE  ( sizeof(cname_low) / sizeof(cname_low[0]) )


/* Returns the country code of "ccstr",
   or -1 if not found
*/
int find_cc( const char *ccstr )
{
	char cc1, cc2, c;
	const char *ps;
	int i, step, count;

	cc1 = tolower( *ccstr++ );
	if( !cc1 )
		return -1;  /* error */
	cc2 = tolower( *ccstr++ );
	if( !cc2  ||  *ccstr )
		return -1;  /* error */
	i    = CNAME_SIZE / 2;
	step = CNAME_SIZE / 4;
	count = 3;  /* base number to make sure all items are found despite
		       i's and step's rounding errors */
	do	{
		ps = cname_low[i];
		if( cc1 == (c=*ps) )
			{
			if( cc2 == (c=*(ps+1)) )
				return i;
			if( cc2 < c )
				{
				i -= step;
				if( i < 0 )
					i = 0;
				}
			else
				{
				i += step;
				if( i >= CNAME_SIZE )
					i = CNAME_SIZE-1;
				}
			}
		else if( cc1 < c )
			{
			i -= step;
			if( i < 0 )
				i = 0;
			}
		else
			{
			i += step;
			if( i >= CNAME_SIZE )
				i = CNAME_SIZE-1;
			}
		if( !(step >>= 1) )
			{
			step = 1;
			count--;
			}
		else
			count++;
		}
		while( count );
	return -1;  /* not found */
}

#endif  /* _IP2CC_COUNTRIES_H_ */
