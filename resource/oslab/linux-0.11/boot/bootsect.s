!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
!
SYSSIZE = 0x3000            ! system 模块占用 0x30000 bytes 即 196kB 大小
!
!	bootsect.s		(C) 1991 Linus Torvalds
!
! bootsect.s is loaded at 0x7c00 by the bios-startup routines, and moves
! iself out of the way to address 0x90000, and jumps there.
!
! It then loads 'setup' directly after itself (0x90200), and the system
! at 0x10000, using BIOS interrupts. 
!
! NOTE! currently system is at most 8*65536 bytes long. This should be no
! problem, even in the future. I want to keep it simple. This 512 kB
! kernel size should be enough, especially as this doesn't contain the
! buffer cache as in minix
!
! The loader has been made as simple as possible, and continuos
! read errors will result in a unbreakable loop. Reboot by hand. It
! loads pretty fast by getting whole sectors at a time whenever possible.

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4				! nr of setup-sectors                   ! setup程序代码占用扇区数
BOOTSEG  = 0x07c0			! original address of boot-sector       ! bootsect程序代码所在内存原始地址
INITSEG  = 0x9000			! we move boot here - out of the way    ! 将bootsect移动到0x9000处
SETUPSEG = 0x9020			! setup starts here                     ! setup程序开始的地址
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).     ! system程序开始的地址
ENDSEG   = SYSSEG + SYSSIZE		! where to stop loading             ! system程序结束的地址

! ROOT_DEV:	0x000 - same type of floppy as boot.
!		0x301 - first partition on first drive etc
ROOT_DEV = 0x306

entry _start
_start:
! 下面这段代码将自身复制到 0x9000 处
	mov	ax,#BOOTSEG         ! 此条与就是 0x07c0 处存放的语句
	mov	ds,ax
	mov	ax,#INITSEG
	mov	es,ax               ! ds=0x07c0  es=0x9000
	mov	cx,#256
	sub	si,si               ! 清零 si, di 寄存器
	sub	di,di
	rep                     ! 将 0x07c0:0x0000 处的 256 个字移动到 0x9000:0x0000
	movw
	! 复制完成从 0x9000 的 go 标号处开始执行
	jmpi	go,INITSEG      ! go(偏移量) 赋值给 ip, #INITSEG 赋值给 cs
go:	mov	ax,cs               ! 此时 cs = 0x9000， 执行后 ax = 0x9000
	mov	ds,ax
	mov	es,ax               ! 设置 ds,es,ax 都为 cs 即 0x9000
	                        ! 为 call 做准备
! put stack at 0x9ff00.
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >>512

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

load_setup:             ! 载入 setup 模块
	mov	dx,#0x0000		! drive 0, head 0
	mov	cx,#0x0002		! sector 2, track 0
	mov	bx,#0x0200		! address = 512, in INITSEG
	mov	ax,#0x0200+SETUPLEN	! service 2, nr of sectors
	int	0x13			! read it
	jnc	ok_load_setup		! ok - continue     ! 加载成功，跳到 ok_load_setup: 执行
	! 加载错误
	mov	dx,#0x0000
	mov	ax,#0x0000		! reset the diskette
	int	0x13            ! BIOS 中断
	                    ! 0x13 是 BIOS 读磁盘扇区的终端
	                    ! ah = 0x02-读磁盘
	                    ! al = 扇区数量(SETUPLEN=4)
	                    ! ch = 柱面号
	                    ! cl = 开始扇区
	                    ! dh = 磁头号
	                    ! dl = 驱动器号
	                    ! 读到内存地址 es:bx 中，此时 es = 0x9000 , bx = 0x0200 ; 所以实际内存地址为 0x90200
	j	load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track

	mov	dl,#0x00
	mov	ax,#0x0800		! AH=8 is get drive parameters
	int	0x13
	mov	ch,#0x00
	seg cs
	mov	sectors,cx
	mov	ax,#INITSEG
	mov	es,ax

