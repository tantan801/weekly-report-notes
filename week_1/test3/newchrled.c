/*
 * newchrled.c —— 第一周作业 test3：把 LED 驱动的控制通道从 write 改成 ioctl
 *
 * 底稿：《新字符设备驱动实验》42.5.1 的新字符设备框架（alloc_chrdev_region /
 *       cdev_init / cdev_add / class_create / device_create，mdev 自动建节点），
 *       ioremap 与 led_switch 与《LED驱动开发实验》41.3 一致。
 *       ioctl 函数签名与 fops 挂法参照《Linux内核定时器实验》50.3 的
 *       static long xxx_unlocked_ioctl(struct file *, unsigned int, unsigned long)。
 *
 * 相对笔记原版的改动（其余全部原样保留）：
 *   1) 新增 #include <linux/ioctl.h>
 *   2) LED_ON / LED_OFF 改用 _IOW 命令码；新增 LED_GET（_IOR）用于读状态
 *   3) 删除 led_write，新增 led_ioctl
 *   4) fops：去掉 .write，挂上 .unlocked_ioctl
 *   5) 新增 led_state 记录当前状态，供 LED_GET 使用
 *   6) printk 补上 KERN_INFO 级别，并修正底稿里的拼写错误 newcheled -> newchrled
 *   7) fops 加 const
 *
 * 硬件：GPIO1_IO03，低电平点亮 LED，高电平熄灭。
 * 目标平台：i.MX6ULL，linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define NEWCHRLED_CNT   1               /* 设备号个数 */
#define NEWCHRLED_NAME  "newchrled"     /* 设备名字 */

#define LEDOFF          0               /* 关灯 */
#define LEDON           1               /* 开灯 */

/* ---------------- ioctl 命令码定义（本作业核心改动） ---------------- */
#define LED_IOC_MAGIC   'l'
#define LED_ON    _IOW(LED_IOC_MAGIC, 1, int)   /* 开灯 */
#define LED_OFF   _IOW(LED_IOC_MAGIC, 2, int)   /* 关灯 */
#define LED_GET   _IOR(LED_IOC_MAGIC, 3, int)   /* 读当前状态 */

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE              (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0X020E02F4)
#define GPIO1_DR_BASE               (0X0209C000)
#define GPIO1_GDIR_BASE             (0X0209C004)

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/* newchrled 设备结构体：封装本设备全部资源 */
struct newchrled_dev {
	dev_t devid;            /* 设备号    */
	struct cdev cdev;       /* cdev      */
	struct class *class;    /* 类        */
	struct device *device;  /* 设备      */
	int major;              /* 主设备号  */
	int minor;              /* 次设备号  */
};

struct newchrled_dev newchrled;     /* led 设备全局实例 */

static int led_state = LEDOFF;      /* 记录当前 LED 状态，供 LED_GET 使用 */

/*
 * @description : LED 打开/关闭
 * @param - sta  : LEDON(1) 打开 LED，LEDOFF(0) 关闭 LED
 * @return       : 无
 */
void led_switch(u8 sta)
{
	u32 val = 0;

	if (sta == LEDON) {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);           /* bit3 清零，输出低电平点亮 LED */
		writel(val, GPIO1_DR);
	} else if (sta == LEDOFF) {
		val = readl(GPIO1_DR);
		val |= (1 << 3);            /* bit3 置 1，输出高电平熄灭 LED */
		writel(val, GPIO1_DR);
	}
}

/*
 * @description : 打开设备
 * @param - inode: 传递给驱动的 inode
 * @param - filp : 设备文件，设置 private_data 指向设备结构体
 * @return       : 0 成功；其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &newchrled;    /* 设置私有数据 */
	return 0;
}

/*
 * @description : 从设备读取数据，本实验空实现
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description : ioctl 回调，应用 ioctl 触发此函数
 * @param - filp : 要打开的设备文件
 * @param - cmd  : 应用程序发送过来的命令
 * @param - arg  : 参数
 * @return       : LED_GET 时返回当前状态；其他命令 0 成功，负值 失败
 */
