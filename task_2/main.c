#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>


int child(int qid, pid_t n)
{
	// Wait for msg with type = n
	long msg = (long) n;
	if (msgrcv(qid, &msg, 0, msg, 0) == -1) {
		perror("Error: child: msgrcv");
		return -1;
	}

	printf("n = %5d pid = %5ld\n", n, (long) getpid());
	fflush(stdout);

	// Send msg with type = n + 1
	msg = (long) n + 1;
	if (msgsnd(qid, &msg, 0, 0) == -1) {
		perror("Error: child: msgsnd");
		return -1;
	}
	return 0;
}

int parent(int n)
{	
	// Create msg queue
	int qid = msgget(IPC_PRIVATE, 0644);
	if (qid == -1) {
		perror("Error: msgget");
		return -1;
	}

	// Create childs
	pid_t cur, first;
	for (pid_t i = 0; i < n; i++) {
		cur = fork();
		if (cur == 0) {
			child(qid, i + 1);
			return 0;
		}
		if (cur == -1) {
			perror("fork");
			return -1;
		}
		if (i == 0)
			first = cur;
	}

	printf("--------Childs:--------\n");
	long msg = 1;
	// Send msg to first child
	if (msgsnd(qid, &msg, 0, 0) == -1) {
		perror("msgsnd");
		msgctl(qid, IPC_RMID, NULL);
		return -1;
	}
	// Receive msg from last child
	if (msgrcv(qid, &msg, 0, n + 1, 0) == -1) {
		perror("msgrcv");
		msgctl(qid, IPC_RMID, NULL);
		return -1;
	}
	printf("--------\\Childs--------\n");
	// Close msg queue
	if (msgctl(qid, IPC_RMID, NULL) == -1) {
		perror("msgctl");
		return -1;
	}
	return 0;
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Error: Wrong number of args\n");
		return 0;
	}

	char *end;
	errno = 0;
	long n = strtol(argv[1], &end, 0);
	if (errno || *end != '\0' || n < 1 || (pid_t) n != n) {
		fprintf(stderr, "Error: Wrong param\n");
		return 0;
	}

	if (parent(n) == -1)
		fprintf(stderr, "Error: parent failed\n");
	
	return 0;
}
