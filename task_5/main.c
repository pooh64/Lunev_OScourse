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
const int FD_CLOSED = 0xdead;

struct chld_t {
	int to_chld[2];
	int to_prnt[2];
	int is_done[2];
	char  *buf;
	size_t buf_s;
	size_t buf_l;
};

#define MAX(a, b) (((a) > (b)) ? (a) : (b))


void child(unsigned this, unsigned n_chld, struct chld_t *carr);
int parent(unsigned n_chld, struct chld_t *carr);

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
size_t get_pbuf_size(unsigned num, unsigned n_chld)
{
	size_t buf_s = 1;
	for (unsigned i = 0; i < n_chld - num; i++) {
		buf_s *= 3;
		if (buf_s > PBUF_MAX)
			return PBUF_MAX;
	}
	return buf_s;
}

int prepare_buffers(unsigned n_chld, struct chld_t *carr)
{
	for (unsigned i = 0; i < n_chld; i++) {
		carr[i].buf_s = get_pbuf_size(i, n_chld);
		carr[i].buf = malloc(carr[i].buf_s);
		if (carr[i].buf == NULL) {
			perror("Error: malloc");
			return -1;
		}
	}
	return 0;
}

int prepare_pipes(int fd_inp, unsigned n_chld, struct chld_t *carr)
{
	for (unsigned i = 0; i < n_chld; i++) {
		if (i != 0) {
			if (pipe(carr[i].to_chld) == -1) {
				perror("Error: pipe\n");
				return -1;
			}
		} else {
			carr[i].to_chld[0] = fd_inp;
			carr[i].to_chld[1] = FD_CLOSED;
		}

		if (pipe(carr[i].to_prnt) == -1) {
			perror("Error: pipe\n");
			return -1;
		}
	}
	return 0;
}

int transmission(int fd_inp, unsigned n_chld)
{
	struct chld_t *carr = calloc(n_chld, sizeof(struct chld_t));
	
	if (prepare_pipes(fd_inp, n_chld, carr) == -1) {
		free(carr);
		return -1;
	}
	
	for (unsigned i = 0; i < n_chld; i++) {
		pid_t cpid = fork();
		if (cpid == -1) {
			perror("Error: fork");
			return -1;
		}
		if (cpid == 0)
			child(i, n_chld, carr);
	}
	
	if (parent(n_chld, carr) == -1) {
		free(carr);
		return -1;
	}
	return 0;
}

int parent(unsigned n_chld, struct chld_t *carr)
{
	if (prepare_buffers(n_chld, carr) == -1) {
		free(carr);
		return -1;
	}

	int max_fd, n_fd;
	fd_set rfds, wfds;

	while (1) {
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		/* Prepare fds */
		for (int i = 0; i < n_chld; i++) {
			if (carr[i].to_chld[1] != FD_CLOSED && carr[i - 1].buf_l != 0) {
				FD_SET(carr[i].to_child[1], &wfds);
				max_fd = MAX(max_fd, carr[i].to_chld[1]);
				n_fd++;
			}
			if (carr[i].to_prnt[0] != FD_CLOSED && carr[i].buf_l == 0) {
				FD_SET(carr[i].to_prnt[0], &rfds);
				max_fd = MAX(max_fd, carr[i].to_prnt[0]);
				n_fd++;
			}
		}
		
		if (n_fd == 0)
			break;

		int ret_select = select(maxfd + 1, &rfds, &wfds, NULL, NULL);
		if (ret_select == -1) {		
			perror("Error: select");
			return -1;
		}

		for (int i = 0; i < n_chld; i++) {
			/*
			 * if isset fd_r
			 * 	read to this buffer
			 *	if read == 0
			 *		(check done) close fd_w to next and this fd
			 *
			 * if isset fd_w
			 * 	write from prev buffer
			 *
			 * Every time do n_fd-- and exit when it's eq to 0
			*/
		}

		// Write data to stdout if last len != 0, decrease
	}
}

void child(unsigned this, unsigned n_chld, struct chld_t *carr)
{
	/* Close unused pipes */
	for (unsigned i = 1; i < n_chld; i++) {
		if (i != 0)
			close(carr[i].to_chld[1]);
		close(carr[i].to_prnt[0]);
		if (i != this) {
			close(carr[i].to_chld[0]);
			close(carr[i].to_prnt[1]);
		}
	}

	void *buf = malloc(CBUF_SIZE);
	ssize_t len;
	int fd_r = carr[this].to_chld[0];
	int fd_w = carr[this].to_prnt[1];
	char msg_ok = 0;

	printf("Child %u ready!\n", this);

	do {
		len = read(fd_r, buf, CBUF_SIZE);
		if (len == -1) {
			perror("Error: read");
			exit(EXIT_FAILURE);
		}
		printf("Child %u: %zu bytes (%.*s)\n", this, len, len, buf);
		if (write(fd_w, buf, len) == -1) {
			fprintf(stderr, "Error: memtofd_cpy failed\n");
			exit(EXIT_FAILURE);
		}
	} while (len);

	printf("Child %u exit!\n", this);
	
	// possibly close and send done here (before closing fd_w!)
	free(carr);
	free(buf);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	unsigned long n_chld;
	int fd_inp;
	if (argc != 3) {
		fprintf(stderr, "Error: Wrong args\n");
		return 0;
	}
	if (str_to_ulong(argv[2], &n_chld) == -1) {
		fprintf(stderr, "Error: Wrong argv[2]\n");
		return 0;
	}
	if ((fd_inp = open(argv[1], O_RDONLY)) == -1) {
		perror("Error: open argv[1]");
		return 0;
	}

	if (transmission(fd_inp, n_chld) == -1) {
		fprintf(stderr, "Error: transmission failed\n");
		return -1;
	}

	return 0;
}
