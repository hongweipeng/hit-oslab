!
!	setup.s		(C) 1991 Linus Torvalds
!
! setup.s is responsible for getting the system data from the BIOS,
! and putting them into the appropriate places in system memory.
! both setup.s and system has been loaded by the bootblock.
!
! This code asks the bios for memory/disk/other parameters, and
! puts them in a "safe" place: 0x90000-0x901FF, ie where the
! boot-block used to be. It is then up to the protected mode
! system to read them from there before the area is overwritten
! for buffer-blocks.
!

! NOTE! These had better be the same as in bootsect.s!

INITSEG  = 0x9000	! we move boot here - out of the way
SYSSEG   = 0x1000	! system loaded at 0x10000 (65536).
SETUPSEG = 0x9020	! this is the current segment

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

entry start
start:              ! setup.s 的代码已经搬到内存 0x90200 处，段寄存器值 0x9020

! ok, the read went well so we get current cursor position and save it for
! posterity.
! 获取光标位置 =>  0x9000:0

	mov	ax,#INITSEG	! this is done in bootsect already, but...
	mov	ds,ax       ! 设置cs=ds=es
	mov	ah,#0x03	! read cursor pos
	xor	bh,bh
	int	0x10		! save it in known place, con_init fetches ! 取光标位置
	mov	[0],dx		! it from 0x90000.
	                ! 取出光标位置（包括其他位置参数）到 [num] 即 0x90000 + num 处
	                ! 内存地址和保存的值和含义如下表
	                ! | 物理内存地址   | 长度 | 名称       |
                    ! | ------------ | ---- | --------- |
                    ! | 0x90000      | 2    | 光标位置   |
                    ! | 0x90002      | 2    | 扩展内存数 |
                    ! | 0x9000C      | 2    | 显卡参数   |
                    ! | 0x901FC      | 2    | 根设备号   |

! Get memory size (extended mem, kB)
! 获取拓展内存大小 => 0x9000:2

	mov	ah,#0x88
	int	0x15        ! 获取扩展内存大小( 1MB 以后的内存都是扩展内存 )
	mov	[2],ax      ! 扩展内存数保存到 0x90002 处

! Get video-card data:

	mov	ah,#0x0f
	int	0x10
	mov	[4],bx		! bh = display page
	mov	[6],ax		! al = video mode, ah = window width

! check for EGA/VGA and some config parameters

	mov	ah,#0x12
	mov	bl,#0x10
	int	0x10
	mov	[8],ax
	mov	[10],bx
	mov	[12],cx

! Get hd0 data
! 获取硬盘参数 => 0x9000:80  大小：16B

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0080
	mov	cx,#0x10
	rep
	movsb

! Get hd1 data

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x46]
	mov	ax,#INITSEG     ! 前面修改了ds寄存器，这里将其设置为0x9000
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	rep
	movsb

! Check that there IS a hd1 :-)

	mov	ax,#0x01500
	mov	dl,#0x81
	int	0x13
	jc	no_disk1
	cmp	ah,#3
	je	is_disk1
no_disk1:
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0090
	mov	cx,#0x10
	mov	ax,#0x00
	rep
	stosb
is_disk1:

! now we want to move to protected mode ...
! 为进入保护模式做准备：
! 1. 将 system 模块的代码从物理内存地址为 0x90000 ~ ? 移动到 0 ~ 0x90000 的地方

	cli			! no interrupts allowed !   ! 不允许中断

! first we move the system to it's rightful place

	mov	ax,#0x0000
	cld			! 'direction'=0, movs moves forward
do_move:
	mov	es,ax		! destination segment
	add	ax,#0x1000
	cmp	ax,#0x9000
	jz	end_move
	mov	ds,ax		! source segment
	sub	di,di
	sub	si,si
	mov 	cx,#0x8000
	rep
	movsw           ! 将 system 模块移到 0 地址
	jmp	do_move

! then we load the segment descriptors

end_move:
	mov	ax,#SETUPSEG	! right, forgot this at first. didn't work :-)
	mov	ds,ax
	lidt	idt_48		! load idt with 0,0
	lgdt	gdt_48		! load gdt with whatever appropriate
	                    ! 通过 lgdt 指令从内存中读取 48 位的内存数据，存入 GDTR 寄存器
	                    ! 48 位数据表示的是全局描述符表的位置和大小，低 32 位表示起始位置，高16位表示表的最后一个字节的偏移（表的大小-1）

! that was painless, now we enable A20
! 8024 是键盘控制器，其输出端口 P2 用来控制 A20 地址线
	call	empty_8042
	mov	al,#0xD1		! command write     ! D1 表示写数据到 8024 的 P2 端口
	out	#0x64,al
	call	empty_8042
	mov	al,#0xDF		! A20 on            ! 选通 A20 地址线
	out	#0x60,al
	call	empty_8042