! Print some inane message
!输出一些信息

	mov	ah,#0x03		! read cursor pos ! 读取光标
	xor	bh,bh
	int	0x10            ! 10 号中断，显示字符
	
	mov	cx,#24          ! 24 ,说明要输出 24 个字符
	mov	bx,#0x0007		! page 0, attribute 7 (normal)  ! 7 是显示属性
	mov	bp,#msg1        ! es:bp 指向待显示 字符串
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)

	mov	ax,#SYSSEG      ! SYSSEG = 0x1000
	mov	es,ax		! segment of 0x010000
	call	read_it     ! 读入 system 模块
	call	kill_motor

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.

	seg cs
	mov	ax,root_dev
	cmp	ax,#0
	jne	root_defined
	seg cs
	mov	bx,sectors
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15
	je	root_defined
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax

! after that (everyting loaded), we jump to
! the setup-routine loaded directly after
! the bootblock:
!开始执行setup代码
	jmpi	0,SETUPSEG      ! 转入地址 0x9020:0x0000,  执行 setup.s

! This routine loads the system at address 0x10000, making sure
! no 64kB boundaries are crossed. We try to load it as fast as
! possible, loading whole tracks whenever we can.
!
! in:	es - starting address segment (normally 0x1000)
!
sread:	.word 1+SETUPLEN	! sectors read of current track
head:	.word 0			! current head
track:	.word 0			! current track

read_it:                ! 为什么读入 system 模块还需要定义一个函数？ system 模块可能很大，要跨越磁道。
	mov ax,es
	test ax,#0x0fff
die:	jne die			! es must be at 64kB boundary
	xor bx,bx		! bx is starting address within segment
rp_read:
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?
	jb ok1_read         ! ENDSEG = SYSSEG + SYSSIZE
	                    ! SYSSIZE = 0x3000  // 该变量可根据 Image 大小设定（编译操作系统时）
	ret
ok1_read:
	seg cs
	mov ax,sectors
	sub ax,sread        ! sread 是当前磁道以读扇区数，ax 为未读扇区数
	mov cx,ax
	shl cx,#9
	add cx,bx
	jnc ok2_read
	je ok2_read
	xor ax,ax
	sub ax,bx
	shr ax,#9
ok2_read:
	call read_track
	mov cx,ax
	add ax,sread
	seg cs
	cmp ax,sectors
	jne ok3_read
	mov ax,#1
	sub ax,head
	jne ok4_read
	inc track
ok4_read:
	mov head,ax
	xor ax,ax
ok3_read:
	mov sread,ax
	shl cx,#9
	add bx,cx
	jnc rp_read
	mov ax,es
	add ax,#0x1000
	mov es,ax
	xor bx,bx
	jmp rp_read

read_track:     // 读磁道
	push ax
	push bx
	push cx
	push dx
	mov dx,track
	mov cx,sread
	inc cx
	mov ch,dl
	mov dx,head
	mov dh,dl
	mov dl,#0
	and dx,#0x0100
	mov ah,#2
	int 0x13
	jc bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
bad_rt:	mov ax,#0
	mov dx,#0
	int 0x13
	pop dx
	pop cx
	pop bx
	pop ax
	jmp read_track

!/*
! * This procedure turns off the floppy drive motor, so
! * that we enter the kernel in a known state, and
! * don't have to worry about it later.
! */
kill_motor:
	push dx
	mov dx,#0x3f2
	mov al,#0
	outb
	pop dx
	ret

sectors:
	.word 0             ! 磁道扇区数

msg1:
	.byte 13,10
	.ascii "Loading system ..."
	.byte 13,10,13,10

.org 508                ! 引导区的末尾 BIOS 用以识别引导区
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xAA55        ! 扇区的最后两个字节，否则会打出非引导设备

.text
endtext:
.data
enddata:
.bss
endbss:
