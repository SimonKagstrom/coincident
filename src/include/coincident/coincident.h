#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Add a thread to coincident.
 *
 * @param fn the function to execute
 * @param priv the private data to pass to the function
 *
 * @return the thread ID (0..n) if the operation was OK, -1 otherwise
 */
extern int coincident_add_thread(int (*fn)(void *), void *priv);


/**
 * Set the number of runs
 *
 * @param n_runs the number of runs
 */
extern void coincident_set_run_limit(int n_runs);

/**
 * Set the time limit (instead of number of runs)
 *
 * @param n_ms the time limit in milliseconds
 */
extern void coincident_set_time_limit(int n_ms);


/**
 * Start coincident!
 *
 * @return 0 if all runs went OK, the exit code otherwise
 */
extern int coincident_run(void);

#if defined(__cplusplus)
};
#endif
