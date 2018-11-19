#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

const size_t BUF_SIZE = 512;
int CAUGHT_SIG;

ssize_t memtofd_cpy(int fd, const char *buf, size_t count)
{
	ssize_t tmp = 1;
	size_t len = count;
	while (len && tmp) {
		tmp = write(fd, buf, len);
		if (tmp == -1) {
			if (errno != EINTR)
				return -1;
		} else {
			len -= tmp;
			buf += tmp;
		}
	}
	return count - len;
}

void handle_signal(int sig)
{
	CAUGHT_SIG = sig;
}

int byte_receive(pid_t pid, uint8_t * val_p, sigset_t * set)
{
	uint8_t val = 0;
	for (uint8_t i = 0; i < 8; i++, val >> 1) {
		alarm(1);
		sigsuspend(set);
		if (errno != EINTR) {
			perror("Error: sigsuspend");
			return -1;
		}

		switch (CAUGHT_SIG) {
		case SIGUSR1:
			break;
		case SIGUSR2:
			val |= 1 << i;
			break;
		case SIGALRM:
			fprintf(stderr, "Error: sender timed out\n");
			return -1;
		}

		if (kill(pid, SIGUSR1) == -1) {
			perror("Error: kill");
			return -1;
		}
	}
	*val_p = val;
	return 0;
}

int byte_send(pid_t pid, uint8_t * val_p, sigset_t * set)
{
	uint8_t val = *val_p;
	for (uint8_t i = 0; i < 8; i++, val >> 1) {
		int sig = (val & (1 << i)) ? SIGUSR2 : SIGUSR1;
		if (kill(pid, sig) == -1) {
			perror("Error: kill");
			return -1;
		}

		alarm(1);
		sigsuspend(set);
		if (errno != EINTR) {
			perror("Error: sigsuspend");
			return -1;
		}

		switch (CAUGHT_SIG) {
		case SIGUSR1:
			break;
		case SIGALRM:
			fprintf(stderr, "Error: receiver timed out\n");
			return -1;
		}
	}
	return 0;
}

int buf_send(pid_t pid, uint8_t * buf, size_t len, sigset_t * set)
{
	for (size_t i = 0; i < len; i++) {
		if (byte_send(pid, buf + i, set) == -1) {
			fprintf(stderr, "Error: byte_send\n");
			return -1;
		}
	}
	return 0;
}

int buf_receive(pid_t pid, uint8_t * buf, size_t len, sigset_t * set)
{
	for (size_t i = 0; i < len; i++) {
		if (byte_receive(pid, buf + i, set) == -1) {
			fprintf(stderr, "Error: byte_receive\n");
			return -1;
		}
	}
	return 0;
}

int child(pid_t ppid, int fd)
{
	struct sigaction act = { };
	act.sa_handler = &handle_signal;
	sigset_t set;
	sigfillset(&set);
	sigdelset(&set, SIGUSR1);
	sigdelset(&set, SIGALRM);
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGALRM, &act, NULL);

	char *buf = malloc(BUF_SIZE);
	if (buf == NULL) {
		perror("Error: malloc");
		return 0;
	}

	ssize_t len;
	do {
		len = read(fd, buf, BUF_SIZE);
		if (len == -1) {
			perror("Error: read");
			return -1;
		}
		if (buf_send(ppid, (uint8_t *) &len, sizeof(len), &set) == -1) {
			fprintf(stderr, "Error: buf_send\n");
			return -1;
		}
		if (len != 0 && buf_send(ppid, buf, len, &set) == -1) {
			fprintf(stderr, "Error: buf_send\n");
			return -1;
		}
	} while (len > 0);

	return 0;
}

int parent(pid_t cpid, int fd)
{
	struct sigaction act = { };
	act.sa_handler = &handle_signal;
	sigset_t set;
	sigfillset(&set);
	sigdelset(&set, SIGUSR1);
	sigdelset(&set, SIGUSR2);
	sigdelset(&set, SIGALRM);
	sigaction(SIGUSR1, &act, NULL);
	sigaction(SIGUSR2, &act, NULL);
	sigaction(SIGALRM, &act, NULL);

	char *buf = malloc(BUF_SIZE);
	if (buf == NULL) {
		perror("Error: malloc");
		return 0;
	}

	ssize_t len;
	do {
		if (buf_receive(cpid, (uint8_t *) &len, sizeof(len), &set) ==
		    -1) {
			fprintf(stderr, "Error: buf_send\n");
			return -1;
		}
		if (len != 0 && buf_receive(cpid, buf, len, &set) == -1) {
			fprintf(stderr, "Error: buf_send\n");
			return -1;
		}
		if (len != 0 && memtofd_cpy(fd, buf, len) == -1) {
			fprintf(stderr, "Error: memtofd_cpy\n");
			return -1;
		}
	} while (len > 0);

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Error: wrong argv\n");
		return 0;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("Error: open");
		return 0;
	}

	sigset_t set;
	sigfillset(&set);
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
		perror("Error: sigprocmask");
		return 0;
	}

	pid_t ppid = getpid();
	pid_t cpid = fork();
	if (cpid == -1) {
		perror("Error: fork");
		return 0;
	}

	if (cpid == 0) {
		if (child(ppid, fd) == -1) {
			fprintf(stderr, "Error: child failed\n");
			return 0;
		}
	} else {
		if (parent(cpid, STDOUT_FILENO) == -1) {
			fprintf(stderr, "Error: parent failed\n");
			return 0;
		}
	}

	return 0;
}