static long led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case LED_ON:
		led_switch(LEDON);
		led_state = LEDON;
		printk(KERN_INFO "newchrled: ioctl LED_ON\n");
		break;

	case LED_OFF:
		led_switch(LEDOFF);
		led_state = LEDOFF;
		printk(KERN_INFO "newchrled: ioctl LED_OFF\n");
		break;

	case LED_GET:
		return led_state;

	default:
		printk(KERN_WARNING "newchrled: unknown cmd 0x%x\n", cmd);
		return -EINVAL;
	}
	return 0;
}

/*
 * @description : 关闭/释放设备
 */
static int led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 设备操作函数集合 */
static const struct file_operations newchrled_fops = {
	.owner          = THIS_MODULE,
	.open           = led_open,
	.read           = led_read,
	.release        = led_release,
	.unlocked_ioctl = led_ioctl,        /* 控制通道：ioctl（原为 .write） */
};

/*
 * @description : 驱动入口函数，insmod 执行
 */
static int __init led_init(void)
{
	u32 val = 0;

	/* 1、寄存器地址映射 */
	IMX6U_CCM_CCGR1   = ioremap(CCM_CCGR1_BASE, 4);
	SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
	SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
	GPIO1_DR          = ioremap(GPIO1_DR_BASE, 4);
	GPIO1_GDIR        = ioremap(GPIO1_GDIR_BASE, 4);

	/* 2、使能 GPIO1 时钟 */
	val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);                  /* 清除以前的设置 */
	val |= (3 << 26);                   /* 设置新值 */
	writel(val, IMX6U_CCM_CCGR1);

	/* 3、设置 GPIO1_IO03 的复用功能，设置 IO 电气属性 */
	writel(5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	/* 4、设置 GPIO1_IO03 为输出功能 */
	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);
	val |= (1 << 3);
	writel(val, GPIO1_GDIR);

	/* 5、默认关闭 LED */
	val = readl(GPIO1_DR);
	val |= (1 << 3);
	writel(val, GPIO1_DR);

	/* -------- 新字符设备核心流程 ---------- */
	/* 1、申请设备号，优先动态分配 */
	if (newchrled.major) {              /* 定义了主设备号，静态分配 */
		newchrled.devid = MKDEV(newchrled.major, 0);
		register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
	} else {                            /* major=0，动态申请 */
		alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
		newchrled.major = MAJOR(newchrled.devid);
		newchrled.minor = MINOR(newchrled.devid);
	}
	printk(KERN_INFO "newchrled: major=%d, minor=%d\n",
	       newchrled.major, newchrled.minor);

	/* 2、初始化 cdev */
	newchrled.cdev.owner = THIS_MODULE;
	cdev_init(&newchrled.cdev, &newchrled_fops);

	/* 3、添加 cdev 到内核 */
	cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);

	/* 4、创建类 */
	newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.class)) {
		return PTR_ERR(newchrled.class);
	}

	/* 5、创建设备，触发 mdev 自动生成 /dev/newchrled */
	newchrled.device = device_create(newchrled.class, NULL, newchrled.devid,
	                                 NULL, NEWCHRLED_NAME);
	if (IS_ERR(newchrled.device)) {
		return PTR_ERR(newchrled.device);
	}

	return 0;
}

/*
 * @description : 驱动出口函数，rmmod 执行
 */
static void __exit led_exit(void)
{
	/* 取消 ioremap 地址映射 */
	iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

	/* 注销字符设备，注意执行顺序 */
	cdev_del(&newchrled.cdev);                                      /* 删除 cdev */
	unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);       /* 释放设备号 */

	device_destroy(newchrled.class, newchrled.devid);               /* 删除设备 */
	class_destroy(newchrled.class);                                 /* 删除类 */
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tantan");
MODULE_DESCRIPTION("Week1 test3: LED driver controlled by ioctl");
