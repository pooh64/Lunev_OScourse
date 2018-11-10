#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>

const char KEY_PATHNAME[] = "key.key";

enum SEM_NUMS {
	SEM_PREP = 0,
	SEM_FILL,	// Do not use with SEM_UNDO!
	SEM_EMPTY,	// Do not use with SEM_UNDO!
	
	SEM_SND_LOCK,
	SEM_SND_INIT,
	SEM_SND_ACTV,
	SEM_SND_RUN,
	
	SEM_REC_LOCK,
	SEM_REC_INIT,
	SEM_REC_ACTV,
	SEM_REC_RUN,
	
	SEM_MAX, 	// sizeof
};

union semun {
	int          	 val;	/* Value for SETVAL */
	struct semid_ds *buf;	/* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;	/* Array for GETALL, SETALL */
	struct seminfo  *__buf;	/* Buffer for IPC_INFO
				(Linux-specific) */
};

struct syncbuf {
	unsigned short lock;
	unsigned short init;
	unsigned short actv;
	unsigned short run;
};

#define SOP_SET(sop, num, op, flg)	\
	do {				\
		sop.sem_num = num;	\
		sop.sem_op  = op;	\
		sop.sem_flg = flg;	\
	} while (0)

ssize_t memtofd_cpy(int fd, char *buf, size_t count);
ssize_t fdtomem_cpy(int fd, char *buf, size_t count);

int pair_capture(int semid, struct syncbuf self, struct syncbuf othr, int (*self_init)(int));
int pair_release(int semid, struct syncbuf self, struct syncbuf othr);

int receiver(int semid, struct syncbuf self, struct syncbuf othr, void *shm, int fd);
int sender  (int semid, struct syncbuf self, struct syncbuf othr, void *shm, size_t shm_size, int fd);
int sender_init_run(int semid);
int receiver_run(int semid, struct syncbuf othr, void *shm, int fd);
int sender_run  (int semid, struct syncbuf othr, void *shm, size_t shm_size, int fd);
