/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
/**
 * 'sched.c' 是主要的内核文件。其中包括了有关调度的基本函数（sleep_on、wakeup、schedule 等）
 * 以及一些简单的系统调用函数（比如 getpid()，仅从当前任务中获取一个字段）。
 */

// 下面是调度程序头文件。定义了任务结构 task_struct 、第 1 各初始任务的数据。还有一些以宏
// 的形式定义的有关描述符参数设置和获取的嵌入式汇编函数程序。
#include <linux/sched.h>
#include <linux/kernel.h>       // 内核头文件。含有一些内核常用函数的原型定义
#include <linux/sys.h>          // 系统调用头文件
#include <linux/fdreg.h>        // 软驱头文件，含有软盘控制器参数的一些定义
#include <asm/system.h>         // 系统头文件。定义了设置或修改描述符/中断门等的嵌入式汇编宏
#include <asm/io.h>             // io 头文件。定义硬件端口输入/输出宏汇编语句
#include <asm/segment.h>        // 段操作头文件。定义了有关段寄存器操作的嵌入式汇编函数

#include <signal.h>             // 信号头文件。定义信号符号常量，sigaction 结构，操作函数原型

// 该宏取信号 nr 在信号位图中对应位的二进制数值。信号编号 1 - 32。
// 比如信号 5 的位图数值等于 1 << (5 - 1) = 16 = 0b00010000
#define _S(nr) (1<<((nr)-1))

// 除了 SIGKILL 和 SIGSTOP 信号以外的其他信号都是可阻塞的(0b1011-1111-1110-1111-1111)
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

/**
 * 内核调试函数。用于显示任务号 nr 的进程号、进程状态和内核堆栈空闲字节数（大约）。
 * @param nr - 任务号（任务号不是进程号）
 * @param p - 任务结构指针
 */
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])  // 检测指定任务数据结构以后等于 0 的字节数
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

/**
 * 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数（大约）。
 * NR_TASKS 是一个宏定义，表示系统能容纳的最大进程（任务）数量（64），定义在 include/kernel/sched.h
 */
void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

// PC 机 8253 定时芯片的输入时钟频率为 1.193180 MHz。Linux 内核希望定时器发出的中断频率是
// 100 Hz，即每 10ms 发出一次时钟中断。因此这里的 LATCH 是设置 8253 input的初值
#define LATCH (1193180/HZ)

extern void mem_use(void);          // ?? 没有任何地方定义和引用该函数

extern int timer_interrupt(void);   // 时钟中断处理程序 kernel/system_call.s
extern int system_call(void);       // 系统调用中断处理程序 kernel/system_call.s

// 每个任务（进程）在内核态运行时都有自己的内核态堆栈。这里定义了任务的内核态堆栈结构
union task_union {                  // 定义任务联合（任务结构成员和 stack 字符数组成员）。
	struct task_struct task;        // 因为一个任务的数据结构与其内核态堆栈在同一内存页
	char stack[PAGE_SIZE];          // 中，所以从给对战寄存器 ss 可以获得其数据段选择符。
};

static union task_union init_task = {INIT_TASK,};   // 定义初始任务的数据 sched.h

/*
 * 从开机开始算起的滴答数时间值全局变量（10 ms/滴答）。系统时钟中断每发生一次即一个滴答。
 * 前面的限定符 volatile ，英文解释是易改变的、不稳定的意思。这个限定词的含义是向编译器
 * 指明变量的内容可能会由于被其他程序修改而改变。通常在程序中声明一个变量时，编译器会尽量
 * 把它放在通用寄存器中，例如 ebx，以提高访问效率。当 CPU 把其值放到 ebx 中后一般就不会
 * 关系该变量对应内存位置中的内容了。若此时其他程序（例如内核程序或一个中断过程）修改了内
 * 存中该变量的值，ebx 中的值并不会随之更新。为了解决这种情况就创建了 volatile 限定符，
 * 让代码在引用该变量时一定要从指定内存位置中取其值。这里即是要求 gcc 不要对 jiffies 进行
 * 优化处理，也不要挪动位置，并且需要从内存中取其值。因为时钟中断处理过程等程序会修改它。
 */
