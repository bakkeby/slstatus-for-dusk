/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <spawn.h>
#include <string.h>

#include "../slstatus.h"
#include "../util.h"

#define MAX_ARGS 10

const char *
run_exec(const char *cmd)
{
	char *p;
	char *argv[MAX_ARGS];
	char *home_dir = getenv("HOME");
	char *exec = strdup(cmd); // only to make a non-const copy
	int argc = 0;
	int pipefd[2];
	pid_t pid;
	ssize_t bytes_read;
	posix_spawn_file_actions_t actions;
	buf[0] = '\0';

	if (pipe(pipefd) == -1)
		die("Error: run_exec '%s' failed reading pipe:", cmd);

	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_addclose(&actions, pipefd[0]); // Close read end in child process
	posix_spawn_file_actions_adddup2(&actions, pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe

	/* Basic splitting of arguments by space character */
	char *token = strtok(exec, " ");
	while (token != NULL && argc < MAX_ARGS - 1) {
		if (strncmp(token, "~/", 2) == 0) {
			/* Replace ~/ with the value of HOME environment variable */
			argv[argc] = malloc(strlen(home_dir) + strlen(token) - 1);
			sprintf(argv[argc], "%s%s", home_dir, token + 1);
		} else {
			argv[argc] = strdup(token);
		}
		argc++;
		token = strtok(NULL, " ");
	}
	argv[argc] = NULL;
	free(exec);

	if (posix_spawnp(&pid, argv[0], &actions, NULL, argv, NULL) != 0)
		die("Error: run_exec '%s' failed excuting posix_spawnp:", cmd);

	close(pipefd[1]); // Close the write end of the pipe in the parent process

	/* Read from the pipe */
	while ((bytes_read = read(pipefd[0], buf, sizeof(buf))) > 0);

	close(pipefd[0]); // Close the read end of the pipe

	/* Wait for the child process to finish */
	waitpid(pid, NULL, 0);

	posix_spawn_file_actions_destroy(&actions);

	/* Replace line break with EOL */
	if ((p = strrchr(buf, '\n')))
		p[0] = '\0';

	/* Free dynamically allocated memory */
	for (int i = 0; i < argc; i++) {
		if (argv[i] != NULL) {
			free(argv[i]);
		}
	}

	return buf[0] ? buf : NULL;
}
