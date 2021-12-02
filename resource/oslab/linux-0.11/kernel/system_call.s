/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd- and flopppy-interrupts are also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *
 * Stack layout in 'ret_from_system_call':
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */
/**
 * system_call.s 文件包含系统调用(system-call) 底层处理子程序。由于有些代码比较雷系，所以
 * 同时也包括始终中断处理（timer-interrupt)句柄。硬盘和软盘的中断处理程序也在这里。
 *
 * 注意：这段代码处理信号（singal）识别，在每次时钟中断和系统调用之后都会进行识别。一般
 * 中断过程并不处理信号识别，因为会给系统造成混乱。
 *
 * 从系统调用返回 （`ret_from_system_call`）。
 */
# 上面 Linus 原注释中的一般中断过程时指出了系统调用中断（int 0x80)和时钟中断（int 0x20)
# 以外的其他中断。这些中断会在内核态或用户态随机发生，若在这些中断过程中也处理信号识别的话，
# 就由可能与系统调用中断和时钟中断过程中对信号的识别处理过程相冲突，违反了内核代码的非抢占
# 原则。因此系统既无必要在这些“其他”中断中处理信号，也不允许这样做。

SIG_CHLD	= 17            # 定义 SIG_CHLD 信号（子进程停止或结束）

EAX		= 0x00              # 堆栈中各个寄存器的偏移位置
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28          # 当特权级变化时栈会切换，用户占指针被保存在内核态栈中
OLDSS		= 0x2C

# 以下这些是任务结构（task_struct)中变量的偏移值，参见 include/linux/sched.h
state	= 0		# these are offsets into the task-struct.
counter	= 4     # 任务运行事件计数（递减）（滴答数），运行时间片
priority = 8    # 运行优先数。任务开始时 counter = priority，越大则运行时间越长
signal	= 12    # 是信号位图，每个比特位代表一种信号，信号值 = 位偏移值 + 1
sigaction = 16		# MUST be 16 (=len of sigaction)
                # sigaction 结构长度必须是 16 字节
                # 信号执行属性结构数组的偏移值，对应信号将要执行的操作和标志信息
blocked = (33*16)   # 受阻塞信号位图的偏移量

# 以下定义在 sigaction 中的偏移量，参见 include/signal.h
# offsets within sigaction
sa_handler = 0          # 信号处理过程的句柄（描述符）
sa_mask = 4             # 信号屏蔽码
sa_flags = 8            # 信号集
sa_restorer = 12        # 恢复函数指针，参见 kernel/signal.c

nr_system_calls = 72

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */
/*
 * 好了，在使用软驱时我收到了并行打印机中断，很奇怪。现在不管他
 */
 # 定义入口点
.globl system_call,sys_fork,timer_interrupt,sys_execve
.globl hd_interrupt,floppy_interrupt,parallel_interrupt
.globl device_not_available, coprocessor_error

# 错误的系统调用号
.align 2                # 内存 4 字节对齐
bad_sys_call:
	movl $-1,%eax       # eax 中置 -1，退出中断
	iret

# 重新执行调度程序入口。调度程序 schedule 在 kernel/sched.c 中
# 当调度程序 schedule() 返回时从 ret_from_sys_call 处继续执行。
.align 2
reschedule:
	pushl $ret_from_sys_call            # 将 ret_from_sys_call 的地址入栈
	jmp schedule