long volatile jiffies=0;
long startup_time=0;                                // 开机时间。从 1970 年开始计时的秒数
struct task_struct *current = &(init_task.task);    // 当前任务指针（初始化时指向任务 0
struct task_struct *last_task_used_math = NULL;     // 使用过协处理器任务的指针

struct task_struct * task[NR_TASKS] = {&(init_task.task), };    // 定义任务指针数组

/*
 * 定义用户堆栈，共 1k 项，容量 4k 字节。在内核初始化操作过程中被用作内核栈，初始化完成
 * 以后将被用作任务 0 的用户态堆栈。在运行任务 0 之前它时内核栈，以后用作任务 0 和 1 的
 * 用户态栈。下面结构用于设置堆栈 ss:esp（数据段选择符：指针），见 head.s。 ss 被设置为
 * 内核数据段选择符（0x10），指针 esp 指在 user_stack 数组最后一项后面。这是因为 Intel
 * CPU 执行堆栈操作时是先递减堆栈指针 sp 值，然后在 sp 指针处保存入栈内容
 */
long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
/*
 * 将当前协处理器内容保存到老协处理器状态数组中，并将当前任务的协处理器内容加载
 * 进协处理器。
 */
// 当任务被调度交换过以后，该函数用以保存原任务的协处理器状态（上下文）并恢复新调度
// 进来的当前任务的协处理器执行状态
void math_state_restore()
{
    // 如果任务没变则返回（上一个任务就是当前任务）。这里的“上一个任务”是指刚被交换出去的任务
	if (last_task_used_math == current)
		return;
	// 在发送协处理器命令之前要宪法 WAIT 指令
	__asm__("fwait");
    // 如果上个任务使用了协处理器，则保存其状态
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}

	// 现在 last_task_used_math 指向当前任务，以备当前任务被交换出去时使用。此时如果当前
	// 任务用过协处理器，则恢复其状态。否则的话说明时第一次使用，于是就向协处理器发初始化命
	// 令，并设置使用了协处理器标志。
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);    // 向协处理器发初始化命令
		current->used_math=1;   // 设置使用已协处理器标志
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
/**
 * 'schedule()' 是调度函数。这是个很好的代码！没有任何理由对它进行修改，因为
 * 它可以在所有环境下工作（比如能够对 IO 边界处理很好的响应等）。只有一件事值得
 * 留意，那就是这里的信号处理代码。
 *
 * 注意！！任务 0 是闲置(idle)任务，只有当没有其他任务可以运行时才调用调用它。它
 * 不能被杀死，也不能睡眠。任务 0 中状态信息 'state' 是从来不用的。
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;        // 任务结构指针的指针

/* check alarm, wake up any interruptible tasks that have got a signal */
/* 检测 alarm（进程的报警定时值），唤醒任何已得到信号的可中断任务 */

    // 从任务数组中最后一个任务开始循环检测 alarm。在循环时跳过空指针项
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
		    // 如果设置过任务的定时值 alarm，并且已经过期 (alarm < jiffies)，则在
		    // 信号位图中置 SIGALARM 信号，即向任务发送 SIGALARM 信号。然后清 alarm。
		    // 该信号的默认操作是终止进程。jiffies 是系统从开机开始算起的滴答数（10ms/滴答）。
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			// 如果信号位图中除被阻塞的信号外还有其他信号，并且任务处于可中断状态，则置
			// 任务为就绪状态。其他 `~(_BLOCKABLE & (*p)->blocked)` 用于忽略被阻塞的信号，
			// 但 SIGKILL 和 SIGSTOP 不能被阻塞。
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */
/* 这里是调度程序的主要部分 */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		// 这段代码也是从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。比较每个
		// 就绪状态任务的 counter (任务运行时间的递减滴答计数)值，哪一个值大，运行时间还不长，
		// next 就指向哪个的任务号。
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		// 如果比较得出有 counter 值不等于 0 的结果，或者系统中没有任务一个可运行的任务存在（此时
		// c = -1，next = 0）则退出循环，执行任务切换操作。否则就根据每个任务的优先值，更新每一个
		// 任务的 counter 值，然后重新比较比较。counter 值的计算方式为 counter = counter / 2 + priority
		// 注意，这里计算过程不开率进程的状态。
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);    // 切换到任务号为 next 的任务，并运行之
}

