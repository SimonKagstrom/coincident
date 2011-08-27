#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Fork a number of child processes, to run in parallel multiple
 * times.
 *
 * @param nProcesses the number of processes to fork, at least 2
 *
 * @return the process ID from 0..nProcesses or -1 on failure
 */
extern int coincident_fork(int n);


extern int coincident_set_run_limit(int n_runs);


extern int coincident_set_time_limit(int n_ms);

#if defined(__cplusplus)
};
#endif
