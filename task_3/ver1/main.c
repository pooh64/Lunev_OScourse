#include "transfer.h"

	
int prep_sem(int semid)
{
	struct sembuf sop[SEM_MAX + 1] = { };
	SOP_SET(sop[SEM_PREP],		SEM_PREP,	0, IPC_NOWAIT);
	SOP_SET(sop[SEM_FILL],		SEM_FILL,	0, 0);
	SOP_SET(sop[SEM_EMPTY],		SEM_EMPTY,	1, 0);
	
	SOP_SET(sop[SEM_SND_LOCK],	SEM_SND_LOCK,	1, 0);
	SOP_SET(sop[SEM_SND_INIT],	SEM_SND_INIT,	1, 0);
	SOP_SET(sop[SEM_SND_ACTV],	SEM_SND_ACTV,	0, 0);
	SOP_SET(sop[SEM_SND_RUN],	SEM_SND_RUN,	0, 0);
	
	SOP_SET(sop[SEM_REC_LOCK], 	SEM_REC_LOCK,	1, 0);
	SOP_SET(sop[SEM_REC_INIT], 	SEM_REC_INIT,	1, 0);
	SOP_SET(sop[SEM_REC_ACTV], 	SEM_REC_ACTV,	0, 0);
	SOP_SET(sop[SEM_REC_RUN],	SEM_REC_RUN,	0, 0);
	// Capture if init
	SOP_SET(sop[SEM_MAX],		SEM_PREP,	1, 0);
	if (semop(semid, sop, SEM_MAX + 1) == -1 && errno != EAGAIN)
		return -1;
	return 0;
}

