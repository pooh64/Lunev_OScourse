#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#define FDCPY_BUF_SIZE 1024

ssize_t fdcpy(int fd_out, int fd_in)
{
	char *buf = malloc(FDCPY_BUF_SIZE);
	if (buf == NULL)
		return -1;
	ssize_t sumary = 0;
	while (1) {
		ssize_t ret = read(fd_in, buf, FDCPY_BUF_SIZE);
		if (ret == -1) {
			if (errno == EINTR) {
				errno = 0;
				continue;
			}
			free(buf);
			return -1;
		} else if (ret == 0)
			break;

		ssize_t len = ret;
		char *cur = buf;
		while (len != 0 && ret != 0) {
			ret = write(fd_out, cur, len);
			if (ret == -1) {
				if (errno == EINTR) {
					errno = 0;
					continue;
				}
				free(buf);
				return -1;
			}
			len -= ret;
			cur += ret;
			sumary += ret;
		}
	}
	free(buf);
	return sumary;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Error: Wrong arg num\n");
		return 0;
	}

	int arg_fd = open(argv[1], O_RDONLY);
	int pipe_fd[2] = { };
	if (pipe(pipe_fd) != 0) {
		perror("Error: pipe failed");
		return 0;
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("Error: fork failed");
		return 0;
	}

	if (pid == 0) {
		close(pipe_fd[0]);
		fdcpy(pipe_fd[1], arg_fd) == -1;	// check err
		close(pipe_fd[1]);
	} else {
		close(pipe_fd[1]);
		fdcpy(STDOUT_FILENO, pipe_fd[0]);
		wait(NULL);
		close(pipe_fd[0]);
	}
	return 0;
}
