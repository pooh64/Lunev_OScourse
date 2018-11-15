#include <fcntl.h>
#include <stdlib.h>
#include "sync.h"

const char KEY_PATHNAME[] = "key.key";

enum SEM_NUMS {
	SEM_FILL,		// Do not use with SEM_UNDO!
	SEM_EMPTY,		// Do not use with SEM_UNDO!

	SEM_SND_LOCK,
	SEM_SND_ACTV,
	SEM_SND_DONE,

	SEM_REC_LOCK,
	SEM_REC_ACTV,
	SEM_REC_DONE,

	SEM_NUM,
};

ssize_t memtofd_cpy(int fd, char *buf, size_t count)
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

int sender_run(int semid, struct syncbuf othr, void *shm, size_t shm_size,
	       int fd)
{
	struct sembuf sop_buf[3];
	int sop_n = 0;
	ssize_t len;

	do {
		// Check othr process, p(empty)
		SOPBUF_ADD(othr.actv, -1, IPC_NOWAIT);
		SOPBUF_ADD(othr.actv, 1, 0);
		SOPBUF_ADD(SEM_EMPTY, -1, 0);
		if (SOPBUF_SEMOP() == -1) {	/* Enter critical section 2 */
			if (errno == EAGAIN)
				fprintf(stderr, "Error: receiver is dead\n");
			else
				perror("Error: semop");
			exit(EXIT_FAILURE);
		}

		len = fdtomem_cpy(fd, (char *)shm + sizeof(size_t),
				  shm_size - sizeof(size_t));
		if (len == -1) {
			fprintf(stderr, "Error: fdtomem_cpy failed\n");
			exit(EXIT_FAILURE);
		}
		*(ssize_t *) shm = len;

		// v(fill)
		SOPBUF_ADD(SEM_FILL, 1, 0);
		if (SOPBUF_SEMOP() == -1) {	/* Leave critical section 2 */
			perror("Error: semop");
			exit(EXIT_FAILURE);
		}
	} while (len > 0);
	return 0;
}

int receiver_run(int semid, struct syncbuf othr, void *shm, int fd)
{
	struct sembuf sop_buf[3];
	int sop_n = 0;
	ssize_t len;

	do {
		// Check othr process, p(fill)
		SOPBUF_ADD(othr.actv, -1, IPC_NOWAIT);
		SOPBUF_ADD(othr.actv, 1, 0);
		SOPBUF_ADD(SEM_FILL, -1, 0);
		if (SOPBUF_SEMOP() == -1) {	/* Enter critical section 2 */
			if (errno == EAGAIN)
				fprintf(stderr, "Error: sender is dead\n");
			else
				perror("Error: semop\n");
			exit(EXIT_FAILURE);
		}

		len = *(ssize_t *) shm;
		if (len == -1) {
			fprintf(stderr, "Error: sender fdtomem_cpy failed\n");
			exit(EXIT_FAILURE);
		}
		len = memtofd_cpy(fd, (char *)shm + sizeof(size_t), len);
		if (len == -1) {
			fprintf(stderr, "Error: memtofd_cpy failed\n");
			exit(EXIT_FAILURE);
		}
		// v(empty)
		SOPBUF_ADD(SEM_EMPTY, 1, 0);
		if (SOPBUF_SEMOP() == -1) {	/* Leave critical section 2 */
			perror("Error: semop\n");
			exit(EXIT_FAILURE);
		}
	} while (len > 0);
	return 0;
}

int sender_init_run(int semid)
{
	union semun arg;
	arg.val = 0;
	if (semctl(semid, SEM_FILL, SETVAL, arg) == -1) {
		perror("Error: semctl");
		return -1;
	}
	arg.val = 1;
	if (semctl(semid, SEM_EMPTY, SETVAL, arg) == -1) {
		perror("Error: semctl");
		return -1;
	}
	return 0;
}

int sender(int semid, struct syncbuf self, struct syncbuf othr, void *shm,
	   size_t shm_size, int fd)
{
	// Capture and sync pair
	if (pair_capture(semid, self, othr, &sender_init_run) == -1) {
		fprintf(stderr, "Error: pair_capture\n");
		return -1;
	}
	// Run
	int tmp = sender_run(semid, othr, shm, shm_size, fd);
	if (tmp == -1)
		fprintf(stderr, "Error: sender_run failed\n");
	// Unlock pair
	if (pair_release(semid, self, othr) == -1) {
		perror("Error: pair_release");
		return -1;
	}
	if (tmp == -1)
		return -1;
	return 0;
}

int receiver(int semid, struct syncbuf self, struct syncbuf othr, void *shm,
	     int fd)
{
	// Capture and sync pair
	if (pair_capture(semid, self, othr, NULL) == -1) {
		fprintf(stderr, "Error: pair_capture\n");
		return -1;
	}
	// Run
	int tmp = receiver_run(semid, othr, shm, fd);
	if (tmp == -1)
		fprintf(stderr, "Error: reciever_run failed\n");
	// Unlock pair
	if (pair_release(semid, self, othr) == -1) {
		perror("Error: pair_release");
		return -1;
	}
	if (tmp == -1)
		return -1;
	return 0;
}

int get_resources(const char *path, int *semid, int *shmid, void **shm_p,
		  size_t * shm_size)
{
	int key_fd = creat(path, 0644);
	if (key_fd == -1 && errno != EEXIST) {
		perror("Error: creat");
		return -1;
	}
	key_t key = ftok(path, 0);
	if (key == -1) {
		perror("Error: ftok");
		return -1;
	}
	*semid = semget(key, SEM_NUM, 0644 | IPC_CREAT);
	if (*semid == -1) {
		perror("Error: semget");
		return -1;
	}
	*shm_size = sysconf(_SC_PAGESIZE);
	*shmid = shmget(key, *shm_size, 0644 | IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		return -1;
	}
	*shm_p = shmat(*shmid, NULL, 0);
	if (*shm_p == (void *)-1) {
		perror("Error: shmat");
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Error: Wrong argv");
		return 0;
	}

	int semid, shmid;
	void *shm;
	long shm_size;
	if (get_resources(KEY_PATHNAME, &semid, &shmid, &shm, &shm_size) == -1) {
		fprintf("Error: get_resorces failed\n");
		return -1;
	}

	struct syncbuf rec;
	rec.lock = SEM_REC_LOCK;
	rec.actv = SEM_REC_ACTV;
	rec.done = SEM_REC_DONE;

	struct syncbuf snd;
	snd.lock = SEM_SND_LOCK;
	snd.actv = SEM_SND_ACTV;
	snd.done = SEM_SND_DONE;

	if (argc == 1) {
		if (receiver(semid, rec, snd, shm, STDOUT_FILENO) == -1)
			fprintf(stderr, "Error: receiver failed\n");
	} else {
		int fd = open(argv[1], O_RDONLY);
		if (fd == -1) {
			perror("Error: open");
			return 0;
		}
		if (sender(semid, snd, rec, shm, shm_size, fd) == -1)
			fprintf(stderr, "Error: sender failed\n");
	}

	return 0;
}
