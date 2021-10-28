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
#define EXT_MEM_K (*(unsigned short *)0x90002)      // 1M 以后的扩展内存大小（单位 KB）
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
 * 0x70 是写端口号，0x80|addr 是要读取的 CMOS 内存地址。
 * 0x71 是读端口号
 */

/**
 * 这段宏可以考虑用下面这个函数代替
 * inline unsigned char CMOS_READ(unsigned char addr)
    {
        outb_p(addr, 0x70);
        return inb_p(0x71);
    }
 */
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

// 将BCD 码转换成数字
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

// 该子程序取CMOS 时钟，并设置开机时间 startup_time(为从1970-1-1-0 时起到开机时的秒数)
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

static long memory_end = 0;         // 机器具有的内存字节数
static long buffer_memory_end = 0;  // 告诉缓冲区末端地址
static long main_memory_start = 0;  // 主内存（将用于分页）开始的位置

struct drive_info { char dummy[32]; } drive_info;   // 用于存放硬盘参数表信息

/**
 * main 的参数为什么是 void ？
 * main 的标准是有三个参数：argc, argv, envp 。但此处并没有使用，此处的 main 只是写成传统的 main 形式和命名
 */
void main(void)		/* This really IS void, no error here. */   /* 这里确实是void，并没错。 */
{			/* The startup routine assumes (well, ...) this */  /* 在startup 程序(head.s)中就是这样假设的。 */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
/*
 * 此时中断仍被禁止着，做完必要的设置后就将其开启。
 */
    // 以下几个变量名的含义：
    // ROOT_DEV ：根设备号
    // buffer_memory_end ：高速缓存末端地址
    // memory_end : 机器内存数
    // main_memory_start : 主内存开始地址
 	ROOT_DEV = ORIG_ROOT_DEV;
 	drive_info = DRIVE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10); // 内存大小（单位字节） = 1MB 字节 + 扩展内存（k） * 1024 字节
	memory_end &= 0xfffff000;               // 忽略不到 4kB (1页) 的内存数
	if (memory_end > 16*1024*1024)          // 如果内存超过 16MB，则按 16MB 计
		memory_end = 16*1024*1024;
	if (memory_end > 12*1024*1024)          // 如果内存 > 12MB，则设置缓冲区末端 = 4MB
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)      // 否则如果 12MB > 内存 > 6MB，则设置缓冲区末端 = 2MB
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;    // 如果内存小于 6MB ,则设置缓冲区末端=1Mb
	main_memory_start = buffer_memory_end;  // 主内存起始位置 = 缓冲区末端
#ifdef RAMDISK      // 如果定义了虚拟盘，则主内存将减少
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
	// 初始化工作：内存，中断，设备，时钟，CPU等
	mem_init(main_memory_start,memory_end);     // 两个参数从 0x90002 的内存地址读取
	trap_init();        // 初始化硬件中断向量表（kernel/traps.c）
	blk_dev_init();     // 块设备初始化。（kernel/blk_dev/ll_rw_blk.c）
	chr_dev_init();     // 字符设备初始化。（kernel/chr_dev/tty_io.c）空，为以后扩展做准备
	tty_init();         // tty 初始化。（kernel/chr_dev/tty_io.c）
	time_init();        // 设置开机启动时间 -> startup_time
	sched_init();       // 调度程序初始化(加载了任务0 的tr, ldtr) （kernel/sched.c）
	buffer_init(buffer_memory_end); // 缓冲管理初始化，建内存链表等。（fs/buffer.c）
	hd_init();          // 硬盘初始化。（kernel/blk_dev/hd.c）
	floppy_init();      // 软驱初始化。（kernel/blk_dev/floppy.c）
	sti();              // 所有初始化工作都做完了，开启中断

    // 下面过程通过在堆栈中设置的参数，利用中断返回指令切换到任务0
	move_to_user_mode();    // 切换到用户模式（include/asm/system.h）
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
/*
 * 注意!! 对于任何其它的任务，'pause()'将意味着我们必须等待收到一个信号才会返
 * 回就绪运行态，但任务 0（task0）是唯一的意外情况（参见 'schedule()' ），因为任
 * 务 0 在任何空闲时间里都会被激活（当没有其它任务在运行时），
 * 因此对于任务0 'pause()' 仅意味着我们返回来查看是否有其它任务可以运行，如果没
 * 有的话我们就回到这里，一直循环执行 'pause()'。
 */
	for(;;) pause();
}


