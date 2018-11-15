#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>

#include <stdio.h>
#include <errno.h>

union semun {
	int val;		/* Value for SETVAL */
	struct semid_ds *buf;	/* Buffer for IPC_STAT, IPC_SET */
	unsigned short *array;	/* Array for GETALL, SETALL */
	struct seminfo *__buf;	/* Buffer for IPC_INFO
				   (Linux-specific) */
};

struct syncbuf {
	unsigned short lock;
	unsigned short actv;
	unsigned short done;
};

#define SOPBUF_ADD(num, op, flg)		\
	do {					\
		sop_buf[sop_n].sem_num = num;	\
		sop_buf[sop_n].sem_op  = op;	\
		sop_buf[sop_n].sem_flg = flg;	\
		sop_n++;			\
	} while (0)

#define SOPBUF_SEMOP()						\
	({							\
		int sop_status = semop(semid, sop_buf, sop_n);	\
		sop_n = 0;					\
		sop_status;					\
	})

int pair_capture(int semid, struct syncbuf self, struct syncbuf othr,
		 int (*self_init) (int));
int pair_release(int semid, struct syncbuf self, struct syncbuf othr);