/**
 * pause() 系统调用。转换当前任务的状态可中断的等待状态，并重新调度。
 * 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程
 * 调用过一个信号捕获函数。只有当捕获了一个信号，并且信号捕获处理函数返回，pause()
 * 才会返回。此时 pause() 返回值应该是 -1，并且 erron 被置为 EINTR。这里没有完全实现（直到0.95版）
 */
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

/**
 * 把当前任务置为不可中断的等待状态，并让睡眠队列头指针指向当前任务。
 * 只有明确地唤醒才会返回。该函数提供了进程与中断处理程序之间的同步
 * 机制。函数参数 p 是等待任务队列头指针。指针是含有一个变量地址的变量。
 * 这里参数 p 使用了指针的指针形式 **p ，这是因为 C 函数参数只能传值，
 * 没有直接的方式让被调用的函数改变调用该函数程序中变量的值。但是指针 *p
 * 指向的目标（这里是任务结构）会改变，因此为了能修改调用该函数中原来就是指针
 * 变量的值，就需要传递指针 *p 的指针即 **p。
 * @param p - 等待任务队列头指针。
 */
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	// 若指针无效，则退出。（指针所指的对象可以是 NULL，但指针本身不应该是 NULL）。
	// 另外，如果当前任务是任务 0，则死机。因为任务 0 的运行不依赖自己的状态，所以内核
	// 代码把任务 0 置为睡眠状态毫无意义。
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");

	// 让 tmp 指向已经在对待队列上的任务（如果有的话），例如 inode->i_wait。并且将睡眠
	// 队列头的等待指针指向当前任务。这样就把当前任务插入到 *p 的等待队列中。然后将当前
	// 任务置为不可中断的等待状态，并执行重新调度。
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();

	// 只有当这个等待任务被唤醒时，调度程序才会返回到这里，表示本进程已被明确地唤醒（就绪态）。
	// 既然大家都在等待同样的资源，那么在资源可用时，就有必要唤醒所有等待该资源的进程。该
	// 函数嵌套调用，也会嵌套唤醒所有等待该资源的进程。这里嵌套调用时指当一个进程调用了 sleep_on()
	// 后就会在该函数中被切换掉，控制权被转移到其他进程中。此时若有进程也需要使用同一资源，那么
	// 也会使用同一个等待队列头指针作为参数 wake_up 了该队列，那么当系统切换去执行头指针所指的
	// 进程 A 时，该进程才会继续执行下面的代码，把碎裂后的一个进程 B 置为就绪状态（唤醒）。而
	// 当轮到 B 进程执行时，它才能继续执行下面的代码。若它前面还有等待的进程 C ，那么它也会把 C
	// 唤醒等。（下面的代码其实还应该添加一条 *p = =temp;）
	// *p = temp;  // 这行是注释者添加的
	if (tmp)
		tmp->state=0;
}

