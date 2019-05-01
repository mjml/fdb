#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <regex.h>

regex_t procname_regex;
regex_t pid_regex;

static int initialized=0;

int pid_dir_filter (const struct dirent* de)
{
	if (de->d_type != DT_DIR) return 0;
	// if dir name is an integer return 1
	// else return 0
	int re = regexec(&pid_regex, de->d_name, 0, NULL, 0);
	if (!re)
		return 1;

	return 0;
}

int find_target_pid (const char* pat)
{
	if (!initialized) {
		regcomp(&pid_regex, "[0-9]+", REG_EXTENDED);
	}
	
	int ret = 0;
	int re = regcomp(&procname_regex, pat, REG_EXTENDED);
	if (re) {
		const int MSGBUFSZ = 1024;
		char msg [MSGBUFSZ];
		regerror (re, &procname_regex, msg, MSGBUFSZ);
		fprintf (stderr, "%s", msg);
		exit (2);
	}
	
	// get list of processes and find matches
	struct dirent **namelist;

	int nd = scandir("/proc", &namelist, pid_dir_filter, NULL);
	if (nd == 0) {
		ret = 0;
		goto end;
	}

	for (int i=0; i < nd; i++) {
		char commfn[1024];
		snprintf(commfn, 1024, "/proc/%s/comm", namelist[i]->d_name);
		FILE* commfile = fopen(commfn, "r");
		if (!commfile) {
			continue;
		}
		char comm[1024];
		fscanf(commfile, "%s", comm);
		fclose(commfile);
		int re = regexec(&procname_regex, comm, 0, NULL, 0);
		if (!re) {
			ret = atoi(namelist[i]->d_name);
			break;
		}
	}
	
	// free the regex
 end:
	regfree(&procname_regex);
	return ret;
	
}



