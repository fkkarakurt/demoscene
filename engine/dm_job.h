/* dm_job.h -- minimal work-stealing parallel-for over Win32 threads.
 *
 * The renderer is embarrassingly parallel per scanline, but scanlines are not
 * equally expensive (a raymarched horizon costs far more than empty sky), so a
 * static split would leave cores idle. Workers pull the next row index off a
 * shared atomic counter instead, which self-balances for free.
 */
#ifndef DM_JOB_H
#define DM_JOB_H

#include "dm_base.h"

#define DM_JOB_MAX_THREADS 64

/* `index` is the work item, `thread` is 0..dm_job_threads()-1 and is stable for
 * the duration of one dispatch -- use it to index per-thread scratch state. */
typedef void (*dm_job_fn)(int index, int thread, void *ud);

/* nthreads <= 0 means "one per logical processor". Safe to call more than once. */
void dm_job_init(int nthreads);
void dm_job_shutdown(void);
int  dm_job_threads(void);

/* Runs fn(0..count-1) across the pool and returns once all of them finish.
 * The calling thread participates as thread 0 rather than idling. */
void dm_job_for(int count, dm_job_fn fn, void *ud);

#endif /* DM_JOB_H */