/*
 * 将当前任务置为可中断的等待状态，并放入 *p 指定的等待队列中。
 */
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	// 若指针无效，则退出。（指针所指的对象可以是 NULL，但指针本身不会是 0）。
	// 如果当前任务是任务 0，则死机
	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");

	// 让 tmp 指向已经在对待队列上的任务（如果有的话），例如 inode->i_wait。并且将睡眠
	// 队列头的等待指针指向当前任务。这样就把当前任务插入到 *p 的等待队列中。然后将当前
	// 任务置为不可中断的等待状态，并执行重新调度。
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();

	// 只有当这个等待任务被唤醒时，程序才会又返回到这里，表示进程已被明确地唤醒并执行。
	// 如果等待队列中还有等待任务，并且队列头指针所指向的任务不是当前任务时，则将该等待
	// 任务置为可运行的就绪状态，并重新执行调度程序。当执行 *p 所指向的不是当前任务时，
	// 表示在当前任务被放入队列后，又有新的任务被插入等待队列前部。因为我们先唤醒他们，
	// 而让自己仍然等待。等待这些后续进入队列的任务被唤醒执行时唤醒本任务。于是去执行
	// 重新调度。
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;    // 这句有误，应该是 *P = tmp; 让队列头指针指向其余等待任务，否则
	            // 在当前任务之前插入等待队列的任务均被抹掉了。
	if (tmp)
		tmp->state=0;
}

/**
 * 唤醒 *p 指向的任务。*p 是任务等待队列头指针。由于新等待任务是插入在等待队列头指针处的，
 * 因此唤醒的是最后进入等待队列的任务。
 * @param p
 */
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;  // 置为就绪（可运行）状态 TASK_RUNNING
		*p=NULL;        // 这行应当删掉
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
/*
 * 好了，从这里开始是一些有关软盘的子程序，本不应该放在内核的主要部分中的。
 * 将它们放在这里是因为软驱需要定时处理，而放在这里是最方便的。
 */
// 下面的代码用于吹了软驱定时，需要结合驱动程序 floppy.c 来阅读。
// 下面数组存放等待软驱马达启动到正常转速的进程指针。数组索引 0-3 分辨对应软驱 A-D。
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
// 下面数组分别存放各软驱马达启动所需要的滴答数。程序中默认启动时间为 50 个滴答数（0.5秒）。
static int  mon_timer[4]={0,0,0,0};
// 下面数组分别存放在马达停转之前需要维持的时间。程序中设定为 10000 个滴答（100秒）。
static int moff_timer[4]={0,0,0,0};
// 对应软驱控制器中当前数字输出机器村。该寄存器每位的定义如下：
// 位 7-4：分别控制驱动器 D-A 马达的启动。1 - 启动；0 - 关闭。
// 位 3：1 - 允许 DMA 和中断请求；0 - 进制 DMA 和中断请求。
// 位 2：1 - 启动软盘控制器；0 - 复位软盘控制器。
// 位 1-0：00 - 11 用于选择控制器的软驱 A-D。
unsigned char current_DOR = 0x0C;       // 这里设置初始值为：允许 DMA 和中断请求、启动 FDC。

/**
 * 指定软驱启动到正常运转状态所需等待时间。
 * 局部变量 selected 是选中软驱标志（blk_drv/floppy.c)。mask 是所选软驱对应的数字输出寄存器中
 * 启动马达比特位。mask 高 4 位是各软驱马达标志。
 * @param nr - 软驱号（0-3）
 * @return 滴答数
 */
int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	// 系统最多有 4 个软驱。首先预先设置好指定软驱 nr 停转之前需要经过的时间（100秒）。然后
	// 取当前 DOR 寄存器指到临时变量 mask 中，并把指定软驱的马达启动标志置位。
	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */  // 停转维持时间
	cli();				/* use floppy_off to turn it off */ // 关中断
	mask |= current_DOR;
	// 如果当前没有选择软驱，则首先复位其他软驱的选择位，然后置指定软驱选择位。
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	// 如果数字输出寄存器的当前值与要求的值不同，则向 FDC 数字输出端口输出新值（mask），并且
	// 如果要求启动的马达还没有启动，则置相应软驱的马达启动定时器值（HZ / 2 = 0.5 秒或 50 个滴答）。
	// 若已经启动，则再设置启动定时为 2 个滴答，能满足下面 do_floppy_timer() 中先递减后判断
	// 的要求。执行本次定时代码的要求即可。此后更新当前数字输出寄存器 current_DOR。
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();                  // 开中断
	return mon_timer[nr];   // 最后返回启动马达所需的时间值
}

