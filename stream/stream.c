/* stream.c
 *
 * DESCRIPTION
 *
 *     This is an implementation of the STREAM benchmark as proposed by McCalpin
 *     (1995).
 *
 *     The benchmark requires different amounts of memory on different systems.
 *     In general, each array operand should be at least 4 x times the size of the
 *     largest available cache memory. (L3 on most modern systems.) There is an
 *     option to print the memory ratio to aid in configuring benchmark memory
 *     requirements.
 */

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <sys/time.h>
#include <omp.h>

#include "common.h"

static real_t a[STREAM_ARRAY_SIZE];
static real_t b[STREAM_ARRAY_SIZE];
static real_t c[STREAM_ARRAY_SIZE];

int L3_SIZE = 0;

/* MIN, MAX, AVG */
static double timings[4][3] = {
    [COPY]  = { FLT_MAX, 0.0, 0.0 },
    [SCALE] = { FLT_MAX, 0.0, 0.0 },
    [ADD]   = { FLT_MAX, 0.0, 0.0 },
    [TRIAD] = { FLT_MAX, 0.0, 0.0 }
};

long BYTES_COPY  = 0;
long BYTES_SCALE = 0;
long BYTES_ADD   = 0;
long BYTES_TRIAD = 0;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void configure_memory(void)
{
    if (!getenv("L3_SIZE"))
    {
        printf("[ERROR] L3_SIZE not set\n");
        exit(1);
    }

    L3_SIZE = atoi(getenv("L3_SIZE")) * 1024;

    BYTES_COPY  = 2 * sizeof(real_t) * STREAM_ARRAY_SIZE;
    BYTES_SCALE = 2 * sizeof(real_t) * STREAM_ARRAY_SIZE;
    BYTES_ADD   = 3 * sizeof(real_t) * STREAM_ARRAY_SIZE;
    BYTES_TRIAD = 3 * sizeof(real_t) * STREAM_ARRAY_SIZE;
}

double walltime(void)
{
    static struct timeval t;
    gettimeofday(&t, NULL);
    return ((double)t.tv_sec + (double)t.tv_usec * 1.0E-6);
}

int clock_ticks()
{
    const int M = 20;

    double  t1, t2, time_value[M];

    /* Collect a sequence of 'M' unique time values from the system */
    for (int i = 0; i < M; i++)
    {
        t1 = walltime();
        while(((t2 = walltime()) - t1) < 1.0E-6);
        time_value[i] = t1 = t2;
    }

    /* Determine the minimum difference between these 'M' values. This result
       will be the estimate (in microseconds) for the clock granularity. */
    int min_delta = 1000000;
    for (int i = 1; i < M; i++)
    {
        int delta = (int)(1.0E6 * (time_value[i] - time_value[i - 1]));
        min_delta = MIN(min_delta, MAX(delta, 0));
    }

   return min_delta;
}

void run_stream(void)
{
    double t_start, t_latency;

    #pragma omp parallel for
    for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
    {
	    a[j] = 1.0;
	    b[j] = 2.0;
	    c[j] = 0.0;
	}

    #pragma omp parallel for
    for (int j = 0; j < STREAM_ARRAY_SIZE; j++)
		a[j] = 2.0 * a[j];

    real_t scalar = 3.0;

    for (int j = 0; j < NUM_BENCH; j++)
    {
        /* COPY */
        t_start = walltime();
        #pragma omp parallel for
        for (int i = 0; i < STREAM_ARRAY_SIZE; i++)
            c[i] = a[i];
        t_latency = walltime() - t_start;
        if (j > 0)
        {
            timings[COPY][MIN] = MIN(timings[COPY][MIN], t_latency);
            timings[COPY][MAX] = MAX(timings[COPY][MAX], t_latency);
            timings[COPY][AVG] += t_latency;
        }

        /* SCALE */
        t_start = walltime();
        #pragma omp parallel for
        for (int i = 0; i < STREAM_ARRAY_SIZE; i++)
            b[i] = scalar * c[i];
        t_latency = walltime() - t_start;
        if (j > 0)
        {
            timings[SCALE][MIN] = MIN(timings[SCALE][MIN], t_latency);
            timings[SCALE][MAX] = MAX(timings[SCALE][MAX], t_latency);
            timings[SCALE][AVG] += t_latency;
        }

        /* ADD */
        t_start = walltime();
        #pragma omp parallel for
        for (int i = 0; i < STREAM_ARRAY_SIZE; i++)
            c[i] = a[i] + b[i];
        t_latency = walltime() - t_start;
        if (j > 0)
        {
            timings[ADD][MIN] = MIN(timings[ADD][MIN], t_latency);
            timings[ADD][MAX] = MAX(timings[ADD][MAX], t_latency);
            timings[ADD][AVG] += t_latency;
        }

        /* TRIAD */
        t_start = walltime();
        #pragma omp parallel for
        for (int i = 0; i < STREAM_ARRAY_SIZE; i++)
            a[i] = b[i] + scalar * c[i];
        t_latency = walltime() - t_start;
        if (j > 0)
        {
            timings[TRIAD][MIN] = MIN(timings[TRIAD][MIN], t_latency);
            timings[TRIAD][MAX] = MAX(timings[TRIAD][MAX], t_latency);
            timings[TRIAD][AVG] += t_latency;
        }
    }

    for (int i = 0; i < 4; i++)
        timings[i][AVG] /= ((int)NUM_BENCH - 1);

    int threads = 0;
    #pragma omp parallel
    {
        #pragma omp master
        threads = omp_get_num_threads();
    }

    printf("%d %.1lf %.1lf %.1lf [GB/s]\n",
        threads,
        (BYTES_TRIAD * 1.0E-9) / timings[TRIAD][MAX],
        (BYTES_TRIAD * 1.0E-9) / timings[TRIAD][MIN],
        (BYTES_TRIAD * 1.0E-9) / timings[TRIAD][AVG]
    );
}

int main(void)
{
    configure_memory();

    run_stream();

    return 0;
}