! well, that went ok, I hope. Now we have to reprogram the interrupts :-(
! we put them right after the intel-reserved hardware interrupts, at
! int 0x20-0x2F. There they won't mess up anything. Sadly IBM really
! messed this up with the original PC, and they haven't been able to
! rectify it afterwards. Thus the bios puts interrupts at 0x08-0x0f,
! which is used for the internal hardware interrupts as well. We just
! have to reprogram the 8259's, and it isn't fun.

! 初始化 8259 (中断控制)， 一段非常机械化的程序

	mov	al,#0x11		! initialization sequence
	out	#0x20,al		! send it to 8259A-1
	.word	0x00eb,0x00eb		! jmp $+2, jmp $+2
	out	#0xA0,al		! and to 8259A-2
	.word	0x00eb,0x00eb
	mov	al,#0x20		! start of hardware int's (0x20)
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x28		! start of hardware int's 2 (0x28)
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x04		! 8259-1 is master
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x02		! 8259-2 is slave
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x01		! 8086 mode for both
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0xFF		! mask off all interrupts for now
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al

! well, that certainly wasn't fun :-(. Hopefully it works, and we don't
! need no steenking BIOS anyway (except for the initial loading :-).
! The BIOS-routine wants lots of unnecessary data, and it's less
! "interesting" anyway. This is how REAL programmers do it.
!
! Well, now's the time to actually move into protected mode. To make
! things as simple as possible, we do no register set-up or anything,
! we let the gnu-compiled 32-bit programs do that. We just jump to
! absolute address 0x00000, in 32-bit protected mode.
	mov	ax,#0x0001	! protected mode (PE) bit
	lmsw	ax		! This is it!       ! 这条指令类似于 mov cr0, ax
	                                    ! cr0 是一个非常酷的寄存器，它的最后一位如果是0，则是16位模式（实模式）；如果是1，则是32位模式（保护模式）
	jmpi	0,8		! jmp offset 0 of segment 8 (cs)
	                ! jmpi 0 8 进入保护模式，之后寻址模式发生改变。跳转到的其实不是 0x00080 地址，而是 0 地址
	                ! 寻址方式，对于地址 cs:ip
	                ! 实模式下：(cs << 4) + ip
	                ! 保护模式下：(根据 cs 查 gdt 表) + ip
	                !           保护模式下的 cs 又被称为选择子，保护模式下的地址翻译可由硬件完成，
	                !           查表结果是一个 32 位的地址，ip 也是一个 32 位的地址，两个地址相加还是32位

! This routine checks that the keyboard command queue is empty
! No timeout is used - if this hangs there is something wrong with
! the machine, and we probably couldn't proceed anyway.
empty_8042:
	.word	0x00eb,0x00eb
	in	al,#0x64	! 8042 status port
	test	al,#2		! is input buffer full?
	jnz	empty_8042	! yes - loop
	ret

! 保护模式下的地址翻译， 初始化 gdt 表
gdt:                    ! 全称是 Global Description Table  全局描述符表, gdt 表作用可看：https://blog.csdn.net/yeruby/article/details/39718119
	.word	0,0,0,0		! dummy     ! 第一个表项为空，不使用

	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9A00		! code read/exec
	.word	0x00C0		! granularity=4096, 386

	.word	0x07FF		! 8Mb - limit=2047 (2048*4096=8Mb)
	.word	0x0000		! base address=0
	.word	0x9200		! data read/write
	.word	0x00C0		! granularity=4096, 386

                        ! 每个 word 是16位，所以每个表项是 64 位，每个表项占 8 个字节
                        ! 初始化后的 gdt 表项如下
                        ! 第 0 个表项：.word	0,0,0,0
                        ! 第 1 个表项：.word 0x07FF, 0x0000, 0x9A00, 0x00C0
                        ! 第 2 个表项：.word 0x07FF, 0x0000, 0x9200, 0x00C0

                        ! ** 若 cs = 8, ip 0 时，查表出来的基地址是多少？ **
                        ! 8 表示 8 个字节，由于每个表项占 8 个字节，因此找到的是第 1 个表项。
                        ! 表项结构及含义
                        !
                        !64                       55                            47                  39                      32
                        ! | 段基地址 (BASE) 31..24| G | D | 0 | AV L| 限长 19..16| P | DPL | 1 | type | 段基地址 (BASE) 23..16 |
                        ! |              段基地址 (BASE) 15..0                   |            段限长 (LIMIT) 15..0            |
                        !31                        23                           15                   7                      0
                        !
                        ! 第 1 表项为：0x00C0-9A00-0000-07FF
                        !               |      |   |
                        !               |      |   | 取表项中 16 ~ 31 位作为 段地址的 0 ~ 15
                        !              \ /   \ /  \ /
                        ! 得到的段地址  0x00 - 00 - 0000  即 0x00000000 。又由于 ip =0 ，所以会跳到 0 地址去执行。
                        ! 你会发现，第二个表项也是指向 0 地址，这两个表项，一个只读（代码），一个读写（数据）

idt_48:                 ! IDT 表，中断处理函数入口
	.word	0			! idt limit=0
	.word	0,0			! idt base=0L       ! 保护模式中断函数表，保护模式下的中断也是需要通过 idt 表查询处理函数

gdt_48:
	.word	0x800		! gdt limit=2048, 256 GDT entries    ! 共有 256 个 GDT 表项
	.word	512+gdt,0x9	! gdt base = 0X9xxxx
	
.text
endtext:
.data
enddata:
.bss
endbss:
