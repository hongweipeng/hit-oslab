/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);

long last_pid=0;

void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);
	data_limit=get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;

	p = (struct task_struct *) get_free_page(); // 申请内存空间用作 PCB，一页大小为 4K
	if (!p)
		return -EAGAIN;
	task[nr] = p;                               // 其中nr 为任务号，由前面find_empty_process()返回。
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	                /* 注意！这样做不会复制超级用户的堆栈 （只复制当前进程内容）。*/
    p->state = TASK_UNINTERRUPTIBLE;            // 将新进程的状态先置为不可中断等待状态
    p->pid = last_pid;                          // 新进程号。由前面调用find_empty_process()得到
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	                    /* 进程的领导权是不能继承的 */
    p->utime = p->stime = 0;                // 初始化用户态时间和核心态时间
    p->cutime = p->cstime = 0;              // 初始化子进程用户态和核心态时间
    p->start_time = jiffies;                // 当前滴答数时间
	p->tss.back_link = 0;
	// 创建内核栈
	p->tss.esp0 = PAGE_SIZE + (long) p;     /* 堆栈指针（由于是给任务结构p 分配了1 页
	                                           新内存，所以此时 esp0 正好指向该页顶端）*/
	p->tss.ss0 = 0x10;                      // 内核数据段
	p->tss.eip = eip;                       // 指令代码指针
	                                        /*
	                                         * p->tss.eip = eip;
	                                         * p->tss.cs = cs & 0xffff;
	                                         * 这两句是将执行地址 cs:eip 放在 TSS 中
	                                         */
	p->tss.eflags = eflags;
	p->tss.eax = 0;                         // 执行时的寄存器也放进去了
	                                        /*
	                                         * eax 的值即为 fork() 系统调用的返回值
	                                         * mov %eax, __NR_fork
	                                         * int 0x80
	                                         * move res, %eax
	                                         */
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;                       // esp 是用户栈
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;                // 段寄存器仅 16 位有效
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);                  // 该新任务 nr 的局部描述符表选择符（LDT 的描述符在 GDT 中）
	p->tss.trace_bitmap = 0x80000000;
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	if (copy_mem(nr,p)) {                   // 返回不为0 表示出错
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	for (i=0; i<NR_OPEN;i++)                // 如果父进程中有文件是打开的，则将对应文件的打开次数增 1
		if ((f=p->filp[i]))
			f->f_count++;
    if (current->pwd)                       // 将当前进程（父进程）的 pwd, root 和 executable 引用次数均增1
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;

	// 在GDT 中设置新任务的 TSS 和 LDT 描述符项，数据从task 结构中取
	// 在任务切换时，任务寄存器tr 由CPU 自动加载
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */
	/* 最后再将新任务设置成可运行状态，以防万一 */
	return last_pid;                        // 返回新进程号（与任务号是不同的）
}

/**
 * 为新进程取得不重复的进程号last_pid，并返回在任务数组中的任务号(数组index)
 * @return int
 */
int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)             // 任务号 0 排除在外
		if (!task[i])
			return i;
	return -EAGAIN;
}
