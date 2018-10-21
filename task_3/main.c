#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


/// Copy buffer size
const int CPY_BUF_SIZE = 512;

ssize_t memtofd_cpy(int fd_out, char *buf, size_t count)
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

