/*
 * client.c —— 第一周作业 test2：TCP echo 客户端
 *
 * 用法：
 *   ./client_x86                      # 连 127.0.0.1:8888，进入交互模式
 *   ./client_x86 192.168.1.105        # 指定服务端 IP
 *   ./client_x86 192.168.1.105 8888   # 指定 IP + 端口
 *   ./client_x86 192.168.1.105 8888 "hello"   # 发一条就退出（脚本化验证用）
 *
 * 编译：
 *   gcc client.c -o client_x86
 *   arm-linux-gnueabihf-gcc client.c -o client_arm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEF_PORT  8888
#define BUF_SIZE  1024

int main(int argc, char *argv[])
{
	const char *ip     = (argc > 1) ? argv[1] : "127.0.0.1";
	int         port   = (argc > 2) ? atoi(argv[2]) : DEF_PORT;
	const char *onemsg = (argc > 3) ? argv[3] : NULL;   /* 给了就发一条退出 */
	int fd;
	struct sockaddr_in saddr;
	char buf[BUF_SIZE];

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port   = htons(port);
	if (inet_pton(AF_INET, ip, &saddr.sin_addr) != 1) {
		printf("bad ip: %s\n", ip);
		close(fd);
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		perror("connect");
		close(fd);
		return -1;
	}
	printf("client: connected to %s:%d\n", ip, port);

	if (onemsg) {                       /* 非交互模式：发一条就退 */
		ssize_t n;

		send(fd, onemsg, strlen(onemsg), 0);
		n = recv(fd, buf, sizeof(buf) - 1, 0);
		if (n > 0) {
			buf[n] = '\0';
			printf("client: echo: %s\n", buf);
		}
		close(fd);
		return 0;
	}

	while (1) {                         /* 交互模式：可以连着发多条 */
		ssize_t n;

		printf("> ");
		fflush(stdout);
		if (!fgets(buf, sizeof(buf), stdin))
			break;                  /* Ctrl+D 退出 */

		if (send(fd, buf, strlen(buf), 0) < 0) {
			perror("send");
			break;
		}
		if (strncmp(buf, "quit", 4) == 0)
			break;

		n = recv(fd, buf, sizeof(buf) - 1, 0);
		if (n <= 0) {
			printf("client: server closed\n");
			break;
		}
		buf[n] = '\0';
		printf("client: echo: %s", buf);
	}

	close(fd);
	return 0;
}
