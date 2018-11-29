#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

const size_t PBUF_MAX = 1024;
const size_t CBUF_SIZE = 1024;
const int FD_CLOSED = 0xdead;

struct chld_t {
	int to_chld[2];
	int to_prnt[2];
	char  *buf;
	char  *buf_p;
	size_t buf_s;
	size_t buf_l;
};

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define HANDLE_ERR(expr, msg)						\
do {									\
	if ((expr) == -1) {						\
		fprintf(stderr, "Error: %s, line %d", msg, __LINE__);	\
		exit(EXIT_FAILURE);					\
	}								\
} while (0)


_Noreturn void child(unsigned this, unsigned n_chld, struct chld_t *carr);
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
		carr[i].buf_p = carr[i].buf;
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
	
	parent(n_chld, carr);
	free(carr);
	return 0;
}

int parent(unsigned n_chld, struct chld_t *carr)
{
	/* Close unused pipes */
	for (unsigned i = 0; i < n_chld; i++) {
		HANDLE_ERR(close(carr[i].to_chld[0]), "close");
		HANDLE_ERR(close(carr[i].to_prnt[1]), "close");
		carr[i].to_chld[0] = FD_CLOSED;
		carr[i].to_prnt[1] = FD_CLOSED;
		if (carr[i].to_chld[1] != FD_CLOSED)
			HANDLE_ERR(fcntl(carr[i].to_chld[1], F_SETFL, O_NONBLOCK | O_WRONLY), "fcntl");
	}

	HANDLE_ERR(prepare_buffers(n_chld, carr), "prepare_buffers");

	while (1) {
		int max_fd, n_fd;
		fd_set rfds, wfds;
		ssize_t len;
		/* Prepare fds */
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		n_fd = 0;
		max_fd = 0;
		for (int i = 0; i < n_chld; i++) {
			if (carr[i].to_chld[1] != FD_CLOSED && carr[i - 1].buf_l != 0) {
				FD_SET(carr[i].to_chld[1], &wfds);
				max_fd = MAX(max_fd, carr[i].to_chld[1]);
				n_fd++;
			}
			if (carr[i].to_prnt[0] != FD_CLOSED) {
				FD_SET(carr[i].to_prnt[0], &rfds);
				max_fd = MAX(max_fd, carr[i].to_prnt[0]);
				n_fd++;
			}
		}
		if (n_fd == 0)
			break;
			
		n_fd = select(max_fd + 1, &rfds, &wfds, NULL, NULL);
		HANDLE_ERR(n_fd, "select");

		for (int i = 0; i < n_chld; i++) {
			if (carr[i].to_chld[1] != FD_CLOSED && FD_ISSET(carr[i].to_chld[1], &wfds) && carr[i - 1].buf_l != 0) {
				len = write(carr[i].to_chld[1], carr[i - 1].buf_p, carr[i - 1].buf_l);
				HANDLE_ERR(len, "write");
				/* Refresh buffer */
				if ((carr[i - 1].buf_l -= len) == 0)
					carr[i - 1].buf_p = carr[i - 1].buf;
				else
					carr[i - 1].buf_p += len;
			}

			len = (carr[i].buf + carr[i].buf_s) - (carr[i].buf_p + carr[i].buf_l);
			if (carr[i].to_prnt[0] != FD_CLOSED && FD_ISSET(carr[i].to_prnt[0], &rfds) && len != 0) {
				len = read(carr[i].to_prnt[0], carr[i].buf_p + carr[i].buf_l, len);
				HANDLE_ERR(len, "read");
				/* Refresh buffer */
				carr[i].buf_l += len;
				if (len == 0) {
					HANDLE_ERR(close(carr[i].to_prnt[0]), "close");
					carr[i].to_prnt[0] = FD_CLOSED;
				}
			}
			
			/* Writing done -> close fd_w */
			if (carr[i].to_chld[1] != FD_CLOSED && carr[i - 1].buf_l == 0 && carr[i - 1].to_prnt[0] == FD_CLOSED) {
				HANDLE_ERR(close(carr[i].to_chld[1]), "close");
				carr[i].to_chld[1] = FD_CLOSED;
			}
		}

		/* Write data to stdout */
		if (carr[n_chld - 1].buf_l != 0) {
			len = write(STDOUT_FILENO, carr[n_chld - 1].buf_p, carr[n_chld - 1].buf_l);
			HANDLE_ERR(len, "write");
			/* Refresh buffer */
			if ((carr[n_chld - 1].buf_l -= len) == 0)
				carr[n_chld - 1].buf_p = carr[n_chld - 1].buf;
			else
				carr[n_chld - 1].buf_p += len;	
		}
	}
	
	for (int i = 0; i < n_chld; i++)
		free(carr[i].buf);
	return 0;
}

_Noreturn void child(unsigned this, unsigned n_chld, struct chld_t *carr)
{
	/* Close unused pipes */
	for (unsigned i = 0; i < n_chld; i++) {
		if (i != 0)
			HANDLE_ERR(close(carr[i].to_chld[1]), "close");
		HANDLE_ERR(close(carr[i].to_prnt[0]), "close");
		if (i != this) {
			HANDLE_ERR(close(carr[i].to_chld[0]), "close");
			HANDLE_ERR(close(carr[i].to_prnt[1]), "close");
		}
	}

	void *buf = malloc(CBUF_SIZE);
	if (buf == NULL) {
		perror("Error: malloc");
		exit(EXIT_FAILURE);
	}
	ssize_t len;
	int fd_r = carr[this].to_chld[0];
	int fd_w = carr[this].to_prnt[1];
	char msg_ok = 0;

	do {
		len = read(fd_r, buf, CBUF_SIZE);
		HANDLE_ERR(len, "read");
		HANDLE_ERR(write(fd_w, buf, len), "write");
	} while (len);
	
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
