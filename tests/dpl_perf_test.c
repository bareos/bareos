/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include <droplet.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

#include "gendata.h"

char *bucket = NULL;
char *profile = NULL;
int Rflag = 0; //random content
int zflag = 0; //set 'z' in content
int rflag = 0; //use random size
int vflag = 0; //verbose
int oflag = 0; //random oids
int max_block_size = 30000;
int n_ops = 1; //per thread
int n_threads = 10;
int timebase = 1; //seconds
int class = 2;
int print_running_stats = 1;
int print_gnuplot_stats = 0;
int seed = 0;

dpl_ctx_t *ctx = NULL;

#if defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )

struct drand48_data
{
    u_short __x[3];
    u_short __old_x[3];
    u_short __c;
    u_short __init;
    u_int64_t __a;
};
#endif

struct drand48_data drbuffer;

pthread_attr_t g_detached_thread_attr, g_joinable_thread_attr;

static pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_t *workers = NULL;

struct
{
  uint64_t n_failures;
  uint64_t n_success;
  uint64_t n_bytes;
  double latency_sq_sum;
  uint64_t latency_sum;
} stats;

typedef struct
{
  uint32_t id;
} tthreadid;

void test_cleanup(void *arg)
{
  tthreadid *threadid = (tthreadid *)arg;

  if (vflag > 1)
    fprintf(stderr, "cleanup %u\n", threadid->id);
}

/*
 * Test thread
 */

static void *test_main(void *arg)
{
  tthreadid *threadid = (tthreadid *)arg;
  int i, ret;
  int dummy;
  char *block;
  size_t block_size;
  uint64_t oid;

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &dummy);

  pthread_cleanup_push(test_cleanup, arg);

  oid = get_oid(oflag, &drbuffer);

  for (i = 0; i < n_ops;i++)
    {
      struct timeval tv1, tv2;
      char id_str[128];

      pthread_testcancel();

      snprintf(id_str, sizeof (id_str), "%llu", (unsigned long long) oid);

      if (rflag)
        block_size = rand() % max_block_size;
      else
        block_size = max_block_size;

      if (vflag)
        fprintf(stderr, "put id %s block_size %llu\n",
                id_str,
                (unsigned long long)block_size);

      block = malloc(block_size);
      if (NULL == block)
        {
          perror("malloc");
          exit(1);
        }

      if (Rflag)
        gen_data(id_str, block, block_size);
      else if (zflag)
        memset(block, 'z', block_size);
      else
        memset(block, 0, block_size);

      gettimeofday(&tv1, NULL);
      ret = dpl_put(ctx, bucket, id_str, NULL, NULL, DPL_CANNED_ACL_PRIVATE, block, block_size);
      gettimeofday(&tv2, NULL);

      pthread_mutex_lock(&stats_lock);
      if (0 == ret)
        {
	  uint64_t latency = (tv2.tv_sec - tv1.tv_sec)*1000 + (tv2.tv_usec - tv1.tv_usec) / 1000;
          stats.n_success++;
          stats.latency_sum += latency;
	  stats.latency_sq_sum += (double) latency*latency;
          stats.n_bytes += block_size;
        }
      else
        {
          stats.n_failures++;
        }
      pthread_mutex_unlock(&stats_lock);

      free(block);
    }

  if (vflag)
    fprintf(stderr, "%u done\n", threadid->id);

  pthread_cleanup_pop(0);

  pthread_exit(NULL);
}

typedef struct _tmainstats
{
  int i; /* loop */
  struct timeval tv1;
  struct timeval tv2;
  uint64_t speed;
  uint64_t total_speed;
} tmainstats;

tmainstats mainstats;

