/*
 * server.c —— 第一周作业 test2：TCP echo 服务端
 *
 * 要点：
 *   1) 同一份源码，既能在 x86 虚拟机上用 gcc 编译，也能交叉编译成 ARM 版；
 *   2) 循环 accept 多个客户端、每个客户端循环收发，支持多轮通信；
 *   3) 端口可通过命令行第 1 个参数指定，默认 8888。
 *
 * 编译：
 *   gcc server.c -o server_x86
 *   arm-linux-gnueabihf-gcc server.c -o server_arm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define DEF_PORT  8888
#define BUF_SIZE  1024

static void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int listenfd, connfd, opt = 1;
	int port = (argc > 1) ? atoi(argv[1]) : DEF_PORT;
	struct sockaddr_in saddr, caddr;
	socklen_t clen;
	char buf[BUF_SIZE];

	/* 客户端异常断开时，避免被 SIGPIPE 直接打死 */
	signal(SIGPIPE, SIG_IGN);

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0)
		die("socket");

	/* 允许立即重用端口，否则重启 server 会报 Address already in use */
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family      = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);   /* 监听所有网卡 */
	saddr.sin_port        = htons(port);

	if (bind(listenfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
		die("bind");
	if (listen(listenfd, 5) < 0)
		die("listen");

	printf("server: listen on 0.0.0.0:%d\n", port);
	fflush(stdout);

	while (1) {                                  /* 循环接受多个客户端 */
		char ip[INET_ADDRSTRLEN];

		clen = sizeof(caddr);
		connfd = accept(listenfd, (struct sockaddr *)&caddr, &clen);
		if (connfd < 0) {
			if (errno == EINTR)
				continue;
			die("accept");
		}

		inet_ntop(AF_INET, &caddr.sin_addr, ip, sizeof(ip));
		printf("server: client %s:%d connected\n",
		       ip, ntohs(caddr.sin_port));
		fflush(stdout);

		while (1) {                              /* 一个客户端内反复收发 */
			ssize_t n = recv(connfd, buf, sizeof(buf) - 1, 0);

			if (n < 0) {
				if (errno == EINTR)
					continue;
				perror("recv");
				break;
			}
			if (n == 0) {
				printf("server: client disconnected\n");
				break;
			}

			buf[n] = '\0';
			printf("server: recv %zd bytes -> %s", n, buf);
			fflush(stdout);

			if (strncmp(buf, "quit", 4) == 0)
				break;

			if (send(connfd, buf, n, 0) < 0) {   /* 原样回显 */
				perror("send");
				break;
			}
		}

		close(connfd);
		printf("server: wait next client...\n");
		fflush(stdout);
	}

	close(listenfd);
	return 0;
}
