// SPDX-License-Identifier: GPL-2.0
/*
 * 读回 Linux 6.6 探针内核留下的启动进度标记。
 *
 * 6.6 的 init/chopin66_mark.c 会把 "CHOPIN66MARK:" + 阶段字符写进 preloader
 * 预留的 ramoops console zone（物理 0x48090000）。6.6 自己起不到 pstore，
 * 所以在 Android 13 的这颗 4.14 内核里直接 ioremap 这块物理内存读出来：
 *   - 启动时打到 dmesg（tag chopin66marks）
 *   - 并常驻 /proc/chopin66_marks
 *
 * 只是只读探针，不写这块内存，任何情况下都不影响正常启动。
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/init.h>

#define CHOPIN66_MARK_PHYS	0x48090000UL
#define CHOPIN66_MARK_SIZE	256
#define CHOPIN66_MARK_MAGIC	"CHOPIN66MARK:"

static char chopin66_marks[CHOPIN66_MARK_SIZE + 1];

static void chopin66_marks_load(void)
{
	void __iomem *base;
	int i;

	base = ioremap(CHOPIN66_MARK_PHYS, CHOPIN66_MARK_SIZE);
	if (!base) {
		pr_err("chopin66marks: ioremap failed\n");
		return;
	}
	for (i = 0; i < CHOPIN66_MARK_SIZE; i++)
		chopin66_marks[i] = readb(base + i);
	chopin66_marks[CHOPIN66_MARK_SIZE] = '\0';
	iounmap(base);

	for (i = 0; i < CHOPIN66_MARK_SIZE; i++) {
		if (chopin66_marks[i] == '\0')
			break;
		if (chopin66_marks[i] < 0x20 || chopin66_marks[i] > 0x7e)
			chopin66_marks[i] = '?';
	}
	chopin66_marks[i] = '\0';
}

static int chopin66_marks_show(struct seq_file *m, void *v)
{
	if (!strncmp(chopin66_marks, CHOPIN66_MARK_MAGIC,
		     sizeof(CHOPIN66_MARK_MAGIC) - 1))
		seq_printf(m, "%s\n", chopin66_marks);
	else
		seq_printf(m, "no marker (phys 0x%lx): %.64s\n",
			   CHOPIN66_MARK_PHYS, chopin66_marks);
	return 0;
}

static int chopin66_marks_open(struct inode *inode, struct file *file)
{
	return single_open(file, chopin66_marks_show, NULL);
}

static const struct file_operations chopin66_marks_fops = {
	.owner		= THIS_MODULE,
	.open		= chopin66_marks_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init chopin66_marks_init(void)
{
	chopin66_marks_load();
	if (!strncmp(chopin66_marks, CHOPIN66_MARK_MAGIC,
		     sizeof(CHOPIN66_MARK_MAGIC) - 1))
		pr_info("chopin66marks: %s\n", chopin66_marks);
	else
		pr_info("chopin66marks: no marker found, head=%.32s\n",
			chopin66_marks);
	proc_create("chopin66_marks", 0444, NULL, &chopin66_marks_fops);
	return 0;
}
late_initcall(chopin66_marks_init);
