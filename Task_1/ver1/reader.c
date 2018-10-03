



int main()
{
	if (argc != 2) {
		fprintf("Wrong arg number\n");
		return 0;
	}

	pid_t self_pid = getpid();

	// Create/open FIFO_PATH_NAMEpid
	char fifo_path[FIFO_PATH_MAX_LEN] = { };
	sprintf(fifo_path, "%s%jx", 		\
		FIFO_PATH_NAME, (uintmax_t) self_pid);
	if (mkfifo(fifo_path, 0777) == -1) {
		if (errno == EEXIST)
			errno = 0;
		else {
			perror("Error: mkfifo FIFO_PATH_NAMEpid");
			return 0;
		}
	}
	
	// Open FIFO_PATH_NAMEpid for readonly, writeonly
	int fd_fifo_r = open(fifo_path, O_RDONLY | O_NONBLOCK);
	if (fd_fifo_r == -1) {
		perror("Error: open FIFO_PATH_NAMEpid RDONLY");
		return 0;
	}
	int fd_fifo_w = open(fifo_path, O_WRONLY | O_NONBLOCK);
	if (fd_fifo_w == -1) {
		perror("Error: open FIFO_PATH_NAMEpid WRONLY");
		return 0;
	}
	
	// Write argv[1] to FIFO_PATH_NAMEpid
	int inp_str_len = strlen(argv[1]);
	ssize_t tmp = buftofd_cpy(
	
	// Save self_pid to PATH_QUEUE
	int fd_queue = open(FIFO_PATH_QUEUE, O_CREAT | O_WRONLY, 0777);
	if (fd_queue == -1) {
		perror("Error: open PATH_QUEUE");
		return 0;
	}
	if (write(fd_queue, &self_pid, sizeof(pid_t)) == -1) {
		perror("Error: write to PATH_QUEUE");
		return 0;
	}
	if (close(fd_queue) == -1) {
		perror("Error: close PATH_QUEUE");
		return 0;
	}
	
	
	
	
	
	
	
	
	
	
	
	