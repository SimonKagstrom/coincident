#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Initialize coincident.
 *
 * Not strictly necessary, but if you have a forking test environment (such
 * as crpcut), doing this at startup avoids re-reading the ELF symbols
 * for every forked child.
 */
extern void coincident_init(void);

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
 * Setup a bucket thread selector
 *
 * Bucket thread selectors run a thread for a number of mem refs based on the
 * current bucket. It then switches thread and continues with the next bucket.
 *
 * @param buckets a vector of the buckets
 * @param n_buckets the number of buckets
 */
extern void coincident_set_bucket_selector(int *buckets, unsigned int n_buckets);


/**
 * Start coincident!
 *
 * @return 0 if all runs went OK, the exit code otherwise
 */
extern int coincident_run(void);

#if defined(__cplusplus)
};
#endif