# int 0x80 -- linux 系统调用入口点
.align 2
system_call:
	cmpl $nr_system_calls-1,%eax        # eax 中存放的是系统调用号
	ja bad_sys_call                     # 调用号如果超出范围的话就在 eax 中置 -1 并退出
	push %ds                            # 保存原段寄存器值
	push %es
	push %fs
	# 一个系统调用最多可带三个参数，也可以不带参数。下面入栈的 ebx ecx 和 edx 中存放系统调用相应
	# C 语言函数调用的参数。这几个寄存器入栈顺序由 GNU GCC 规定的，ebx 中可存放第一个 参数，ecs
	# 存放第二个参数，edx 存放第三个参数。
	# 系统调用语句可参见头文件 include/unistd.h 中的系统调用宏
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters     # 调用的参数
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds         # ds es指向内核数据段（全局描述符表中数据段描述符）
	mov %dx,%es         # 内核数据
	# fs 指向局部数据段（局部描述符表中数据段描述符），即指向本次系统调用的用户程序的数据段。
	# 注意，在 linux 0.11 中内核给任务分配的代码和数据内存段时重叠的，它们的段基址和段限长
	# 相同。参见 fork.c 中的 copy_mem() 函数。
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs         # fs 可以找到用户数据
	# 下面这句操作数的含义是：调用地址 = [sys_call_table + %eax * 4]
	# sys_call_table[] 是一个指针数组，定义在 include/linux/sys.h 中，该指针数组中设置
	# 了所有 72 个系统调用 C 处理函数的地址
	call sys_call_table(,%eax,4)    # a(,%eax,4) = a + 4 * eax  # 间接调用指定功能的 C 函数
	pushl %eax          # 把系统调用的返回值入栈
	# 下面5行是查看当前任务的运行状态。如果不在就绪状态（state 不等于 0）就去指定调度程序。
	# 如果该任务就绪状态，但时间片已用完（counter = 0），则也去执行调度程序。
	# 例如当后台进程组中的进程执行控制终端读写操作时，那么默认条件下该后台进程组的所有进程都会
	# 收到 SIGTTIN 或 SIGTTOU 信号，导致进程组中的所有进程都处于停止状态。而当前经常则会立刻返回。
	movl current,%eax   # 取当前任务（进程）数据结构地址 -> eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule

# 下面这段代码执行从系统调用 C 函数返回后，对信号进行识别处理。其他中断服务程序退出时也将
# 跳转到这里进行处理后才退出中断过程，例如处理器出错中断 int 16.
ret_from_sys_call:
    # 首先判断当前任务是不是初始任务 task0，如果时则不必对其进行信号量方面的处理，直接返回
	movl current,%eax		# task[0] cannot have signals
	cmpl task,%eax
	je 3f                   # 向前跳转到标号 3 处退出中断处理
	# 通过对原调用程序代码选择符的检查来判断调用程序是否是用户任务。如果不是则直接退出中断。
	# 这是因为任务在内核态执行是不可抢占的。否则对任务进行信号量做识别处理。这里比较选择符是否
	# 为用户代码段的选择符 0x000f （RPL=3，局部表，第一个代码段）来判断是否为用户任务。
	# 如果不是则说明是某个中断服务程序跳转到标号 3，阈值跳转退出中断程序。如果原堆栈段选择符
	# 不为 0x17 （即原堆栈不在用户段中），也就说明本次系统调用的调用者不是用户任务，也退出。
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f
	# 下面这段代码（到标号3）用户处理当前任务中的信号。首先取当前任务结构中的信号位图（32位，每
	# 位代表 1 种信号），然后用任务结构中的信号阻塞（屏蔽）码，则色不允许的信号位，取得数值最小的
	# 信号值，再把原信号位图中该信号对应的位复位（置0），最后将该信号值作为参数之一调用 do_signal()。
	# do_signal() 在 kernel/signal.c 中，其参数包括 13 个入栈信息
	movl signal(%eax),%ebx  # 取信号位图 -> ebx,每 1 位代表 1 种信号，共 32 个信号
	movl blocked(%eax),%ecx # 取阻塞（屏蔽）信号位图 -> ecs
	notl %ecx               # 每位取反
	andl %ebx,%ecx          # 获得许可的信号位图
	bsfl %ecx,%ecx          # 从地位（位0）开始扫描位图，看是否有 1 的位
	je 3f                   # 若没有跳到标号3
	btrl %ecx,%ebx          # 复位该信号（ebx 含有原 signal 位图）
	movl %ebx,signal(%eax)  # 重新保存 signal 位图信息, current -> signal
	incl %ecx               # 将信号调整为 1 开始的数 （1-32）
	pushl %ecx              # 信号值入栈作为调用 do_signal 的参数之一
	call do_signal          # 调用 C 函数信号处理程序 (kernel/signal.c)
	popl %eax               # 弹出入栈的信号值
