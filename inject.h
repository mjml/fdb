#ifndef _INJECT_H
#define _INJECT_H

/**
 * Synchronously:
 *    Save remote registers
 *    Read the current code at the remote IP
 *    Poketext a string of text (code) into the process
    -- dangerous to do this for a running process since it could be inside 
    -- a system call that is inside a shared library; 
       could stop multiple processes 
 *    Resume the process and wait for the blob to finish
 *    Poketext the old code into the former remote IP
 *    Restore registers
 *    (Do not PTRACE_CONT the target process)
 */

int inject_and_run_text (int pid, int size, const uint8_t* text);


#endif