/**
 * 产生格式化信息并输出到标准输出设备stdout(1)，这里是指屏幕上显示。参数'*fmt'
 * 指定输出将采用的格式，参见各种标准C 语言书籍。该子程序正好是vsprintf 如何使
 * 用的一个例子。
 * 该程序使用vsprintf()将格式化的字符串放入printbuf 缓冲区，然后用write()
 * 将缓冲区的内容输出到标准设备（1--stdout）。
 * @param fmt
 * @param ...
 * @return
 */
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };      // 调用执行程序时参数的字符串数组
static char * envp_rc[] = { "HOME=/", NULL };       // 调用执行程序时的环境字符串数组

static char * argv[] = { "-/bin/sh",NULL };         // 调用执行程序时参数的字符串数组
static char * envp[] = { "HOME=/usr/root", NULL };  // 调用执行程序时的环境字符串数组

void init(void)
{
	int pid,i;

    // 读取硬盘参数包括分区表信息并建立虚拟盘和安装根文件系统设备
	setup((void *) &drive_info);        // setup() 是系统调用，对应函数是 sys_setup()，在kernel/blk_drv/hd.c
	(void) open("/dev/tty0",O_RDWR,0);  // 用读写方式访问设备 /dev/tty0, 这里对应了终端控制台，返回句柄号 0 -- stdin 标准输入设备
	(void) dup(0);      // 复制 0 号句柄，产生句柄 1 号 -- stdout 标准输入设备
	(void) dup(0);      // 复制 0 号句柄，产生句柄 2 号 -- stderr 标准出错输出设备
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);     // 打印缓冲区块数和总字节数，每块1024 字节
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);  // 空闲内存字节数

	/**
	 * 下面 fork() 用于创建一个子进程（子任务），对于被创建的子进程，fork() 将返回 0 值，
	 * 对于原（父进程）将返回子进程的进程号，所以对于 if (!fork()) {...} 代码块内是子进程执行的内容。
	 *
	 * 从代码看到，子进程关闭了句柄 0 （stdin），以只方式打开了 /etc/rc 文件，并执行 /bin/sh 程序，
	 * 所代参数和环境变量分别由 argv_rc 和 envp_rc 数据给出
	 */
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))     // 若文件打开失败，则退出 (/lib/_exit.c)
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);  // 装入/bin/sh 程序并执行。(/lib/execve.c)
		_exit(2);                           // 若 execve() 执行失败则退出(出错码2,“文件或目录不存在”)
	}

	/**
	 * 下面是父进程执行的语句。wait() 是等待子进程停止或种植，其返回值是子进程的进程号（pid）。
	 * 这三句的作用是父进程等在子进程的结束，&i 是存放返回状态信息的位置。如果 wait() 返回值不等于进程号，则继续的等待。
	 */
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;

	/**
	 * 如果执行到这里，说明刚刚创建的子进程执行已停止或终止了。下面循环中首先再创建一个子进程，
	 * 如果出错，则显示“初始化程序创建子进程失败”的信息并继续执行。对于所创建的子进程关闭所有以前遗留的句柄
	 * （stdin, stdout, stderr），新创建一个会话并设置进程组号，然后重新打开 /dev/tty0 作为 stdin ，
	 * 并复制出 stdout, stderr。再次执行系统解释程序 /bin/sh 。但这次执行所选用的参数和环境数据是另一套了，
	 * 然后父进程再次运行 wait() 等待。如果子进程又停止了执行，则再标准输出上显示出错信息“子进程 <pid> 停止运行，返回码是 <i>”
	 * 然后继续充实下去，形成大循环。
	 */
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
