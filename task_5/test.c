#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


int main()
{
	int fd = open("fifo_test", O_RDWR);
	if (fd == -1)
		return 0;

	char buf[1024];
	ssize_t len;

	while (1) {
		len = read(STDIN_FILENO, buf, sizeof(buf));
		if (len == -1)
			return 0;
		len = write(fd, buf, len);
	}
	return 0;
}
