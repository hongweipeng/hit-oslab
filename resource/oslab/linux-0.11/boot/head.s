/*
 *  linux/boot/head.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  head.s contains the 32-bit startup code.
 *
 * NOTE!!! Startup happens at absolute address 0x00000000, which is also where
 * the page directory will exist. The startup code will be overwritten by
 * the page directory.
 */
#;/*
#; *   head.s 含有 32 位启动代码。
#; *
#; * 注意!!! 32 位启动代码是从绝对地址 0x00000000 开始的，因为这里也同样
#; * 会有页目录将存在的地方，因此 setup.s 里的页目录将被覆盖掉。
#; */
.text
.globl idt,gdt,pg_dir,tmp_floppy_area
pg_dir:
.globl startup_32
startup_32:                 #; head 是进入之后的初始化
                            #; 下面 5 行设置各个数据段寄存器，指向 gdt 数据段描述符
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs             #; 指向 gdt 的 0x10 项（数据段）
	lss stack_start,%esp    #; 设置栈（系统栈）
	                        #; struct{long *a; shot b;} stack_start = {&user_stack[PAGE_SIZE>>2], 0x10 }
	call setup_idt          #; 又一次初始化 idt 表和 gdt 表
	call setup_gdt
	movl $0x10,%eax		# reload all the segment registers
	mov %ax,%ds		# after changing gdt. CS was already
	mov %ax,%es		# reloaded in 'setup_gdt'
	mov %ax,%fs
	mov %ax,%gs
	lss stack_start,%esp
	xorl %eax,%eax
1:	incl %eax		# check that A20 really IS enabled
	movl %eax,0x000000	# loop forever if it isn't  #; 意思是 movl %eax, ds:0x000000，把数据段 0 地址处的值赋给 eax
	cmpl %eax,0x100000
	je 1b           #; 数据段 0 地址处和 1M 地址处相同（A20没开启），就死循环

/*
 * NOTE! 486 should set bit 16, to check for write-protect in supervisor
 * mode. Then it would be unnecessary with the "verify_area()"-calls.
 * 486 users probably want to set the NE (#5) bit also, so as to use
 * int 16 for math errors.
 */
 #;/*
 #;* 注意! 在下面这段程序中，486 应该将位 16 置位，以检查在超级用户模式下的写保护,
 #;* 此后"verify_area()"调用中就不需要了。486 的用户通常也会想将NE(#5)置位，以便
 #;* 对数学协处理器的出错使用 int 16。
 #;*/
 #;
 #; 下面这段程序用于检查数学协处理器芯片是否存在。方法是修改控制寄存器 cr0，在假设存在
 #; 协处理器的情况下执行一个协处理器指令，如果出错则说明协处理器芯片不存在。需要设置 cr0
 #; 中协处理器仿真位 EM（位2），并复位协处理器存在标志 MP（位1）
	movl %cr0,%eax		# check math chip
	andl $0x80000011,%eax	# Save PG,PE,ET
/* "orl $0x10020,%eax" here for 486 might be good */
	orl $2,%eax		# set MP
	movl %eax,%cr0
	call check_x87
	jmp after_page_tables       #; 页表

/*
 * We depend on ET to be correct. This checks for 287/387.
 */
 #;/*
 #;* 我们依赖于ET 标志的正确性来检测287/387 存在与否。
 #;*/
check_x87:
	fninit
	fstsw %ax
	cmpb $0,%al
	je 1f			/* no coprocessor: have to set bits */
	                #; 如果存在则跳转到标号 1 处，否则改写 cr0
	movl %cr0,%eax
	xorl $6,%eax		/* reset MP, set EM */
	movl %eax,%cr0
	ret
.align 2
1:	.byte 0xDB,0xE4		/* fsetpm for 287, ignored by 387 */
	ret

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok. This routine will be over-
 *  written by the page tables.
 */
 #;/*
 #; * 下面这段是设置中断描述符表子程序 setup_idt
 #; *
 #; * 将中断描述符表 idt 设置成具有 256 个项，并都指向 ignore_int 中断门。然后加载
 #; * 中断描述符表寄存器(用 lidt 指令)。真正实用的中断门以后再安装。当我们在其它
 #; * 地方认为一切都正常时再开启中断。该子程序将会被页表覆盖掉。
 #; */
