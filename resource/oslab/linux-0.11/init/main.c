/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__     // 定义该变量是为了包括定义在 unistd.h 中的内嵌汇编代码等信息
#include <unistd.h>
#include <time.h>       // 时间类型头文件。其中最主要定义了tm 结构和一些有关时间的函数原形

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
/*
 * 我们需要下面这些内嵌语句- 从内核空间创建进程(forking)将导致没有写时复
 * 制（COPY ON WRITE）!!!直到一个执行 execve 调用。这对堆栈可能带来问题。处
 * 理的方法是在 fork() 调用之后不让 main() 使用任何堆栈。因此就不能有函数调
 * 用- 这意味着 fork 也要使用内嵌的代码，否则我们在从 fork() 退出时就要使用堆栈了。
 *
 * 实际上只有 pause 和 fork 需要使用内嵌方式，以保证从 main() 中不会弄乱堆栈，
 * 但是我们同时还定义了其它一些函数。
 */
static inline _syscall0(int,fork)               // int fork()：创建进程
static inline _syscall0(int,pause)              // int pause()：暂停进程的执行，直到收到一个信号
static inline _syscall1(int,setup,void *,BIOS)  // int setup(void * BIOS): 仅用于 linux 初始化（仅在这个程序中被调用）
static inline _syscall0(int,sync)               // int sync(): 更新文件系统

#include <linux/tty.h>      // tty 头文件，定义了有关 tty_io 串行通信方面的参数，常熟
#include <linux/sched.h>    // 调度程序头文件，定义了任务结构 task_struct，首个任务的初始数据，还有一些以宏的形式定义的有关描述符
                            // 参数设置和获取的嵌入式汇编函数程序。
#include <linux/head.h>     // head 头文件，定义了段描述符的简单结构和几个选择符常量
#include <asm/system.h>     // system 头文件，以宏的形式定义了许多有关设置或修改描述符/idt等的嵌入式汇编子程序
#include <asm/io.h>         // io 头文件，以宏的形式定义了对 io 端口操作的函数

#include <stddef.h>         // 标准定义头文件，以宏的形式定义了 NULL ，offsetof(TYPE, MEMBER)
#include <stdarg.h>         // 标准参数头文件，以宏的形式定义了变量参数列表。主要定义了一个类型(va_list)和三个宏(va_start, va_arg va_end)
#include <unistd.h>
#include <fcntl.h>          // 文件控制头文件，用于文件及其描述符的操作控制常数、符号的定义
#include <sys/types.h>      // 类型头文件，定义了基本的系统数据类型

#include <linux/fs.h>       // 文件系统头文件，定义了文件表结构（file, buffer_head, m_inode 等)

static char printbuf[1024]; // 输出缓冲区

extern int vsprintf();                              // 送格式化输出到一字符串中(kernel/vsprintf.c)
extern void init(void);                             // 函数原型，用于初始化，函数的实现就在本文件
extern void blk_dev_init(void);                     // 块设备初始化子程序（kernel/blk_drv/ll_rw_blk.c）
extern void chr_dev_init(void);                     // 字符设备初始化（kernel/chr_drv/tty_io.c）
extern void hd_init(void);                          // 硬盘初始化程序（kernel/blk_drv/hd.c）
extern void floppy_init(void);                      // 软驱初始化程序（kernel/blk_drv/floppy.c）
extern void mem_init(long start, long end);         // 内存管理初始化（mm/memory.c）
extern long rd_init(long mem_start, int length);    //虚拟盘初始化(kernel/blk_drv/ramdisk.c)
extern long kernel_mktime(struct tm * tm);          // 建立内核时间（秒）。
extern long startup_time;                           // 内核启动时间（开机时间）（秒）。

/*
 * This is set up by the setup-routine at boot-time
 */
/*
 * 以下这些数据是从 setup.s 程序在引导设置的。
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)      // 1M 以后的扩展内存大小（KB）
#define DRIVE_INFO (*(struct drive_info *)0x90080)  // 硬盘参数表基址
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)  // 根文件系统所在的设备号

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
/*
 * 是啊，是啊，下面这段程序很差劲，但我不知道如何正确地实现，而且好象
 * 它还能运行。如果有关于实时时钟更多的资料，那我很感兴趣。这些都是试
 * 探出来的，以及看了一些 bios 程序，呵！
 *
 * 这段宏读取CMOS 实时时钟信息。
 * 0x70 是写端口号，0x80|addr 是要读取的CMOS 内存地址。
 * 0x71 是读端口号
 */

/**
 * 这段宏可以用下面这个函数代替
 * _inline unsigned char CMOS_READ(unsigned char addr)
    {
        outb_p(addr,0x70);
        return inb_p(0x71);
    }
 */
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

// 将BCD 码转换成数字
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

/**
 * main 的参数为什么是 void ？
 * main 的标准是有三个参数：argc, argv, envp 。但此处并没有使用，此处的 main 只是写成传统的 main 形式和命名
 */
void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
 	ROOT_DEV = ORIG_ROOT_DEV;
 	drive_info = DRIVE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10);
	memory_end &= 0xfffff000;
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	// 初始化工作：内存，中断，设备，时钟，CPU等
	mem_init(main_memory_start,memory_end);     // 两个参数从 0x90002 的内存地址读取
	trap_init();
	blk_dev_init();
	chr_dev_init();
	tty_init();
	time_init();
	sched_init();
	buffer_init(buffer_memory_end);
	hd_init();
	floppy_init();
	sti();
	move_to_user_mode();    // 用户模式
	if (!fork()) {		/* we count on this going ok */
		init();
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid,i;

	setup((void *) &drive_info);
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}
