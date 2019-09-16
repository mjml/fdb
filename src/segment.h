#pragma once

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN extern
#endif


EXTERN long find_segment_address_regex (int pid, const char* unitpath_regex, char* oname, int noname,
																				unsigned long* paddr, unsigned long* paddr2, unsigned long* poffset);
EXTERN long find_symbol_offset_regex (const char* unitpath, const char* symbol_regex, char* sname, int nsname,
																			unsigned long* poffset);

