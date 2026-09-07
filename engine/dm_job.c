#include "dm_job.h"

#include <windows.h>
#include <process.h>
#include <string.h>

static struct {
    int          nthreads;
    HANDLE       th[DM_JOB_MAX_THREADS];
    HANDLE       start;      /* released once per worker at each dispatch */
    HANDLE       finished;   /* each worker releases once when its drain ends */
    volatile LONG next;
    volatile LONG quit;
    int          count;
    dm_job_fn    fn;
    void        *ud;
    int          inited;
} J;

/* Pull items off the shared counter until the queue is empty. */
static void dm_job_drain(int thread)
{
    for (;;) {
        LONG i = InterlockedIncrement(&J.next) - 1;
        if (i >= (LONG)J.count) break;
        J.fn((int)i, thread, J.ud);
    }
}

static unsigned __stdcall dm_job_worker(void *arg)
{
    int thread = (int)(intptr_t)arg;
    for (;;) {
        WaitForSingleObject(J.start, INFINITE);
        if (J.quit) break;
        dm_job_drain(thread);
        ReleaseSemaphore(J.finished, 1, NULL);
    }
    return 0;
}

void dm_job_init(int nthreads)
{
    if (J.inited) return;

    if (nthreads <= 0) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        nthreads = (int)si.dwNumberOfProcessors;
    }
    if (nthreads > DM_JOB_MAX_THREADS) nthreads = DM_JOB_MAX_THREADS;
    if (nthreads < 1) nthreads = 1;

    J.nthreads = nthreads;
    J.quit     = 0;

    int workers = nthreads - 1;
    LONG cap    = workers > 0 ? workers : 1;
    J.start     = CreateSemaphore(NULL, 0, cap, NULL);
    J.finished  = CreateSemaphore(NULL, 0, cap, NULL);

    for (int i = 0; i < workers; i++)
        J.th[i] = (HANDLE)_beginthreadex(NULL, 0, dm_job_worker,
                                         (void *)(intptr_t)(i + 1), 0, NULL);
    J.inited = 1;
}

int dm_job_threads(void) { return J.inited ? J.nthreads : 1; }

void dm_job_for(int count, dm_job_fn fn, void *ud)
{
    if (count <= 0) return;
    if (!J.inited) dm_job_init(0);

    J.count = count;
    J.fn    = fn;
    J.ud    = ud;
    J.next  = 0;

    int workers = J.nthreads - 1;
    if (workers > 0) ReleaseSemaphore(J.start, workers, NULL);

    dm_job_drain(0);

    for (int i = 0; i < workers; i++)
        WaitForSingleObject(J.finished, INFINITE);
}

void dm_job_shutdown(void)
{
    if (!J.inited) return;

    int workers = J.nthreads - 1;
    J.quit = 1;
    if (workers > 0) {
        ReleaseSemaphore(J.start, workers, NULL);
        WaitForMultipleObjects((DWORD)workers, J.th, TRUE, INFINITE);
        for (int i = 0; i < workers; i++) CloseHandle(J.th[i]);
    }
    CloseHandle(J.start);
    CloseHandle(J.finished);
    memset(&J, 0, sizeof J);
}