/**
 * 等待指定的软驱马达启动所需的一段时间，然后返回。
 * 设置指定软驱的马达启动到正常转速所需的延时，然后睡眠等待。在定时中断过程中会一直
 * 递减判断这里设定的延时值。当延时到期，就会唤醒这里的等待进程。
 * @param nr
 */
void floppy_on(unsigned int nr)
{
	cli();      // 关中断
	// 如果马达启动定时还没到，就一直把当前进程置为不可中断睡眠状态并放入等待马达运行的队列中
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();      // 开中断
}

/**
 * 设置关闭相应软驱马达停转定时器（3秒）。
 * 若不使用该函数明确关闭指定的软驱马达，则在马达开启 100 秒之后也会被关闭。
 * @param nr
 */
void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

/**
 * 软盘定时处理子程序。更新马达启动定时值和关闭马达关闭停转计时值。该子程序会在时钟定时
 * 中过程中被调用，因此系统每经过一个滴答（10ms）就会被调用一次，随时更新马达开启或停转
 * 定时器的值。如果某一个马达停转定时到，则数字输出集群其马达启动位复位。
 */
void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))      // 如果不是 DOR 指定的马达则跳过
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])        // 如果马达启动定时到唤醒进程
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {    // 如果马达停转定时到则
			current_DOR &= ~mask;       // 复位相应马达启动位，并且
			outb(current_DOR,FD_DOR);   // 更新数字输出寄存器
		} else
			moff_timer[i]--;            // 否则马达停转计时递减
	}
}

// 下面是关于定时器的代码。最多可有 64 个定时器
#define TIME_REQUESTS 64

// 定时器链表结构和定时器数组。该定时器链表专用于供软驱关闭马达和启动马达定时操作。这种
// 类型定时器类似现代 Linux 系统中的动态定时器（Dynamic Timer），仅供内核使用。
static struct timer_list {
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

/**
 * 添加定时器。
 * 软盘驱动程序(floppy.c)利用该函数执行启动或关闭马达的延时操作。
 * @param jiffies - 以 10ms 计的定时值（滴答数）
 * @param fn - 定时时间到时执行的函数指针
 */
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	// 如果定时处理函数指针位空，退出。
	if (!fn)
		return;
	cli();
	// 如果定时值 <= 0，则立即调用处理函数。并且该定时器不加入链表。
	if (jiffies <= 0)
		(fn)();
	else {  // 否认从定时器数组中，找一个空闲项
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break;
        // 如果定时器数组已满，则系统崩溃。否则向定时器数组结构填入相应信息，
        // 并链入链表头。
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		// 链表项按定时值从小到大排序。在排序时减去排在前面需要的滴答数，这样在处理
		// 定时器时只要查看链表头的第一项的定时是否到期即可。（这段程序好像没考虑周全。
		// 如果新插入的定时器值小于原来的头一个定时器值时则根本不会进入循环中，但此时
		// 还是应该将紧随其后面的一个定时器值减去第一个定时器的值。即如果第 1 个定时值
		// <= 第 2 个，则第 2 个定时器减去第 1 个的值即可。否则进入下面循环中进行处理。）
		while (p->next && p->next->jiffies < p->jiffies) {
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

/**
 * 时钟中断的函数处理程序，在 system_call.s 中的 _timer_interupt 被调用。
 * @param cpl - 当前特权级 0 或 3，是时钟中断发生时正被执行的代码选择符中的
 * 特权级。cpl = 0 时表示中断时正在执行内核代码；cpl = 3 时表示中断发生时
 * 正在执行用户代码。对于一个进程由于执行时间片用完时，则进行任务切换。并执行
 * 一个计时更新工作。
 */
void do_timer(long cpl)
{
	extern int beepcount;           // 扬声器发声时间滴答数（chr_drv/console.c)
	extern void sysbeepstop(void);  // 关闭扬声器（kernel/chr_drv/console.c）

	// 如果发声计数次数到
	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	ltr(0);
	lldt(0);
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);     // 设置 int 0x80 的对应的处理函数
}