3:	popl %eax               # eax 种含有入栈的系统调用返回值
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

# int16 -- 处理器错误中断。类型：错误；错误码：无
# 这是一个外部的基于硬件的异常。
.align 2
coprocessor_error:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax         # ds, es 置为指向内核数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax         # fs 置为指向局部数据段（出错程序的数据段）
	mov %ax,%fs
	pushl $ret_from_sys_call # 把下面调用返回地址入栈
	jmp math_error          # 执行 C 函数 math_error() 参见 math/math_emulate.c

# int7 -- 设备不存在或协处理器不存在。类型：错误；错误码：无
# 如果控制寄存器 CRO 中 EM（模拟）标志置位，则当 CPU 执行一个协处理器指令时就会引发该
# 中断，这样 CPU 就可以有机会让这个中断处理程序模拟协处理器指令。
# CRO 的交换标志 TS 是在 CPU 执行任务转换时设置的。TS 可以用来确定什么时候协处理器中的
# 内容与 CPU 正在执行的任务不匹配了。当 CPU 在于宁一个协处理器转移指令时发现 TS 置位时，
# 就会引发该中断。此时就可以保存前一个任务的协处理器内容，并恢复新任务的协处理器执行状态。
# 参见 kernel/sched.c 。
.align 2
device_not_available:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax         # ds, es 置为指向内核数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax         # fs 置为指向局部数据段（出错程序的数据段）
	mov %ax,%fs
	# 清 CR0 中任务已交换标志 TS，并取 CR0 值。若其中协处理器仿真标志 EM 没有置位，说明不是 EM
	# 引起的中断，则恢复任务协处理器状态，执行 C 函数 math_state_restore()，并返回到
	# ret_from_sys_call 处执行。
	pushl $ret_from_sys_call    # ret_from_sys_call 入栈
	clts				# clear TS so that we can use math
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je math_state_restore
	# 若 EM 标志是置位的，则去执行数学仿真程序 math_emulate()
	pushl %ebp
	pushl %esi
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret                 # 这里的 ret 将跳转到 ret_from_sys_call

# int32 -- （int 0x20）时钟中断处理程序。中断频率被设置为 100 Hz (include/linux/sched.h 宏 HZ)
# 定时芯片 8253/8254 是在 (kernel/sched.c 函数 sched_init) 初始化的。因此这里 jiffies 每 10 毫秒 加 1.
# 这段代码将 jiffies 增 1，发送结束中断指令给 8259 控制器，然后用当前特权级作为参数调用 C 函数
# do_timer(long CPL)。当调用返回时转去检测并处理信号。
.align 2
timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax     # ds, es 置为指向内核数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax     # fs 置为指向局部数据段（程序的数据段）
	mov %ax,%fs
	incl jiffies
	# 由于初始化中断控制芯片时没有采用自动 EOI，所以这里需要发指令结束该硬件中断
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20      # 操作命令字 OCW2 送 0x20 端口
	# 下面从堆栈中去除执行系统调用代码的选择符（CS段寄存器值）中的当前特权级别（0或3）并压入堆栈，
	# 作为 do_timer 的参数。do_timer() 函数执行任务切换、计时等工作，参见 kernel/sched.c
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call

#### 这个是 sys_execve() 系统调用。取中断调用程序的代码指针作为参数调用 C 函数 do_execve()
# 参见 fs/exec.c
.align 2
sys_execve:
	lea EIP(%esp),%eax  # eax 指向堆栈中保存的用户程序 eip 指针处 (EIP + %esp)
	pushl %eax
	call do_execve
	addl $4,%esp        # 丢弃调用时压入栈的 EIP 值
	ret

#### sys_fork() 系统调用，用于创建子进程，时 system_call 功能2.原型在 include/linux/sys.h 中
# 首先调用 C 函数 find_empty_process() 取得一个进程号 pid。若返回负数说明任务数组已满。然后调用
# copy_process() 复制进程。
.align 2
sys_fork:
	call find_empty_process # 调用 find_empty_process() 参见 kernel/fork.c
	testl %eax,%eax     # 在 eax 中返回进程号 pid。若为负数则退出
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process   # 调用 C 函数 copy_process() 参见：kernel/fork.c
	addl $20,%esp       # 丢弃这里所有压栈内容
