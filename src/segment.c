#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <alloca.h>
#include <inttypes.h>
#include <memory.h>
#include <assert.h>
#include <regex.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#include "segment.h"

/**
 * Given a regex, look at process pid's mapped pages to find a matching executable unit. For the first that is found,
 * fill in oname, paddr, paddr2, and offset with its object name.
 */
long find_segment_address_regex (int pid, const char* objectpath_regex, char* oname, int noname,
																 unsigned long* paddr, unsigned long* paddr2, unsigned long* poffset)
{
	const int strsize = 512;
	char path[strsize];

	regex_t re;
	if (regcomp(&re, objectpath_regex, REG_EXTENDED)) {
		return 1;
	}

	snprintf(path, strsize, "/proc/%d/maps", pid);
	FILE* fmap = fopen(path, "r");
	if (fmap == NULL) {
		return 2;
	}

	char* r;
	*paddr = 0;
	do {
		unsigned long saddr, saddr2;
		unsigned long offs;
		char privs[6];
		char objname[strsize];
		char line[strsize];
		int n=0;
		r = fgets(line, strsize, fmap);
		n = sscanf(line, "%lx - %lx %4s %lx %*x:%*x %*d %s\n", &saddr, &saddr2, privs, &offs, objname);
		if (n == 2) continue;
		if (!regexec(&re,objname,(size_t)0,NULL,0) && strchr(privs, 'x')) {
			*paddr = saddr;
			if (paddr2) *paddr2 = saddr2;
			if (poffset) *poffset = offs;
			printf("Found segment %s at 0x%lx.\n", objname, *paddr);
			strncpy(oname, objname, noname);
			break;
		}
	} while (r != NULL);

	if (*paddr == 0) {
		printf("No segment found.\n");
	}
	
	fclose(fmap);
	regfree(&re);
	
	return 0;
	
}


/**
 * Given symbol_regex, find the first occurrence of a matching symbol in the file object specified by unitpath
 * and fill the fields sname and poffset with its symbolname and offset, respectively.
 */
long find_symbol_offset_regex (const char* objectpath, const char* symbol_regex, char* sname, int nsname,
															 unsigned long* poffset)
{
	const int strsize = 512;
	char readelf_cmd[strsize];
	char line[strsize];
	regex_t re_symbol;
	FILE* fhre;
	int n;
	char* r;
	
	if (regcomp(&re_symbol, symbol_regex, REG_EXTENDED)) {
		return 1;
	}
	
	snprintf(readelf_cmd, strsize, "readelf -s %s", objectpath);
	printf("# %s\n", readelf_cmd);
	fhre = popen(readelf_cmd, "r");
	do {
		uint64_t sofs;
		char symname[strsize];
		r = fgets(line, strsize, fhre);
		n = sscanf(line, "%*d: %lx %*d %*s %*s %*s %*s %s", &sofs, symname);
		/*
		if (n==2) {
			printf("#%s", line);
		} else {
			printf(line);
		} 
		*/
		if (!regexec(&re_symbol,symname,(size_t)0,NULL,0)) {
			strncpy(sname, symname, nsname);
			*poffset = sofs;
			printf("Found symbol %s offset at 0x%lx.\n", symname, *poffset);
			break;
		}
	} while (r != NULL);
	
	pclose(fhre);
	regfree(&re_symbol);
	return 0;
	
}
