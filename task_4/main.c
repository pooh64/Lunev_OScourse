#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const size_t BUF_SIZE = 512;

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

ssize_t fdtomem_cpy(int fd, char *buf, size_t count)
{
	ssize_t tmp = 1;
	size_t len = count;
	while (len && tmp) {
		tmp = read(fd, buf, len);
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

// sigalrm, sigusr1, sigusr2
int byte_receive(pid_t pid, uint8_t *val_p, sigset_t *set)
{
	uint8_t val = 0;
	for (uint8_t i = 0; i < 8; i++, val >> 1) {
		alarm(1);
		sigsuspend(set);
		if (errno != EINTR) {
			perror("Error: sigsuspend");
			return -1;
		}
		
		switch(CAUGHT_SIG) {
		case SIGUSR1:
			break;
		case SIGUSR2:
			val |= 1 << i;
			break;
		case SIGALRM:
			fprintf(stderr, "Error: dead sender\n");
			return -1;
		}
		
		if (kill(pid, SIGUSR1) == -1) {
			perror("Error: kill");
			fprintf(stderr, "Error: dead sender\n");
			return -1;
		}
	}
	return 0;
}

// sigusr1, sigalrm
int byte_send(pid_t pid, uint8_t *val_p, sigset_t *set)
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
		
		switch(CAUGHT_SIG) {
		case SIGUSR1:
			break;
		case SIGALRM:
			fprintf(stderr, "Error: dead sender\n");
			return -1;
		}
	}
	return 0;
}

int child(pid_t ppid, int fd)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGALRM);
	
	char *buf = malloc(BUF_SIZE);
	if (buf == NULL) {
		perror("Error: malloc");
		return 0;
	}
	
	return 0;
}

int parent(pid_t cpid)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGUSR2);
	sigaddset(&set, SIGALRM);
	
	char *buf = malloc(BUF_SIZE);
	if (buf == NULL) {
		perror("Error: malloc");
		return 0;
	}
	
	return 0;
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Error: wrong argv\n");
		return 0;
	}

	int fd = open(path, O_RDONLY);
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
		if (parent(cpid) == -1) {
			fprintf(stderr, "Error: parent failed\n");
			return 0;
		}
	}

	return 0;
}
