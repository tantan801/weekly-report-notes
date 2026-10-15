/*
 * week1_demo.c —— 第一周作业 test1
 *
 * 需求：
 *   1) 写一个 ko，可以 insmod / rmmod；
 *   2) 演示内核 8 级 printk，受控制台日志级别过滤；
 *   3) 在 /proc 下生成驱动子节点：读取打印 ko 信息，写入把数据传给驱动并 printk。
 *
 * 来源说明：
 *   - 模块骨架（__init/__exit/module_init/module_exit）  见《字符设备驱动开发入门》1.2.1
 *   - 8 级 printk 等级顺序                                见《字符设备驱动开发入门》1.2.3-3）
 *   - MODULE_LICENSE / MODULE_AUTHOR                      见《字符设备驱动开发入门》1.2.4
 *   - /proc 子节点（proc_create/single_open/seq_file）笔记未涉及，
 *     沿用《第一周周报》2.2 节写法，函数头注释风格对齐工作空间笔记。
 *
 * 目标平台：i.MX6ULL，linux-imx-rel_imx_4.1.15_2.1.0_ga_alientek
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define WEEK1_DEMO_NAME   "week1_demo"   /* 节点名字 */
#define WEEK1_DEMO_NUM    1              /* 设备号个数（本作业用不到，保持笔记风格） */

static char *last_data;      /* 保存用户最后一次写入的数据 */
static int   data_len;       /* 最后一次写入的字节数 */
static int   open_count;     /* /proc/week1_demo 被打开的次数 */

/*
 * @description : 读取 /proc/week1_demo 时调用，打印 ko 信息
 * @param - m   : seq_file 结构体
 * @param - v   : 迭代器，本处不用
 * @return      : 0 成功
 */
static int week1_demo_show(struct seq_file *m, void *v)
{
	seq_printf(m, "== week1 demo ko info ==\n");
	seq_printf(m, "module name : %s\n", WEEK1_DEMO_NAME);
	seq_printf(m, "author      : tantan\n");
	seq_printf(m, "open_count  : %d\n", open_count);
	seq_printf(m, "last_data   : %s\n", last_data ? last_data : "(none)");
	return 0;
}

/*
 * @description : 打开 /proc/week1_demo 时调用
 *                计数打开次数，并挂上 single_open 的 show 回调
 * @param - inode : 传递给驱动的 inode
 * @param - file  : 设备文件
 * @return        : single_open 的返回值，0 成功
 */
static int week1_demo_open(struct inode *inode, struct file *file)
{
	open_count++;
	return single_open(file, week1_demo_show, NULL);
}

/*
 * @description : 向 /proc/week1_demo 写入数据时调用
 *                把用户数据拷进内核，再用 8 个等级的 printk 全部打一遍
 * @param - file  : 设备文件
 * @param - buf   : 用户空间数据缓冲区
 * @param - count : 要写入的数据长度
 * @param - ppos  : 文件偏移
 * @return        : 写入的字节数，负值表示失败
 */
static ssize_t week1_demo_write(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	char *tmp;

	if (count == 0)
		return 0;

	tmp = kzalloc(count + 1, GFP_KERNEL);     /* +1 留给结尾的 '\0' */
	if (!tmp)
		return -ENOMEM;

	/* copy_from_user 返回未拷贝成功的字节数，非 0 即为失败 */
	if (copy_from_user(tmp, buf, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	kfree(last_data);
	last_data = tmp;
	data_len  = count;

	/* ---- 演示 8 个等级，数字越小优先级越高 ---- */
	printk(KERN_EMERG   "week1: [EMERG]  %s", tmp);
	printk(KERN_ALERT   "week1: [ALERT]  %s", tmp);
	printk(KERN_CRIT    "week1: [CRIT]   %s", tmp);
	printk(KERN_ERR     "week1: [ERR]    %s", tmp);
	printk(KERN_WARNING "week1: [WARN]   %s", tmp);
	printk(KERN_NOTICE  "week1: [NOTICE] %s", tmp);
	printk(KERN_INFO    "week1: [INFO]   %s", tmp);
	printk(KERN_DEBUG   "week1: [DEBUG]  %s", tmp);

	return count;
}

/* 设备操作函数集合 */
static const struct file_operations week1_demo_fops = {
	.owner   = THIS_MODULE,
	.open    = week1_demo_open,
	.read    = seq_read,
	.write   = week1_demo_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

/*
 * @description : 驱动入口函数，insmod 时执行
 * @param       : 无
 * @return      : 0 成功；其他 失败
 */
static int __init week1_demo_init(void)
{
	if (!proc_create(WEEK1_DEMO_NAME, 0664, NULL, &week1_demo_fops)) {
		printk(KERN_ERR "%s: create /proc/%s failed\n",
		       WEEK1_DEMO_NAME, WEEK1_DEMO_NAME);
		return -ENOMEM;
	}

	printk(KERN_INFO "%s: insmod ok, /proc/%s created\n",
	       WEEK1_DEMO_NAME, WEEK1_DEMO_NAME);
	return 0;
}

/*
 * @description : 驱动出口函数，rmmod 时执行
 * @param       : 无
 * @return      : 无
 */
static void __exit week1_demo_exit(void)
{
	remove_proc_entry(WEEK1_DEMO_NAME, NULL);

	kfree(last_data);
	last_data = NULL;
	data_len  = 0;

	printk(KERN_INFO "%s: rmmod ok\n", WEEK1_DEMO_NAME);
}

module_init(week1_demo_init);
module_exit(week1_demo_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tantan");
MODULE_DESCRIPTION("Week1 test1: /proc node + 8-level printk demo");