ssize_t memtofd_cpy(int fd, char *buf, size_t count)
{	
	ssize_t tmp = 1;
	size_t len = count;
	while (len && tmp)
	{
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
	while (len && tmp)
	{
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


int pair_capture(int semid, struct syncbuf self, struct syncbuf othr, int (*self_init)(int))
{
	struct sembuf sop[3] = { };

	SOP_SET(sop[0], self.lock,	-1, SEM_UNDO);	// p(self.lock)
	SOP_SET(sop[1], self.run,	 0, 0);		// wait(self.run)
	SOP_SET(sop[2], othr.run,	 0, 0);		// wait(othr.run)
	if (semop(semid, sop, 3) == -1) {
		perror("Error: semop");
		return -1;
	}
	
	// User-defined function
	if (self_init != NULL && (*self_init)(semid) == -1) {
		fprintf(stderr, "Error: self_init failed\n");
		return -1;
	}

	SOP_SET(sop[0], self.init,	-1, SEM_UNDO);	// p(self.init)
	SOP_SET(sop[1], self.actv,	 1, SEM_UNDO);	// v(self.actv)
	if (semop(semid, sop, 2) == -1) {
		perror("Error: semop");
		return -1;
	}
	
	SOP_SET(sop[0], othr.init,	 0, 0);		// wait(othr.init)
	SOP_SET(sop[1], self.run,	 1, SEM_UNDO);	// v(self.run)
	if (semop(semid, sop, 2) == -1) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}

int pair_release(int semid, struct syncbuf self, struct syncbuf othr)
{
	struct sembuf sop[4] = { };
	
	SOP_SET(sop[0], self.actv,	-1, SEM_UNDO);	// p(self.actv)
	if (semop(semid, sop, 1) == -1) {
		perror("Error: semop");
		return -1;
	}							
							
	SOP_SET(sop[0], othr.lock,	 0, IPC_NOWAIT);// check(othr.lock)
	SOP_SET(sop[1], othr.actv,	 0, 0);		// wait(othr.actv)
	SOP_SET(sop[2], self.init,	 1, SEM_UNDO);	// v(self.init)
	SOP_SET(sop[3], self.run,	-1, SEM_UNDO);	// p(self.run)
	if (semop(semid, sop, 4) == -1) {
		if (errno == EAGAIN)
			fprintf(stderr, "Error: other process failed\n");
		else
			perror("Error: semop");
		return -1;
	}
	
	// Success
	
	SOP_SET(sop[0], othr.run,	 0, 0);		// wait(othr.run)
	SOP_SET(sop[1], self.lock,	 1, SEM_UNDO);	// v(self.lock)
	if (semop(semid, sop, 2) == -1) {
		perror("Error: semop");
		return -1;
	}
	return 0;
}


int sender_run(int semid, struct syncbuf othr, void *shm, size_t shm_size, int fd)
{
	struct sembuf sop_1[2], sop_2;
	SOP_SET(sop_1[0], othr.lock,	 0, IPC_NOWAIT);// check(othr.lock)
	SOP_SET(sop_1[1], SEM_EMPTY,	-1, 0);		// p(empty)
	
	SOP_SET(sop_2,    SEM_FILL,	 1, 0);		// v(fill)

	while (1) {
		if (semop(semid, sop_1, 2) == -1) {
			if (errno == EAGAIN)
				fprintf(stderr, "Error: receiver is dead\n");
			else
				perror("Error: semop");
			return -1;
		}

		ssize_t len = fdtomem_cpy(fd, (char*) shm + sizeof(size_t), shm_size - sizeof (size_t));
		if (len == -1) {
			fprintf(stderr, "Error: fdtomem_cpy failed\n");
			return -1;
		}
		*(ssize_t*) shm = len;

		if (semop(semid, &sop_2, 1) == -1) {
			perror("Error: semop");
			return -1;
		}
		
		if (len == 0)
			break;
	}
	return 0;
}

int receiver_run(int semid, struct syncbuf othr, void *shm, int fd)
{
	struct sembuf sop_1[2], sop_2;
	SOP_SET(sop_1[0], othr.lock,	 0, IPC_NOWAIT);// check(othr.lock)
	SOP_SET(sop_1[1], SEM_FILL,	-1, 0);		// p(fill)
	
	SOP_SET(sop_2,    SEM_EMPTY,	 1, 0);		// v(empty)

	while (1) {
		if (semop(semid, sop_1, 2) == -1) {
			if (errno == EAGAIN)
				fprintf(stderr, "Error: sender is dead\n");
			else
				perror("Error: semop\n");
			return -1;
		}

		ssize_t len = *(ssize_t*) shm;
		len = memtofd_cpy(fd, (char*) shm + sizeof(size_t), len);
		if (len == -1) {
			fprintf(stderr, "Error: memtofd_cpy failed\n");
			return -1;
		}

		if (semop(semid, &sop_2, 1) == -1) {
			perror("Error: semop\n");
			return -1;
		}

		if (len == 0)
			break;
	}
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


int sender(int semid, struct syncbuf self, struct syncbuf othr, void *shm, size_t shm_size, int fd)
{
	/// Sync pair
	if (pair_capture(semid, self, othr, &sender_init_run) == -1) {
		fprintf(stderr, "Error: pair_capture\n");
		return -1;
	}
	/// Run
	int tmp = sender_run(semid, othr, shm, shm_size, fd); 
	if (tmp == -1)
		fprintf(stderr, "Error: sender_run failed\n");
	/// Unlock pair
	if (pair_release(semid, self, othr) == -1) {
		perror("Error: pair_release");
		return -1;
	}
	if (tmp == -1)
		return -1;
	return 0;
}

int receiver(int semid, struct syncbuf self, struct syncbuf othr, void *shm, int fd)
{
	/// Get pair
	if (pair_capture(semid, self, othr, NULL) == -1) {
		fprintf(stderr, "Error: pair_capture\n");
		return -1;
	}
	/// Run
	int tmp = receiver_run(semid, othr, shm, fd); 
	if (tmp == -1)
		fprintf(stderr, "Error: reciever_run failed\n");
	/// Unlock pair
	if (pair_release(semid, self, othr) == -1) {
		perror("Error: pair_release");
		return -1;
	}
	if (tmp == -1)
		return -1;
	return 0;
}



int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Error: Wrong argv");
		return 0;
	}

	int key_fd = creat(KEY_PATHNAME, 0644);
	if (key_fd == -1 && errno != EEXIST) {
		perror("Error: creat");
		return -0;
	}
	key_t key = ftok(KEY_PATHNAME, 0);
	if (key == -1) {
		perror("Error: ftok");
		return 0;
	}
	int semid = semget(key, SEM_MAX, 0644 | IPC_CREAT);
	if (semid == -1) {
		perror("Error: semget");
		return 0;
	}
	long shm_size = sysconf(_SC_PAGESIZE);
	int shmid = shmget(key, shm_size, 0644 | IPC_CREAT);
	if (shmid == -1) {
		perror("Error: shmget");
		return 0;
	}
	void *shm = shmat(shmid, NULL, 0);
	if (shm == (void*) -1) {
		perror("Error: shmat");
		return 0;
	}

	if (prep_sem(semid) == -1) {
		fprintf(stderr, "Error: prep_sem failed\n");
		return 0;
	}
	
	struct syncbuf rec;
	rec.lock = SEM_REC_LOCK;
	rec.init = SEM_REC_INIT;
	rec.actv = SEM_REC_ACTV;
	rec.run  = SEM_REC_RUN;
	
	struct syncbuf snd;
	snd.lock = SEM_SND_LOCK;
	snd.init = SEM_SND_INIT;
	snd.actv = SEM_SND_ACTV;
	snd.run  = SEM_SND_RUN;

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
