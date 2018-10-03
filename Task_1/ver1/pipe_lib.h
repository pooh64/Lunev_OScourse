

const char QUEUE_PATH_NAME[] = "pid.queue";
const char FIFO_PATH_NAME[] = "fifo_pid.";
const int  FIFO_PATH_MAX_LEN = sizeof(FIFO_NAME_PREFIX) \
			       + sizeof(uintmax_t) * 2 + 1;

#define FDTOFD_CPY_BUF_SIZE 512

ssize_t buftofd_cpy(int fd_out, char *buf, size_t count)
{	
	ssize_t tmp = 1;
	size_t len = count;
	while (len && tmp)
	{
		tmp = write(fd_out, buf, len);
		if (tmp == -1) {
			if (errno == EINTR) {
				errno = 0;
				continue;
			}
			return -1;
		}
		len -= tmp;
		buf += tmp;
	}
	return count - len;
}
	
		

ssize_t fdtofd_cpy(int fd_out, int fd_in)
{
	char *buf = malloc(FDCPY_BUF_SIZE);
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
