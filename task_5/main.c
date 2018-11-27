#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

const size_t PBUF_MAX = 1024;
const size_t CBUF_SIZE = 1024;

struct child_t {
	int to_child[2];
	int to_parent[2];
	char *buf;
	size_t buf_s;
	size_t len;
};


void child(unsigned this, unsigned n_child, struct child_t *carr);
int parent(unsigned n_child, struct child_t *carr);

ssize_t memtofd_cpy(int fd, const char *buf, size_t count)
{
	ssize_t tmp;
	size_t len = count;
	do {
		tmp = write(fd, buf, len);
		if (tmp == -1) {
			if (errno != EINTR)
				return -1;
		} else {
			len -= tmp;
			buf += tmp;
		}
	} while (len && tmp);
	return count - len;
}

int str_to_ulong(const char *str, unsigned long int *val_p)
{
	char *endptr;
	errno = 0;
	*val_p = strtoul(str, &endptr, 0);
	if ((errno == ERANGE && (*val_p == LONG_MAX || *val_p == LONG_MIN))
	    || (errno != 0 && *val_p == 0) || (*endptr != '\0'))
		return -1;
	return 0;
}

// last i -> buf_s == 1?
size_t get_pbuf_size(unsigned num, unsigned n_child)
{
	size_t buf_s = 1;
	for (unsigned i = 0; i < n_child - num; i++) {
		buf_s *= 3;
		if (buf_s > PBUF_MAX)
			return PBUF_MAX;
	}
	return buf_s;
}

int prepare_buffers(unsigned n_child, struct child_t *carr)
{
	for (unsigned i = 0; i < n_child; i++) {
		carr[i].buf_s = get_pbuf_size(i, n_child);
		carr[i].buf = malloc(carr[i].buf_s);
		if (carr[i].buf == NULL) {
			perror("Error: malloc");
			return -1;
		}
	}
	return 0;
}

int prepare_pipes(int fd_inp, unsigned n_child, struct child_t *carr)
{
	for (unsigned i = 0; i < n_child; i++) {
		if (i != 0) {
			if (pipe(carr[i].to_child) == -1) {
				perror("Error: pipe\n");
				return -1;
			}
		} else {
			carr[i].to_child[0] = fd_inp;
		}

		if (pipe(carr[i].to_parent) == -1) {
			perror("Error: pipe\n");
			return -1;
		}
	}
	return 0;
}

int transmission(int fd_inp, unsigned n_child)
{
	struct child_t *carr = calloc(n_child, sizeof(struct child_t));
	
	if (prepare_pipes(fd_inp, n_child, carr) == -1) {
		free(carr);
		return -1;
	}
	
	for (unsigned i = 0; i < n_child; i++) {
		pid_t cpid = fork();
		if (cpid == -1) {
			perror("Error: fork");
			return -1;
		}
		if (cpid == 0)
			child(i, n_child, carr);
	}
	
	if (parent(n_child, carr) == -1) {
		free(carr);
		return -1;
	}
	
	return 0;
}

int parent(unsigned n_child, struct child_t *carr)
{
	if (prepare_buffers(n_child, carr) == -1) {
		free(carr);
		return -1;
	}

	int maxfd = 0;
	fd_set rfds, wfds;
	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	unsigned n_done = 0;

	for (unsigned i = 0; i < n_child; i++) {
		if (i != 0) {
			close(carr[i].to_child[0]);

			FD_SET(carr[i].to_child[1], &wfds);
			if (carr[i].to_child[1] > maxfd)
				maxfd = carr[i].to_child[1];
		}
		close(carr[i].to_parent[1]);

		FD_SET(carr[i].to_parent[0], &rfds);
		if (carr[i].to_parent[0] > maxfd)
			maxfd = carr[i].to_parent[0];
	}

	while (1) {
		int ret_select = select(maxfd + 1, &rfds, &wfds, NULL, NULL);
		if (ret_select == -1) {		
			perror("Error: select");
			return -1;
		}

		unsigned n_done = 0;

		for (unsigned i = n_done; i < n_child; i++) {
			char msg;
			ssize_t len;
			
			if (i != 0 && FD_ISSET(carr[i].to_child[1], &wfds)) {
				len = write(carr[i].to_child[1], carr[i - 1].buf, carr[i - 1].len);
				if (len == -1)
					exit(EXIT_FAILURE);
				carr[i - 1].len -= len;
				if (carr[i - 1].len == 0 && carr[i - 1].to_parent[0] == -1) {
					FD_CLR(carr[i].to_child[1], &wfds);
					close(carr[i].to_child[1]);
					carr[i].to_child[1] = -1;
				}
			} else if (i != 0 && carr[i].to_child[1] != -1) {
				FD_SET(carr[i].to_child[1], &wfds);
			}

			if (FD_ISSET(carr[i].to_parent[0], &rfds) && carr[i].len == 0) {
				len = read(carr[i].to_parent[0], carr[i].buf, carr[i].len);
				if (len == -1)
					exit(EXIT_FAILURE);
				carr[i].len = len;
				if (len == 0) {
					FD_CLR(carr[i].to_parent[0], &rfds);
					close(carr[i].to_parent[0]);
					carr[i].to_parent[0] == -1;
					n_done++;
				}
			} else if (carr[i].to_parent[0] != -1) {
				FD_SET(carr[i].to_parent[0], &rfds);
			}
		}

		if (carr[n_child - 1].len > 0) 
			printf("Parent: %u (%*.s)\n", carr[n_child - 1].len, carr[n_child - 1].len, carr[n_child - 1].buf);

		if (n_done == n_child) {
			printf("Done\n");
			exit(EXIT_SUCCESS);
		}
	}
}

void child(unsigned this, unsigned n_child, struct child_t *carr)
{
	/* Close unused pipes */
	for (unsigned i = 1; i < n_child; i++) {
		if (i != 0)
			close(carr[i].to_child[1]);
		close(carr[i].to_parent[0]);
		if (i != this) {
			close(carr[i].to_child[0]);
			close(carr[i].to_parent[1]);
		}
	}

	void *buf = malloc(CBUF_SIZE);
	ssize_t len;
	int fd_r = carr[this].to_child[0];
	int fd_w = carr[this].to_parent[1];
	char msg_ok = 0;

	printf("Child %u ready!\n", this);

	do {
		len = read(fd_r, buf, CBUF_SIZE);
		if (len == -1) {
			perror("Error: read");
			exit(EXIT_FAILURE);
		}
		sleep(1);
		printf("Child %u: %zu bytes (%.*s)\n", this, len, len, buf);
		if (write(fd_w, buf, len) == -1) {
			fprintf(stderr, "Error: memtofd_cpy failed\n");
			exit(EXIT_FAILURE);
		}
	} while (len);

	printf("Child %u exit!\n", this);

	free(carr);
	free(buf);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	unsigned long n_child;
	int fd_inp;
	if (argc != 3) {
		fprintf(stderr, "Error: Wrong args\n");
		return 0;
	}
	if (str_to_ulong(argv[2], &n_child) == -1) {
		fprintf(stderr, "Error: Wrong argv[2]\n");
		return 0;
	}
	if ((fd_inp = open(argv[1], O_RDONLY)) == -1) {
		perror("Error: open argv[1]");
		return 0;
	}

	if (transmission(fd_inp, n_child) == -1) {
		fprintf(stderr, "Error: parent failed\n");
		return -1;
	}

	return 0;
}