void print_stats_put_nolock(void)
{
  double lat_sigma_2 = 0;
  double lat_avg = 0;

  if (stats.n_success > 0)
    {
      lat_avg = (double) stats.latency_sum / stats.n_success;
      lat_sigma_2 = sqrt(stats.latency_sq_sum / stats.n_success - lat_avg*lat_avg);
    }


  printf("time=%lu ok=%llu fail=%llu bytes=%llu (%lluMbit/s) aver %lluMbit/s latency: %.1f ms sd: %.1f ops: %.2f (p)\n",
          mainstats.tv2.tv_sec - mainstats.tv1.tv_sec,
          (unsigned long long)stats.n_success,
          (unsigned long long)stats.n_failures,
          (unsigned long long)stats.n_bytes,
          (unsigned long long)mainstats.speed/timebase,
          (unsigned long long)mainstats.total_speed/(mainstats.i*timebase),
          lat_avg,
          lat_sigma_2,
          ((float)(mainstats.tv2.tv_sec - mainstats.tv1.tv_sec) * 1000 + (mainstats.tv2.tv_usec - mainstats.tv1.tv_usec)/1000) > 0 ? (float) stats.n_success / (((float)(mainstats.tv2.tv_sec - mainstats.tv1.tv_sec) * 1000 + (mainstats.tv2.tv_usec - mainstats.tv1.tv_usec)/1000) / 1000.0) : 0);

  return;
}

void print_gpstats_put_nolock(void)
{
  double lat_sigma_2 = 0;
  double lat_avg = 0;
  double total_time_ms;

  if (stats.n_success > 0)
    {
      lat_avg = (double) stats.latency_sum / stats.n_success;
      lat_sigma_2 = sqrt(stats.latency_sq_sum / stats.n_success - lat_avg*lat_avg);
    }

  total_time_ms = (double) (mainstats.tv2.tv_sec - mainstats.tv1.tv_sec) * 1000.0 + (mainstats.tv2.tv_usec - mainstats.tv1.tv_usec) /1000.0;

  /* (d/p/g) nt class size time latency sd ops*/

  printf("# 1 n_threads class max_block_size seed time n_succ n_fail lat lat_sd ops\n");
  printf("1 ");
  printf("%d ", n_threads);
  printf("%d ", class);
  printf("%d ", max_block_size);
  printf("%d ", seed);
  printf("%lu ", mainstats.tv2.tv_sec - mainstats.tv1.tv_sec);
  printf("%lu %lu ", (long unsigned) stats.n_success, (long unsigned) stats.n_failures);
  printf("%.1f %.1f ", lat_avg, lat_sigma_2);
  printf("%.2f ", total_time_ms != 0 ? (float) stats.n_success * 1000.0 / total_time_ms : 0);

  printf("\n");
}

void printstats_lock(void)
{
  pthread_mutex_lock(&stats_lock);
  print_stats_put_nolock();

  fflush(stdout);
  pthread_mutex_unlock(&stats_lock);
}

void printgpstats_lock(void)
{
  pthread_mutex_lock(&stats_lock);
  print_gpstats_put_nolock();

  fflush(stdout);
  pthread_mutex_unlock(&stats_lock);
}

void *timer_thread_main(void *arg)
{
  uint64_t prev_bytes;
  int dummy;

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &dummy);
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);

  pthread_mutex_lock(&stats_lock);
  memset(&mainstats, 0, sizeof(tmainstats));
  prev_bytes = 0;
  mainstats.i = 1;
  gettimeofday(&mainstats.tv1, NULL);
  pthread_mutex_unlock(&stats_lock);

  while (1)
    {
      sleep(timebase);
      pthread_mutex_lock(&stats_lock);
      mainstats.speed = (((stats.n_bytes ) - prev_bytes)*8)/(1024*1024);
      mainstats.total_speed += mainstats.speed;

      gettimeofday(&mainstats.tv2, NULL);

      if (print_running_stats != 0)
        {
          print_stats_put_nolock();
          fflush(stdout);
        }

      prev_bytes = stats.n_bytes;
      mainstats.i++;
      pthread_mutex_unlock(&stats_lock);

    }

  pthread_exit(NULL);
}