setup_idt:
	lea ignore_int,%edx             #; 将 ignore_int 的偏移值存到 edx 寄存器
	movl $0x00080000,%eax           #; 将选择子 0x0008 放到 eax 的高 16 位
	movw %dx,%ax		/* selector = 0x0008 = cs */
	                    #; ignore_int 偏移值的低 16 位置入 eax 的低 16 位中。此时
	                    #; eax 含有门描述符低 4 字节的值
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */
	                    #; 此时 edx 含有门描述符高 4 字节的值。

	lea idt,%edi
	mov $256,%ecx
rp_sidt:
	movl %eax,(%edi)    #; movl %eax,[%edi]      #; 将哑中断门描述符存入表中
	movl %edx,4(%edi)   #; movl %eds,[%edi+4]
	addl $8,%edi        #; edi 指向表中下一项
	dec %ecx
	jne rp_sidt
	lidt idt_descr      #; 加载中断描述符表寄存器值
	ret

/*
 *  setup_gdt
 *
 *  This routines sets up a new gdt and loads it.
 *  Only two entries are currently built, the same
 *  ones that were built in init.s. The routine
 *  is VERY complicated at two whole lines, so this
 *  rather long comment is certainly needed :-).
 *  This routine will beoverwritten by the page tables.
 */
 #;/*
 #; * 下面这段是设置全局描述符表项 setup_gdt
 #; *
 #; * 这个子程序设置一个新的全局描述符表gdt，并加载。此时仅创建了两个表项，与前
 #; * 面的一样。该子程序只有两行，“非常的”复杂，所以当然需要这么长的注释了:)。
 #; */
setup_gdt:
	lgdt gdt_descr
	ret

/*
 * I put the kernel page tables right after the page directory,
 * using 4 of them to span 16 Mb of physical memory. People with
 * more than 16MB will have to expand this.
 */
 #;/*
 #; * Linus 将内核的内存页表直接放在页目录之后，使用了 4 个表来寻址 16 Mb 的物理内存。
 #; * 如果你有多于 16 Mb 的内存，就需要在这里进行扩充修改。
 #; */
 #;
 #; 每个页表长为 4 Kb 字节，而每个页表项需要 4 个字节，因此一个页表共可以存放 1000 个表项，
 #; 如果一个表项寻址 4 Kb 的地址空间，则一个页表就可以寻址 4 Mb 的物理内存。页表项
 #; 的格式为：项的前 0-11 位存放一些标志，如是否在内存中(P 位0)、读写许可(R/W 位1)、
 #; 普通用户还是超级用户使用(U/S 位2)、是否修改过(是否脏了)(D 位6)等；表项的位12-31
 #; 是页框地址，用于指出一页内存的物理起始地址。
.org 0x1000             #; 从偏移 0x1000 处开始是第 1 个页表（偏移 0 开始处将存放页表目录）
pg0:

.org 0x2000
pg1:

.org 0x3000
pg2:

.org 0x4000
pg3:

.org 0x5000             #; 定义下面的内存数据块从偏移 0x5000 处开始
/*
 * tmp_floppy_area is used by the floppy-driver when DMA cannot
 * reach to a buffer-block. It needs to be aligned, so that it isn't
 * on a 64kB border.
 */
 #;/*
 #; * 当DMA（直接存储器访问）不能访问缓冲块时，下面的 tmp_floppy_area 内存块
 #; * 就可供软盘驱动程序使用。其地址需要对齐调整，这样就不会跨越 64kB 边界。
 #; */
tmp_floppy_area:
	.fill 1024,1,0

