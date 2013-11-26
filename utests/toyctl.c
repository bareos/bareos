/* TODO: copyright */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <assert.h>
#include "toyctl.h"

extern int verbose;

static int
toyserver_wait(pid_t pid)
{
  int r;
  int status;

  for (;;)
    {
      r = waitpid(pid, &status, 0);
      if (r < 0)
	{
	  if (errno == EAGAIN)
	    continue;
	  if (errno == EINTR)
	    continue;
	  if (errno == ESRCH)
	    return -1;	  /* it's gone */
	  if (errno == ECHILD)
	    return -1;	  /* it's gone */
	  fprintf(stderr, "Couldn't wait for toyserver pid %d: %s\n",
		  pid, strerror(errno));
	  return -1;
	}

      if (r != pid)
	continue;

      if (WIFEXITED(status))
	{
	  if (!WEXITSTATUS(status))
	    return 0;	  /* normal successful exit */
	  if (verbose)
	    fprintf(stderr, "Toyserver exited with status %d\n", WEXITSTATUS(status));
	  return -1;
	}
      if (WIFSIGNALED(status))
	{
	  if (verbose)
	    fprintf(stderr, "Toyserver terminated by signal %d%s\n",
		    WTERMSIG(status),
		    WCOREDUMP(status) ? ", core dumped" : "");
	  return -1;
	}
    }
}

/*
 * Shut down the server, given it's PID.  It's pretty simple and brutal:
 * we just send it SIGTERM and wait to reap it.
 */
static void
toyserver_shutdown(pid_t pid)
{
    kill(pid, SIGTERM);
    toyserver_wait(pid);
}

int
toyserver_start(const char *name, struct server_state **statep)
{
  int fd = -1;
  int r;
  struct stat sb;
  pid_t pid = -1;
  int argc = 0;
  void *p;
  char *argv[6];
  char statefile[1024];

  snprintf(statefile, sizeof(statefile), "%s.state",
	  name ? name : "toyserver");

  if (verbose)
    fprintf(stderr, "Starting toyserver\n");

  argv[argc++] = "./toyserver";

  if (verbose)
    argv[argc++] = "-v";

  if (name)
    {
      argv[argc++] = "-s";
      argv[argc++] = statefile;
    }
  argv[argc++] = NULL;

  pid = fork();
  if (pid < 0)
    {
      perror("fork");
      return -1;
    }
  if (!pid)
    {
      /* child process */
      execv(argv[0], argv);
      perror(argv[0]);
      exit(1);
    }
  else
    {
      /* parent process */
      r = toyserver_wait(pid);
      if (r < 0)
	return -1;
    }

  /* in the parent, and toyserver started successfully */

  fd = open(statefile, O_RDWR);
  if (fd < 0)
    {
      perror(statefile);
      goto error;
    }
  r = fstat(fd, &sb);
  if (r < 0)
    {
      perror(statefile);
      goto error;
    }
  if (sb.st_size != sizeof(struct server_state))
    {
      fprintf(stderr, "Statefile %s is wrong size, expecting %d got %llu\n",
	      statefile, (int)sizeof(struct server_state),
	      (unsigned long long)sb.st_size);
      goto error;
    }

  if (verbose)
    fprintf(stderr, "Mapping statefile\n");

  p = mmap(NULL, sizeof(struct server_state), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    {
      perror("mmap");
      goto error;
    }

  *statep = (struct server_state *)p;
  if (verbose)
    fprintf(stderr, "Started toyserver pid %d\n", (*statep)->pid);
  return 0;

error:
  if (fd >= 0)
    close(fd);
  if (pid > 1)
    toyserver_shutdown(pid);
  return -1;
}

void
toyserver_stop(struct server_state *state)
{
  if (state)
    {
      if (verbose)
	fprintf(stderr, "Shutting down toyserver pid %d\n", state->pid);
      toyserver_shutdown(state->pid);
    }
}

const char *
toyserver_addrlist(struct server_state *state)
{
  static char buf[128];

  snprintf(buf, sizeof(buf), "localhost:%d", state->port);
  return buf;
}

/* vim: set sw=2 sts=2: */
