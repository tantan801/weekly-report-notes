/*
 * ledApp.c —— 第一周作业 test3：用 ioctl 控制 LED
 *
 * 用法：
 *   ./ledApp /dev/newchrled on
 *   ./ledApp /dev/newchrled off
 *   ./ledApp /dev/newchrled get
 *   ./ledApp /dev/newchrled toggle
 *   ./ledApp /dev/newchrled 1 | 0      # 兼容原版数字写法
 *
 * 编译：
 *   arm-linux-gnueabihf-gcc ledApp.c -o ledApp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#define LED_IOC_MAGIC 'l'
#define LED_ON   _IOW(LED_IOC_MAGIC, 1, int)
#define LED_OFF  _IOW(LED_IOC_MAGIC, 2, int)
#define LED_GET  _IOR(LED_IOC_MAGIC, 3, int)

static void usage(const char *p)
{
	printf("Usage: %s <dev> on|off|get|toggle|1|0\n", p);
	printf("  e.g. %s /dev/newchrled on\n", p);
}

int main(int argc, char *argv[])
{
	int fd, state, i;

	if (argc != 3) {
		usage(argv[0]);
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		printf("open %s failed: %s\n", argv[1], strerror(errno));
		return -1;
	}

	if (!strcmp(argv[2], "on") || !strcmp(argv[2], "1")) {
		if (ioctl(fd, LED_ON, 0) < 0) {
			perror("ioctl LED_ON");
			close(fd);
			return -1;
		}
		printf("LED ON\n");
	} else if (!strcmp(argv[2], "off") || !strcmp(argv[2], "0")) {
		if (ioctl(fd, LED_OFF, 0) < 0) {
			perror("ioctl LED_OFF");
			close(fd);
			return -1;
		}
		printf("LED OFF\n");
	} else if (!strcmp(argv[2], "get")) {
		state = ioctl(fd, LED_GET, 0);
		if (state < 0) {
			perror("ioctl LED_GET");
			close(fd);
			return -1;
		}
		printf("LED state = %d (%s)\n", state, state ? "ON" : "OFF");
	} else if (!strcmp(argv[2], "toggle")) {
		for (i = 0; i < 3; i++) {
			ioctl(fd, LED_ON, 0);
			printf("ON\n");
			usleep(300 * 1000);
			ioctl(fd, LED_OFF, 0);
			printf("OFF\n");
			usleep(300 * 1000);
		}
	} else {
		usage(argv[0]);
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}
