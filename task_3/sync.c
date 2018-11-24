#include "sync.h"

int pair_capture(int semid, struct syncbuf self, struct syncbuf othr,
		 int (*self_init) (int))
{
	struct sembuf sop_buf[4];
	int sop_n = 0;

	// Capture lock
	SOPBUF_ADD(self.lock, 0, 0);
	SOPBUF_ADD(self.actv, 0, 0);
	SOPBUF_ADD(othr.actv, 0, 0);
	SOPBUF_ADD(self.lock, 1, SEM_UNDO);
	if (SOPBUF_SEMOP() == -1) {		/* Enter critical section 1 */
		perror("Error: semop");
		return -1;
	}
	
	// User-defined init
	if (self_init != NULL && (*self_init) (semid) == -1) {
		fprintf(stderr, "Error: self_init failed\n");
		return -1;
	}
	
	// Activate self
	SOPBUF_ADD(othr.lock, -1, 0);
	SOPBUF_ADD(othr.lock,  1, 0);
	SOPBUF_ADD(self.actv,  1, SEM_UNDO);
	if (SOPBUF_SEMOP() == -1) {		/* Not a start of critical section */
		perror("Error: semop");
		return -1;
	}
	
	// Wait until the other is activated
	SOPBUF_ADD(othr.lock, -1, IPC_NOWAIT);
	SOPBUF_ADD(othr.lock,  1, 0);
	SOPBUF_ADD(othr.actv, -1, 0);
	SOPBUF_ADD(othr.actv,  1, 0);
	if (SOPBUF_SEMOP() == -1) {
		perror("Error: semop");
		return -1;
	}

	return 0;
}

int pair_release(int semid, struct syncbuf self, struct syncbuf othr)
{
	struct sembuf sop_buf[5];
	int sop_n = 0;

	// Process ready for exit
	SOPBUF_ADD(self.done,  1, SEM_UNDO);
	if (SOPBUF_SEMOP() == -1) {
		perror("Error: semop");
		return -1;
	}
	
	// Wait for othr, release lock
	SOPBUF_ADD(othr.actv, -1, IPC_NOWAIT);
	SOPBUF_ADD(othr.actv,  1, 0);
	SOPBUF_ADD(othr.done, -1, 0);
	SOPBUF_ADD(othr.done,  1, 0);
	SOPBUF_ADD(self.lock, -1, SEM_UNDO);
	if (SOPBUF_SEMOP() == -1) {		/* Leave critical section 1 */
		if (errno == EAGAIN)
			fprintf(stderr, "Error: othr process failed\n");
		else
			perror("Error: semop");
		return -1;
	}
	
	// Success, release actv
	SOPBUF_ADD(othr.lock,  0, 0);
	SOPBUF_ADD(self.actv, -1, SEM_UNDO);
	if (SOPBUF_SEMOP() == -1) {
		perror("Error: semop");
		return -1;
	}

	return 0;
}