1:	ret

#### int46 -- （int 0x2e) 硬盘中断处理程序，响应硬件中断请求 IRQ14。
# 当请求的硬盘操作完成或出错就会发出此中断信号。（参见：kernel/blk_drv/hd.c）。
# 首先向 8259A 中断控制从芯片发送结束硬件中断指令(EOI)，然后取变量 do_hd 中的函数指针
# 放入 edx 寄存器中，并设置 do_hd 为 NULL，接着判断 edx 函数指针是否为空。如果为空，则
# 给 edx 赋值指向 unexpected_hd_interrupt() ，用于显示出错信息。随后 8259A 主芯片
# 送 EOI 指令，并调用 edx 中指针指向的函数：read_intr()、write_intr() 或 unexpected_hd_interrupt()。
hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax     # ds, es 置为指向内核数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax     # fs 置为指向局部数据段（程序的数据段）
	mov %ax,%fs
	# 由于初始化中断控制芯片时没有采用自动 EOI，所以之类需要发指令结束该硬件中断。
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1    # 送从 8259A。
	jmp 1f			# give port chance to breathe           # jmp 起延时作用
1:	jmp 1f
    # do_hd 定义了一个函数指针，将被赋予 read_intr() 或 write_intr() 函数地址。放到 edx 寄存器后
    # 就将 do_hd 指针变量置为 NULL。然后测试得到的函数指针，若该指针为空，则赋予该指针指向 C 函数
    # unexpected_hd_interrupt()，以处理未知硬盘中断。
1:	xorl %edx,%edx
	xchgl do_hd,%edx
	testl %edx,%edx     # 测试函数指针是否为 NULL
	jne 1f              # 若空，则使指针指向 C 函数 unexpected_hd_interrupt，参见：kernel/blk_drv/hd.c
	movl $unexpected_hd_interrupt,%edx
1:	outb %al,$0x20      # 送主 8259A 中断控制器 EOI 指令（结束硬件中断）。
	call *%edx		# "interesting" way of handling intr.
	pop %fs         # 上句调用 do_hd 指向的 C 函数
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

#### int38 -- （int 0x26）软盘驱动器中断处理程序，相应硬件中断请求 IRQ6。
# 其处理过程与上面对硬盘的处理基本一样。（kernel/blk_drv/floppy.c）。
# 首先向 8259A 中断控制器主芯片发送 EOI 指令，然后取变量 do_floppy 中的函数指针放入 eax
# 寄存器中，并置 do_floppy 为 NULL，接着判断 eax 函数指针是否为空。若为空，则给 eax 赋值指向
# unexpected_floppy_interrupt()，用于显示出错信息。随后调用 eax 指向的函数：rw_interrupt，
# 或 seek_interrupt 或 recal_interrupt 或 reset_interrupt 或 unexpected_floppy_interrupt。
floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax     # ds, es 置为指向内核数据段
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax     # fs 置为指向局部数据段（程序的数据段）
	mov %ax,%fs
	movb $0x20,%al      # 送主 8259A 中断控制器 EOI 指令（结束硬件中断）。
	outb %al,$0x20		# EOI to interrupt controller #1
	# do_floppy 为一函数指针，将被赋值实际处理 C 函数指针。该指针在被交换到 eax 寄存器后就将
	# do_floppy 变量置空。然后测试 eax 中原指针是否为空，若是则使指针指向 C 函数
	# unexpected_floppy_interrupt
	xorl %eax,%eax
	xchgl do_floppy,%eax
	testl %eax,%eax     # 测试函数指针是否为 NULL？
	jne 1f              # 若不为空，跳转标号 1；若为空，则使指针指向 unexpected_floppy_interrupt
	movl $unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.   # 间接调用
	pop %fs             # 上局调用 do_floppy 指向的函数
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

#### int39 -- (int 0x27) 并行口中断处理程序，对应硬件中断请求信号 IRQ7。
# 本版本内核还未实现。这里只是发送 EOI 指令。
parallel_interrupt:
	pushl %eax
	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	iret
