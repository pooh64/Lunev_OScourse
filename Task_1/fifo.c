#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

// Value to identify pair
struct ident_t {
	pid_t pid;
};

/// Fifo with id list
const char   FIFO_QUEUE_PATH[] = "queue.fifo";
const char   FIFO_CHANNEL_PATH_PREFIX[] = "channel.";
const int    FIFO_CHANNEL_PATH_MAX = sizeof(FIFO_CHANNEL_PATH_PREFIX) + \
				     sizeof(struct ident_t) * 2;
const mode_t FIFO_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP	|	\
			 S_IROTH | S_IWOTH;

/// Timeouts
const int FIFO_MAX_SLEEP_US = ((unsigned) 2) << 20;
const int FIFO_MIN_SLEEP_US = ((unsigned) 0) << 1;

/// Copy buffer size (atomic)
const int FDTOFD_CPY_BUF_SIZE = 512;

ssize_t buftofd_cpy(int fd_out, char *buf, size_t count)
{	
	ssize_t tmp = 1;
	size_t len = count;
	while (len && tmp)
	{
		tmp = write(fd_out, buf, len);
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

ssize_t fdtofd_cpy(int fd_out, int fd_in)
{
	char *buf = malloc(FDTOFD_CPY_BUF_SIZE);
	if (buf == NULL)
		return -1;
	ssize_t sumary = 0;
	while (1) {
		ssize_t len = read(fd_in, buf, FDTOFD_CPY_BUF_SIZE);
		if (len == -1) {
			if (errno == EINTR) {
				errno = 0;
				continue;
			}
			free(buf);
			return -1;
		} else if (len == 0)
			break;
		
		ssize_t tmp = buftofd_cpy(fd_out, buf, len);
		if (tmp == -1) {
			free(buf);
			return -1;
		}
		sumary += tmp;
		if (tmp != len)
			break;
	}
	free(buf);
	return sumary;
}

struct ident_t get_id()
{
	struct ident_t id;
	id.pid = getpid();
	return id;
}

int get_channel_path(char **buf_p, struct ident_t id)
{
	*buf_p = malloc(sizeof(char) * FIFO_CHANNEL_PATH_MAX);
	if (buf_p == NULL)
		return -1;
	return sprintf(*buf_p, "%s%jx\0", FIFO_CHANNEL_PATH_PREFIX, (uintmax_t) id.pid);
}

int init_channel(char *path, mode_t mode)
{
	int fd;	
	
	// Try to create fifo
	if (mkfifo(path, mode) != -1)
		return 0;
	if (errno != EEXIST) {
		perror("Error: mkfifo");
		return -1;
	}

	// If channel exist
	fd = open(path, O_WRONLY | O_NONBLOCK);
	if (fd == -1) {
		if (errno != ENXIO) {	// If channel closed for write
			perror("Error: open");
			return -1;
		} else // Channel closed
			return 0;
	}	
	
	// Channel exist and open for read
	while (1) {
		usleep(FIFO_MIN_SLEEP_US);
		if (close(fd) == -1) {
			perror("Error: close");
			return -1;
		}
		fd = open(path, O_WRONLY | O_NONBLOCK);
		if (fd != -1)
			continue;
		if (errno == ENXIO) // Channel closed for read
			break;
		else {
			perror("Error: open");
			return -1;
		}
	}
	return 0;
}

/// returns 1 if msg received
int receiver_wait_byte(int fd)
{
	int delay = FIFO_MIN_SLEEP_US;
	char msg;
	int tmp;
	for (; delay <= FIFO_MAX_SLEEP_US; delay *= 2) {
		tmp = read(fd, &msg, sizeof(msg));
		if (tmp > 0)
			return 1;
		if (tmp == -1 && errno != EAGAIN) {
			return -1;
		}
		usleep(delay);
	}
	return 0;
}	


#define ERR_HANDLER(str_msg)			\
do {						\
	fprintf(stderr, "Error: " str_msg);	\
	fprintf(stderr, "Line: %d\n", __LINE__);\
	unlink(channel_path);			\
	free(channel_path);			\
	return -1;				\
} while (0)		

#define ERR_HANDLER_PERR(str_msg)		\
do {						\
	perror("Error: " str_msg);		\
	fprintf(stderr, "Line: %d\n", __LINE__);\
	unlink(channel_path);			\
	free(channel_path);			\
	return -1;				\
} while (0)	
	
int receiver()
{	
	// Prepare and open channel
	struct ident_t id = get_id();
	char *channel_path;
	get_channel_path(&channel_path, id);
	if (init_channel(channel_path, FIFO_MODE) == -1)
		ERR_HANDLER("init channel");
		
	int fd_channel = open(channel_path, O_RDONLY | O_NONBLOCK);
	if (fd_channel == -1)
		ERR_HANDLER_PERR("open channel");
	
	// Create/open queue and write id
	if (mkfifo(FIFO_QUEUE_PATH, FIFO_MODE) == -1 && errno != EEXIST)
		ERR_HANDLER_PERR("mkfifo queue");

	int fd_queue = open(FIFO_QUEUE_PATH, O_RDWR);
	if (fd_queue == -1)
		ERR_HANDLER_PERR("open queue");
	
	if (write(fd_queue, &id, sizeof(id)) != sizeof(id))
		ERR_HANDLER_PERR("write id to queue");
	
	// Wait for sender
	if (receiver_wait_byte(fd_channel) != 1)
		ERR_HANDLER("It seems the sender is dead\n");
	
	// Close queue fifo
	if (close(fd_queue) == -1)
		ERR_HANDLER_PERR("close queue");
	
	// Set new flags, write data, close file
	if (fcntl(fd_channel, F_SETFL, O_RDONLY) == -1)
		ERR_HANDLER_PERR("fcntl channel");
		
	if (fdtofd_cpy(STDOUT_FILENO, fd_channel) == -1)
		ERR_HANDLER("copy from channel to stdout");
		
	if (close(fd_channel) == -1)
		ERR_HANDLER_PERR("close channel");
	
	unlink(channel_path);
	free(channel_path);
	return 0;
}


int sender(const char *inp_path)
{
	// Open input file
	int fd_inp = open(inp_path, O_RDONLY);
	if (fd_inp == -1) {
		perror("Error: open input file");
		return -1;
	}

	// Open queue
	int fd_queue;
	while (1) {
		fd_queue = open(FIFO_QUEUE_PATH, O_RDONLY);
		if (fd_queue == -1) {
			if (errno != ENOENT) {
				perror("Error: open queue");
				return -1;
			}
		} else
			break;
		usleep(FIFO_MIN_SLEEP_US);
	}
	
	// Read id from queue
	struct ident_t id;
	while (1) {
		int tmp = read(fd_queue, &id, sizeof(id));
		if (tmp == sizeof(id))
			break;
		if (tmp == -1) {
			perror("Error: Sender: read id");
			return -1;
		}
		usleep(FIFO_MIN_SLEEP_US);
	}
	
	// Open channel by id
	char *channel_path;
	get_channel_path(&channel_path, id);
	int fd_channel = open(channel_path, O_NONBLOCK | O_WRONLY);
	if (fd_channel == -1) 
		ERR_HANDLER_PERR("It seems the receiver is dead");
		
	// Set new flags
	if (fcntl(fd_channel, F_SETFL, O_WRONLY) == -1)
		ERR_HANDLER_PERR("fcntl channel");
	
	// Sending msg that sender is ready
	char msg = 1;
	if (buftofd_cpy(fd_channel, &msg, sizeof(msg)) == -1)
		ERR_HANDLER("send sync-msg");
		
	// Send data
	if (fdtofd_cpy(fd_channel, fd_inp) == -1)
		ERR_HANDLER("copy from input to channel");
		
	if (close(fd_channel) == -1)
		ERR_HANDLER_PERR("close channel");
		
	if (close(fd_inp) == -1)
		ERR_HANDLER_PERR("close input file");
		
	unlink(channel_path);
	free(channel_path);
	return 0;
}

#undef ERR_HANDLER
#undef ERR_HANDLER_PERR


int main(int argc, char *argv[]) 
{
	if (argc == 2) {
		if (sender(argv[1]) == -1)
			fprintf(stderr, "Error: Sender failed\n");
	} else if (argc == 1) {
		if (receiver() == -1)
			fprintf(stderr, "Error: Receiver failed\n");
	} else 
		fprintf(stderr, "Error: wrong argument list\n");
	return 0;
}