#; 下面这几个入栈操作(pushl)作用为调用 /init/main.c 中 main() 函数的参数和返回作准备。
#; 入栈操作是模拟调用 main.c 程序时首先将返回地址入栈的操作，所以如果
#; main.c 程序真的退出时，就会返回到这里的标号 L6 处继续执行下去，也即死循环。
#; 将 main.c 的地址压入堆栈，这样，在设置分页处理（setup_paging）结束后
#; 执行 'ret' 返回指令时就会将 main.c 程序的地址弹出堆栈，并去执行 main.c 程序去了。
after_page_tables:
	pushl $0		# These are the parameters to main :-)
	pushl $0
	pushl $0
	pushl $L6		# return address for main, if it decides to.
	pushl $main         #; '_main' 是编译程序对 main 函数的内部表示方法。
	jmp setup_paging    #; 跳到 init/main.c 中的 main 函数
L6:
	jmp L6			# main should never return here, but
				# just in case, we know what happens.
				#; 会发现 jmp L6 是一个循环，也就是说 main.c 中的函数 main 若是 return 了，return 值保存到L6地址，然后死机 = =

/* This is the default interrupt "handler" :-) */
#;/* 下面是默认的中断“向量句柄” :-) */
int_msg:
	.asciz "Unknown interrupt\n\r"
.align 2
ignore_int:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	pushl $int_msg
	call printk     #; 调用 /kernel/printf.c 中的 printk 方法
	popl %eax
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret        #; 中断返回（把中断调用时压入栈的CPU 标志寄存器（32 位）值也弹出）


/*
 * Setup_paging
 *
 * This routine sets up paging by setting the page bit
 * in cr0. The page tables are set up, identity-mapping
 * the first 16MB. The pager assumes that no illegal
 * addresses are produced (ie >4Mb on a 4Mb machine).
 *
 * NOTE! Although all physical memory should be identity
 * mapped by this routine, only the kernel page functions
 * use the >1Mb addresses directly. All "normal" functions
 * use just the lower 1Mb, or the local data space, which
 * will be mapped to some other place - mm keeps track of
 * that.
 *
 * For those with more memory than 16 Mb - tough luck. I've
 * not got it, why should you :-) The source is here. Change
 * it. (Seriously - it shouldn't be too difficult. Mostly
 * change some constants etc. I left it at 16Mb, as my machine
 * even cannot be extended past that (ok, but it was cheap :-)
 * I've tried to show which constants to change by having
 * some kind of marker at them (search for "16Mb"), but I
 * won't guarantee that's all :-( )
 */
 #;/*
 #; * Setup_paging
 #; *
 #; * 这个子程序通过设置控制寄存器 cr0 的标志（PG 位31）来启动对内存的分页处理
 #; * 功能，并设置各个页表项的内容，以恒等映射前 16 MB 的物理内存。分页器假定
 #; * 不会产生非法的地址映射（也即在只有 4Mb 的机器上设置出大于4Mb 的内存地址）。
 #; *
 #; * 注意！尽管所有的物理地址都应该由这个子程序进行恒等映射，但只有内核页面管
 #; * 理函数能直接使用> 1Mb 的地址。所有“一般”函数仅使用低于 1Mb 的地址空间，或
 #; * 者是使用局部数据空间，地址空间将被映射到其它一些地方去-- mm(内存管理程序)
 #; * 会管理这些事的。
 #; *
 #; * 对于那些有多于 16Mb 内存的家伙- 太幸运了，我还没有，为什么你会有:-)。代码就
 #; * 在这里，对它进行修改吧。（实际上，这并不太困难的。通常只需修改一些常数等。
 #; * 我把它设置为 16Mb，因为我的机器再怎么扩充甚至不能超过这个界限（当然，我的机
 #; * 器很便宜的:-)）。我已经通过设置某类标志来给出需要改动的地方（搜索“16Mb”），
 #; * 但我不能保证作这些改动就行了 :-( )
 #; */
