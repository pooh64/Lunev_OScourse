#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf("Wrong arg number\n");
		return 0;
	}
	
	// Get input fd
	int fd_inp = open(argv[1], O_RDONLY);
	if (fd_inp == -1) {
		perror("Error: inp_file open");
		return 0;
	}

	pid_t self_pid = getpid();

	// Make fifo_pid
	char fifo_filename[FIFO_NAME_MAX_LEN] = { };
	sprintf(fifo_filename, "%s%jx", 		\
		FIFO_NAME_PREFIX, (uintmax_t) self_pid);
	if (mkfifo(fifo_filename, 0777) == -1) {
		if (errno == EEXIST)
			errno = 0;
		else {
			perror("Error: mkfifo");
			return 0;
		}
	}
	
	// Save pid to pid_queue
	int fd_queue = open(FIFO_PATH_PID_QUEUE, O_CREAT | O_WRONLY, 0777);
	if (fd_queue == -1) {
		perror("Error: open pid_queue");
		return 0;
	}
	if (write(fd_queue, &self_pid, sizeof(pid_t)) == -1) {
		perror("Error: write to pid_queue");
		return 0;
	}
	if (close(fd_queue) == -1) {
		perror("Error: close pid_queue");
		return 0;
	}

	// Open fifo and send data
	int fd_fifo = open(fifo_filename, O_WRONLY);
	if (fd_fifo == -1) {
		perror("Error: open fifo");
		return 0;
	}
	if (fdcpy(fd_fifo, fd_inp) == -1) {
		perror("Error: fdcpy");
		return 0;
	}

	// Close inp, fifo.	
	if (close(fd_fifo) == -1) {
		perror("Error: close fifo");
		return 0;
	}
	if (close(fd_inp) == -1) {
		perror("Error: close inp_file");
		return 0;
	}

	return 0;
}
