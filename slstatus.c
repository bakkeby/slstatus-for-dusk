/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "arg.h"
#include "slstatus.h"
#include "util.h"

struct arg {
	const char *(*func)(const char *);
	const char *fmt;
	const char *args;
	const char *status_no;
	const unsigned int update_interval;
};

char buf[1024];
static volatile sig_atomic_t done;

#include "config.h"

static void
terminate(const int signo)
{
	if (signo != SIGUSR1)
		done = 1;
}

static void
difftimespec(struct timespec *res, struct timespec *a, struct timespec *b)
{
	res->tv_sec = a->tv_sec - b->tv_sec - (a->tv_nsec < b->tv_nsec);
	res->tv_nsec = a->tv_nsec - b->tv_nsec + (a->tv_nsec < b->tv_nsec) * 1E9;
}

static void
usage(void)
{
	die("usage: %s [-s] [-1]", argv0);
}

int
main(int argc, char *argv[])
{
	struct sigaction act;
	struct timespec start, current, diff, intspec, wait;
	size_t i;
	unsigned int loop_count = 0;
	char status[MAXLEN];
	char status_no[3] = {0};
	const char *res;

	/* The external command to run to update individual statuses. */
	const char *extcmd[] = { "duskc", "--ignore-reply", "run_command", "setstatus", status_no, status, NULL };

	/* Get the bar height and store it in an environment variable.
	 * The run command will return NULL if dusk is not running. */
	const char *bar_height = run_command("duskc get_bar_height");
	if (bar_height)
		setenv("BAR_HEIGHT", bar_height, 1);

	/* Create a lock file that prevents multiple instances of this
	 * program to be running for the same user on the same display. */
	char lock_file[100] = {0};
	snprintf(lock_file, sizeof lock_file - 1, "/tmp/.%s.slstatus.%s.lock", getenv("USER"), getenv("DISPLAY"));
	struct flock fl = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0
	};

	int lock_fd = open(lock_file, O_CREAT | O_RDWR, 0644);
	if (lock_fd == -1) {
		fprintf(stderr, "Error: Failed to open lock file %s: %s\n", lock_file, strerror(errno));
		exit(1);
	}

	/* Try to acquire a lock on the file */
	if (fcntl(lock_fd, F_SETLK, &fl) == -1) {
		fprintf(stderr, "Error: Another instance of the program is already running\n");
		exit(1);
	}

	ARGBEGIN {
	case '1':
		done = 1;
		/* FALLTHROUGH */
	default:
		usage();
	} ARGEND

	if (argc)
		usage();

	memset(&act, 0, sizeof(act));
	act.sa_handler = terminate;
	sigaction(SIGINT,  &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_flags |= SA_RESTART;
	sigaction(SIGUSR1, &act, NULL);

	do {
		if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
			die("clock_gettime:");

		for (i = 0; i < LEN(args); i++) {
			if (loop_count % args[i].update_interval)
				continue;

			status[0] = '\0';
			if (!(res = args[i].func(args[i].args)))
				res = unknown_str;

			if (esnprintf(status, sizeof(status), args[i].fmt, res) < 0)
				break;

			esnprintf(status_no, sizeof(status_no), args[i].status_no);

			if (fork() == 0) {
				setsid();
				execvp(extcmd[0], (char **)extcmd);
				die("dwm: execvp '%s' failed:", extcmd[0]);
			}
		}

		++loop_count;

		if (!done) {
			if (clock_gettime(CLOCK_MONOTONIC, &current) < 0)
				die("clock_gettime:");
			difftimespec(&diff, &current, &start);

			intspec.tv_sec = interval / 1000;
			intspec.tv_nsec = (interval % 1000) * 1E6;
			difftimespec(&wait, &intspec, &diff);

			if (wait.tv_sec >= 0 && nanosleep(&wait, NULL) < 0 && errno != EINTR)
				die("nanosleep:");
		}
	} while (!done);

	/* Release the lock on the file */
	fl.l_type = F_UNLCK;
	fcntl(lock_fd, F_SETLK, &fl);

	/* Close the lock file when the program exits */
	close(lock_fd);
	unlink(lock_file);

	return 0;
}
