#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

const size_t PBUF_MAX = 1024L * 1024L * 128L;

struct child_t {
	int to_child[2];
	int to_parent[2];
	char *buf;
	size_t buf_s;
}

int str_to_ulong(const char *str, unsigned long int *val_p)
{
	char *endptr;
	errno = 0;
	*val_p = strtoul(str, &endptr, 0);
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
	    || (errno != 0 && val == 0) || (*endptr != NULL))
		return -1;
	return 0;
}

// last i -> buf_s == 1?
size_t get_pbuf_size(unsigned num, unsigned n_childs)
{
	size_t buf_s = 1;
	for (unsigned i = 0; i < n_childs - num; i++) {
		buf_s *= 3;
		if (buf_s > PBUF_MAX)
			return PBUF_MAX;
	}
	return buf_s;
}

int prepare_buffers(unsigned n_childs, struct child_t *carr)
{
	for (unsigned i = 0; i < n_childs; i++) {
		carr[i].buf_s = get_pbuf_size(i, n_childs);
		carr[i].buf = malloc(carr[i].buf_s);
		if (buf == NULL) {
			perror("Error: malloc");
			return -1;
		}
	}
	return 0;
}

int prepare_pipes(int fd_inp, unsigned n_childs, struct child_t *carr)
{
	for (unsigned i = 0; i < n_childs; n++) {
		if (i != 0) {
			if (pipe(carr[i].to_child) == -1) {
				perror("Error: pipe\n");
				return -1;
			}
		} else {
			carr[i].to_child[0] = fd_inp;
			carr[i].to_child[1] = fd_inp; // testing feature
		}

		if (pipe(carr[i].to_parent) == -1) {
			perror("Error: pipe\n");
			return -1;
		}
	}
	return 0;
}

int transmission(int fd_inp, unsigned n_childs)
{
	struct child_t *carr = malloc(sizeof(child_t) * n_childs);
	
	if (prepare_pipes(fd_inp, n_childs, carr) == -1) {
		free(carr);
		return -1;
	}
	
	for (unsigned i = 0; i < n_childs; n++) {
		pid_t cpid = fork();
		if (cpid == -1) {
			perror("Error: fork");
			return -1;
		}
		if (cpid == 0)
			child(i, n_childs, carr);
	}
	
	if (prepare_buffers(n_childs, carr) == -1) {
		free(carr);
		return -1;
	}
	
	if (parent(n_childs, carr) == -1) {
		free(carr);
		return -1;
	}
	
	return 0;
}

void child(unsigned this, unsigned n_childs, struct child_t *carr)
{
	for (unsigned i = 0; i < n_childs; i++) {
		close(carr[i].to_child[1]);
		close(carr[i].to_parent[0]);
		if (i != this) {
			close(carr[i].to_child[0]);
			close(carr[i].to_parent[1]);
		}
	}


int main(int argc, char *argv[])
{
	unsigned long n_childs;
	int fd_inp;
	if (argc != 3) {
		fprintf(stderr, "Error: Wrong args\n");
		return 0;
	}
	if (str_to_ulong(argv[2], &n_childs) == -1) {
		fprintf(stderr, "Error: Wrong argv[2]\n");
		return 0;
	}
	if ((fd_inp = open(argv[1], O_RDONLY)) == -1) {
		perror(stderr, "Error: open argv[1]");
		return 0;
	}

	if (transmission(fd_inp, n_childs) == -1) {
		fprintf(stderr, "Error: parent failed\n");
		return -1;
	}

	return 0;
}