void doit()
{
  pthread_t timer_thread;
  int i, ret;

  ret = dpl_init();
  if (-1 == ret)
    {
      fprintf(stderr, "dpl init failed\n");
      exit(1);
    }

  ctx = dpl_ctx_new(NULL, profile);
  if (NULL == ctx)
    {
      fprintf(stderr, "dpl context creation failed\n");
      exit(1);
    }

  memset(&stats, 0, sizeof (stats));

  ret = pthread_attr_init(&g_detached_thread_attr);
  if (0 != ret)
    {
      fprintf(stderr, "pthread_attr_init: %d\n", ret);
      exit(1);
    }

  (void)pthread_attr_setdetachstate(&g_detached_thread_attr,
                                    PTHREAD_CREATE_DETACHED);

  ret = pthread_attr_init(&g_joinable_thread_attr);
  if (0 != ret)
    {
      fprintf(stderr, "pthread_attr_init: %d\n", ret);
      exit(1);
    }

  (void)pthread_attr_setdetachstate(&g_joinable_thread_attr,
                                    PTHREAD_CREATE_JOINABLE);

  ret = pthread_create(&timer_thread, &g_detached_thread_attr,
                       timer_thread_main, NULL);
  if (0 != ret)
    {
      fprintf(stderr, "pthread_create %d", ret);
      exit(1);
    }

  workers = malloc(n_threads * sizeof (pthread_t));
  if (NULL == workers)
    {
      perror("malloc");
      exit(1);
    }

  for (i = 0;i < n_threads;i++)
    {
      tthreadid *threadid;

      threadid = malloc(sizeof (*threadid));
      if (NULL == threadid)
        {
          perror("malloc");
          exit(1);
        }
      threadid->id = i;

      ret = pthread_create(&workers[i], &g_joinable_thread_attr,
                           test_main, threadid);
      if (0 != ret)
        {
          fprintf(stderr, "pthread_create %d", ret);
          exit(1);
        }
    }

  if (vflag)
    fprintf(stderr, "joining threads\n");

  for (i = 0;i < n_threads;i++)
    {
      void *status;

      if ((ret = pthread_join(workers[i], &status)) != 0)
        {
          fprintf(stderr, "pthread_cancel %d", ret);
          exit(1);
        }
    }

  if (vflag)
    fprintf(stderr, "fini\n");

  if (vflag)
    fprintf(stderr, "canceling timer\n");

  if ((ret = pthread_cancel(timer_thread)) != 0)
    {
      fprintf(stderr, "pthread_cancel %d", ret);
      exit(1);
    }

  if (print_running_stats != 0)
    {
      printstats_lock();
    }
  if (print_gnuplot_stats != 0)
    {
      printgpstats_lock();
    }

  if (print_running_stats != 0)
    {
      fprintf(stderr, "n_success %llu\n",
              (unsigned long long)stats.n_success);
    }
}

void usage()
{
  fprintf(stderr, "usage: bizresttest_01 [-P print gnuplot stats] [-t timebase][-R (random content)][-z (fill buffer with 'z's)][-r (random size)][-o (random oids)] [-S use seed] [-N n_threads][-n n_ops][-v (vflag)] [-p profile] [-B bucket] [-C class_of_service] [-b size]\n");
  exit(1);
}

int main(int argc, char **argv)
{
  char opt;

  memset(&drbuffer, 0, sizeof(struct drand48_data));

  while ((opt = getopt(argc, argv, "t:PRzrb:N:n:vB:p:oC:S:U")) != -1)
    switch (opt)
      {
      case 'o':
        oflag = 1;
        break ;
      case 't':
        timebase = atoi(optarg);
        break ;
      case 'R':
        Rflag = 1;
        break ;
      case 'z':
        zflag = 1;
        break ;
      case 'r':
        rflag = 1;
        break ;
      case 'N':
        n_threads = atoi(optarg);
        break ;
      case 'b':
        max_block_size = atoi(optarg);
        break;
      case 'n':
        n_ops = atoi(optarg);
        break ;
      case 'P':
        print_gnuplot_stats = 1;
        print_running_stats = 0;
        break;
      case 'B':
        bucket = strdup(optarg);
        assert(NULL != bucket);
        break ;
      case 'p':
        profile = strdup(optarg);
        assert(NULL != profile);
        break ;
      case 'C':
        class = atoi(optarg);
        break;
      case 'S':
        #if defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )
        srand48(atoi(optarg));
        #else
        srand48_r(atoi(optarg), &drbuffer);
        #endif
        seed = atoi(optarg);
        break;
      case 'v':
        vflag++;
        break ;
      case '?':
      default:
        usage();
      }
  argc -= optind;
  argv += optind;

  if (argc != 0)
    usage();

  if (NULL == bucket)
    usage();

  doit();

  return 0;
}
