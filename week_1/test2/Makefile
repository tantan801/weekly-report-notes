CC_X86 := gcc
CC_ARM := arm-linux-gnueabihf-gcc
CFLAGS := -Wall -O2

all: server_x86 client_x86 server_arm client_arm

x86: server_x86 client_x86
arm: server_arm client_arm

%_x86: %.c
	$(CC_X86) $(CFLAGS) $< -o $@

%_arm: %.c
	$(CC_ARM) $(CFLAGS) $< -o $@

# 板子 rootfs 的 glibc 比工具链老时报 "GLIBC_2.xx not found"，用这个编静态版
static_arm: server.c client.c
	$(CC_ARM) $(CFLAGS) -static server.c -o server_arm_static
	$(CC_ARM) $(CFLAGS) -static client.c -o client_arm_static

clean:
	rm -f server_x86 client_x86 server_arm client_arm server_arm_static client_arm_static

.PHONY: all x86 arm static_arm clean