.align 2                    #; 按 4 字节方式对齐内存地址边界
setup_paging:               #; 首先对 5 页内存（1 页目录 + 4 页页表）清零
	movl $1024*5,%ecx		/* 5 pages - pg_dir+4 page tables */
	xorl %eax,%eax
	xorl %edi,%edi			/* pg_dir is at 0x000 */        #; 页目录从 0x000 地址开始
	cld;rep;stosl
	#; 下面 4 句设置页目录中的项，我们共有 4 个页表所以只需设置4 项。
    #; 页目录项的结构与页表中项的结构一样，4 个字节为 1 项。参见上面的说明。
    #; "$pg0+7"表示：0x00001007，是页目录表中的第 1 项。
    #; 则第 1 个页表所在的地址= 0x00001007 & 0xfffff000 = 0x1000；第 1 个页表
    #; 的属性标志= 0x00001007 & 0x00000fff = 0x07，表示该页存在、用户可读写。
	movl $pg0+7,pg_dir		/* set present bit/user r/w */
	movl $pg1+7,pg_dir+4		/*  --------- " " --------- */
	movl $pg2+7,pg_dir+8		/*  --------- " " --------- */
	movl $pg3+7,pg_dir+12		/*  --------- " " --------- */
	#; 下面 6 行填写 4 个页表中所有项的内容，共有：4(页表)*1024(项/页表) = 4096 项(0 - 0xfff)，
    #; 也即能映射物理内存 4096*4Kb = 16Mb。
    #; 每项的内容是：当前项所映射的物理内存地址+ 该页的标志（这里均为7）。
    #; 使用的方法是从最后一个页表的最后一项开始按倒退顺序填写。一个页表的最后一项
    #; 在页表中的位置是 1023*4 = 4092。因此最后一页的最后一项的位置就是 $pg3+4092。
	movl $pg3+4092,%edi     #; edi -> 最后一页的最后一项
	movl $0xfff007,%eax		/*  16Mb - 4096 + 7 (r/w user,p) */
	                        #; 最后一项对应的物理内存页面的地址是 0xfff000
	                        #; 加上属性标志 7，即为 0xfff007
	std                     #; 方向位置位，edi 值递减（4 字节）
1:	stosl			/* fill pages backwards - more efficient :-) */
	subl $0x1000,%eax       #; 每填写好一项，物理内存地址值减 0x1000 (4k)
	jge 1b                  #; 如果小于 0，则说明都填写好了
	xorl %eax,%eax		/* pg_dir is at 0x0000 */       #;/* 页目录表(pg_dir)在 0x0000 处。 */
	movl %eax,%cr3		/* cr3 - page directory start */
	movl %cr0,%eax      #; 设置启动使用分页处理（cr0 的 PG 标志，位 31）
	orl $0x80000000,%eax
	movl %eax,%cr0		/* set paging (PG) bit */       #; 添上 PG 标志
	ret			/* this also flushes prefetch-queue */
                #; 在改变分页处理标志后要求使用转移指令刷新预取指令队列，这里用的是返回指令 ret。
                #; 该返回指令的另一个作用是将堆栈中的 main 程序的地址弹出，并开始运行 /init/main.c
                #; 程序。本程序到此真正结束了。

.align 2        #; 按 4 字节方式对齐内存地址边界
.word 0
idt_descr:      #; 下面两行是 lidt 指令的 6 字节操作数：长度，基址
	.word 256*8-1		# idt contains 256 entries
	.long idt
.align 2
.word 0
gdt_descr:      #; 下面两行是lgdt 指令的 6 字节操作数：长度，基址
	.word 256*8-1		# so does gdt (not that that's any
	.long gdt		# magic number, but it works for me :^)

	.align 8
idt:	.fill 256,8,0		# idt is uninitialized

#; 全局表。前 4 项分别是空项（不用）、代码段描述符、数据段描述符、系统段描述符，
#; 其中系统段描述符 linux 没有派用处。后面还预留了 252 项的空间，用于放置所创建
#; 任务的局部描述符(LDT)和对应的任务状态段 TSS 的描述符。
#; (0-nul, 1-cs, 2-ds, 3-sys, 4-TSS0, 5-LDT0, 6-TSS1, 7-LDT1, 8-TSS2 etc...)
gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00c09a0000000fff	/* 16Mb */          #; 代码段最大长度 16M
	.quad 0x00c0920000000fff	/* 16Mb */          #; 数据段最大长度 16M
	.quad 0x0000000000000000	/* TEMPORARY - don't use */
	.fill 252,8,0			/* space for LDT's and TSS's etc */
