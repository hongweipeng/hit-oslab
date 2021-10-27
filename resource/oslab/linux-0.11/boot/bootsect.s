!
! SYS_SIZE is the number of clicks (16 bytes) to be loaded.
! 0x3000 is 0x30000 bytes = 196kB, more than enough for current
! versions of linux
;
; SYS_SIZE 的加载按 16 字节为单位进行加载。因为 0x3000 即为 0x30000 bytes = 196 KB
; 对当前的版本空间已经足够了
!
; 指编译连接后 system 模块的大小，这里给出了一个默认值
SYSSIZE = 0x3000            ; system 模块占用 0x30000 bytes 即 196 kB 大小
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

;/* ************************************************************************
;	bootsect 被 BIOS －启动子程序加载至 0x7c00（31k）处，并将自己移动到了
;	地址 0x90000（576k）处，并跳转至那里。
;	它然后使用 BIOS 中断将 'setup' 直接加载到自己的后面（0x90200）（576.5k），
;	并将 system 模块加载到地址 0x10000 处。
;
;	注意：目前的内核系统最大长度限制为（8*65536）（512kB）字节，即使是在
;	将来这也应该没有问题的。我想让它保持简单明了。这样512k的最大内核长度应该
;	足够了，尤其是这里没有像 minix 中一样包含缓冲区高速缓冲。
;
;	加载程序已经做的够简单了，所以持续的读出错将导致死循环。只能手工重启。
;	只要可能，通过一次取取所有的扇区，加载过程可以做的很快的。
;************************************************************************ */

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 4				! nr of setup-sectors                   ; setup 程序代码占用扇区数（setup－sectors）值
BOOTSEG  = 0x07c0			! original address of boot-sector       ; bootsect 程序代码所在内存原始地址（是段地址，以下同）
INITSEG  = 0x9000			! we move boot here - out of the way    ; 将 bootsect 移动到 0x9000 处
SETUPSEG = 0x9020			! setup starts here                     ; setup 程序开始的地址
SYSSEG   = 0x1000			! system loaded at 0x10000 (65536).     ; system 模块加载到 0x10000 （64 kB） 处
ENDSEG   = SYSSEG + SYSSIZE		! where to stop loading             ; system 程序结束的地址 ENDSEG = 0x4000

! ROOT_DEV:	0x000 - same type of floppy as boot.
!		0x301 - first partition on first drive etc
; ROOT_DEV:	0x000 - 根文件系统设备使用与引导时同样的软驱设备
;           0x301 - 根文件系统设备在第一个硬盘的第一个分区上
ROOT_DEV = 0x306            ; 指定根文件系统设备是第1个硬盘的第1个分区。这是 Linux 老式的硬盘命名
                            ; 方式，具体的含义如下：
                            ; 设备号 = 主设备号 * 256 + 次设备号
                            ;         (也即 dev_no = (major << 8) + minor)
                            ; (主设备号：1－内存，2－磁盘，3－硬盘，4－ttyx，5－tty，6－并行口，7－非命名管道)
                            ; 300 - /dev/hd0 － 代表整个第1个硬盘
                            ; 301 - /dev/hd1 － 第1个盘的第1个分区
                            ; ... ...
                            ; 304 - /dev/hd4 － 第1个盘的第4个分区
                            ; 305 - /dev/hd5 － 代表整个第2个硬盘
                            ; 306 - /dev/hd6 － 第2个盘的第1个分区
                            ; ... ...
                            ; 309 - /dev/hd9 － 第1个盘的第4个分区

entry _start                ; 程序入口
_start:
; 以下10行作用是将自身 (bootsect) 从目前段位置 0x07c0 (31k)
; 移动到 0x9000 (576k) 处，共 256 字 （512 字节），然后跳到移动后的 go 标号处，也就是本程序的下一语句处
	mov	ax,#BOOTSEG         ; 此条与就是 0x07c0 处存放的语句
	mov	ds,ax               ; 将段寄存器 ds 设置为 0x07c0
	mov	ax,#INITSEG
	mov	es,ax               ; 将段寄存器 es 设置为 0x9000 => ds=0x07c0  es=0x9000
	mov	cx,#256             ; 移动计数值
	sub	si,si               ; 清零 si, di 寄存器
	sub	di,di
	                        ; 源地址 ds:si = 0x07c0:0x0000
	                        ; 目标地址 es:di = 0x9000:0x0000
	rep                     ; 将 0x07c0:0x0000 处的 256 个字（即 512 个字节）移动到 0x9000:0x0000
	movw
	; 复制完成从 0x9000 的 go 标号处开始执行
	jmpi	go,INITSEG      ; go(偏移量) 赋值给 ip, #INITSEG 赋值给 cs
go:	mov	ax,cs               ; 此时 cs = 0x9000， 执行后 ax = 0x9000
	mov	ds,ax
	mov	es,ax               ; 设置 ds,es,ss 都为移动后的段处 cs 即 0x9000
	                        ; 为 call 做准备
! put stack at 0x9ff00.
; 将堆栈指针 sp 指向 0x9ff00  (即 0x9000:0xff00) 处
	mov	ss,ax
	mov	sp,#0xFF00		! arbitrary value >>512
	                        ; /* 由于代码段移动过了，所以需要重新设置堆栈段的位置。
	                        ;    sp 只要指向大于 512 偏移（即 0x90200h) 处都可以；
	                        ;    因为从 0x90200 地址开始处还要放置 setup 程序，
	                        ;    而此时 setup 程序大约为 4 个扇区，因此 sp 要指向大于
	                        ;    (0x200 + 0x200 * 4 + 堆栈大小) 处
	                        ;    在 linux 0.11 中，sp 设置为了 0xff00

! load the setup-sectors directly after the bootblock.
! Note that 'es' is already set up.

; 在 bootsect 程序块后紧跟着加载 setup 模块的代码数据。
; 注意 es 已经设置号了。（在代码移动后 es 已经指向了目的段地址处 0x9000 处)

load_setup:             ; 载入 setup 模块
	mov	dx,#0x0000		! drive 0, head 0
	mov	cx,#0x0002		! sector 2, track 0
	mov	bx,#0x0200		! address = 512, in INITSEG
	mov	ax,#0x0200+SETUPLEN	! service 2, nr of sectors
	int	0x13			! read it
	                    ; 从磁盘读取并放置内存 es:bx 处，es:bx 指向数据缓冲区，如果出错 CF 标志置位
	jnc	ok_load_setup		! ok - continue     ; 加载成功，跳到 ok_load_setup: 执行
	! 加载错误，则重新尝试
	mov	dx,#0x0000
	mov	ax,#0x0000		! reset the diskette
	int	0x13            ; BIOS 中断
	                    ; 0x13 是 BIOS 读磁盘扇区的终端
	                    ; ah = 0x02 - 读磁盘   al = 扇区数量(SETUPLEN=4)
	                    ; ch = 柱面号          cl = 开始扇区
	                    ; dh = 磁头号          dl = 驱动器号
	                    ;
	                    ; 读到内存地址 es:bx 中，此时 es = 0x9000 , bx = 0x0200 ; 所以实际内存地址为 0x90200
	j	load_setup

ok_load_setup:

! Get disk drive parameters, specifically nr of sectors/track

	mov	dl,#0x00
	mov	ax,#0x0800		! AH=8 is get drive parameters
	                    ; ah = 0x08 - 获取磁盘参数
	                    ; dl = 驱动器号
	int	0x13
	mov	ch,#0x00
	seg cs              ; 表示下一条语句的操作数在 cs 段寄存器所指的段中
	mov	sectors,cx      ; 由于上一句 seg cs 的缘故，这行理解为 mov cs:sectors, cx
	                    ; 因为已经获取的磁盘参数，此时 cx 的值表示了每个磁道扇区数
	mov	ax,#INITSEG
	mov	es,ax           ; 应为上面中断取磁盘参数会改掉 es 值，这里重新改回

! Print some inane message
; 输出一些信息

	mov	ah,#0x03		! read cursor pos ! 读取光标
	xor	bh,bh
	int	0x10            ; 10 号中断，显示字符
	
	mov	cx,#24          ; 24 ,说明要输出 24 个字符
	mov	bx,#0x0007		! page 0, attribute 7 (normal)  ; 7 是显示属性
	mov	bp,#msg1        ; es:bp 指向待显示 字符串
	mov	ax,#0x1301		! write string, move cursor     ; 写字符串并移动光标
	int	0x10

! ok, we've written the message, now
! we want to load the system (at 0x10000)
; 现在开始将 system 模块加载到 0x10000 (64kB) 处

	mov	ax,#SYSSEG      ; SYSSEG = 0x1000
	mov	es,ax		! segment of 0x010000
	call	read_it     ; 读入 system 模块
	call	kill_motor  ; 关闭驱动器马达，这样就可以知道驱动器的状态了

! After that we check which root-device to use. If the device is
! defined (!= 0), nothing is done and the given device is used.
! Otherwise, either /dev/PS0 (2,28) or /dev/at0 (2,8), depending
! on the number of sectors that the BIOS reports currently.

; 此后，我们检查要使用哪个根文件系统设备（简称根设备）。如果已经指定了设备(!=0) ,
; 就直接使用给定的设备；否则就根据 BIOS 报告的每磁道扇区数来确定到底使用
; /dev/PS0 (2,28) 还是 /dev/at0 (2,8) 。
;       上卖弄两个设备文件的含义：
;       在 Linux 中软驱的主设备号是 2 (参考前面 ROOT_DEV 的注释)，次设备号 = type * 4  + nr,
;       其中 nr 为 0~3 分别对应软驱 A, B, C 或 D；type 是软驱的类型 (2 -> 1.2M 或 7 -> 1.44 等)
;       因为 7 * 4 + 0 = 28， 所以 /dev/PS0 (2,28) 是指 1.44M A驱动器，其设备号是 021c
;       同理 /dev/at (2,8) 指的是 1.2M A驱动器，其设备号 0208

	seg cs
	mov	ax,root_dev     ; mov ax, cs:root_dev
	cmp	ax,#0
	jne	root_defined    ; 如果 ax != 0, 转到 root_defined
	seg cs
	mov	bx,sectors
	mov	ax,#0x0208		! /dev/ps0 - 1.2Mb
	cmp	bx,#15
	je	root_defined
	mov	ax,#0x021c		! /dev/PS0 - 1.44Mb
	cmp	bx,#18
	je	root_defined
undef_root:             ; 如果都没有，则死循环（死机）
	jmp undef_root
root_defined:
	seg cs
	mov	root_dev,ax     ; 将检查过的设备号保存起来

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
; 这个程序将 system 模块加载到内存 0x10000 处，并确定没有跨越 64kB 的内存边界。
; 我们试图尽快的加载，只要可能，就每次加载整条磁道的数据
;
; in: es - 开始内存地址段（通常是 0x1000）
;
sread:	.word 1+SETUPLEN	! sectors read of current track ; 当前磁道中已读的扇区数。开始时已经读进 1 扇区的引导扇区
head:	.word 0			! current head                      ; 当前磁头号
track:	.word 0			! current track                     ; 当前磁道号

; 测试输入的段值。必须位于内存地址 64kB 边界处，否则进入死循环
read_it:                ; 为什么读入 system 模块还需要定义一个函数？ system 模块可能很大，要跨越磁道，用函数更灵活。
	mov ax,es
	test ax,#0x0fff
die:	jne die			! es must be at 64kB boundary       ; es 值必须位于 64kB 边界处
	xor bx,bx		! bx is starting address within segment ; bx 为段内偏移地址
	                ; 清 bx 寄存器，用于表示当前段内保存数据的开始位置
rp_read:
; 判断是否已经读入全部数据。比较当前所读段是否是 ssytem 数据末端所处的段 (#ENDSEG)：
; - 如果不是，就跳转至下面的 ok1_read 标号处继续读取数据；
; - 如果是，退出子程序返回
	mov ax,es
	cmp ax,#ENDSEG		! have we loaded all yet?
	jb ok1_read         ! ENDSEG = SYSSEG + SYSSIZE
	                    ! SYSSIZE = 0x3000  // 该变量可根据 Image 大小设定（编译操作系统时）
	ret
ok1_read:
; 计算和验证当前磁道需要读取的扇区数，放在 ax 寄存器中。
; 根据当前磁道还未读取的扇区数以及段内数据字节开始偏移位置，计算如果全部读取这些未读扇区，读取总字节数
; 是否超过 64kB 段长度的限制，如果超过，则根据此次最多能读入的字节数 (64kB - 段内偏移位置)，反算出
; 此次需要读取的扇区数。
	seg cs
	mov ax,sectors      ; 取每磁道的扇区数
	sub ax,sread        ; sread 是表示当前磁道已读扇区数，因此 ax 为未读扇区数
	mov cx,ax
	shl cx,#9           ; shl - 逻辑左移    cx = (ax * 512) 字节
	add cx,bx           ; cx = cx + 段内当前偏移值(bx)
	                    ; 操作后，cx 值含义是段内共读入的字节数
	jnc ok2_read        ; 若没有超过 64kB 则跳至 ok2_read 处执行
	je ok2_read
	xor ax,ax           ; 若加上此次读取磁道上所有未读扇区时超过 64kB ，则计算
	                    ; 此时最多能读入的字节数 (64kB - 段内读偏移位置 bx) ，再转成需要读取的扇区数
	sub ax,bx
	shr ax,#9           ; shr - 逻辑右移  ax = ax / 512
ok2_read:
	call read_track
	mov cx,ax           ; 再调用了 read_track ，ax 里的值含义是已读取的扇区数 ？
	add ax,sread
	seg cs
	cmp ax,sectors      ; 如果当前磁道上还有未读扇区，则跳转至 ok_read 处
	jne ok3_read
	mov ax,#1           ; 读该磁道的下一磁头面（1 号磁头）上的数据。如果已完成，则去读下一磁道。
	sub ax,head         ; 判断当前磁头号
	jne ok4_read        ; 如果是 0 磁头，则再去读 1 磁头面上的扇区数据
	inc track           ; 否则去读下一磁道
ok4_read:
	mov head,ax         ; 保存当前磁头号
	xor ax,ax           ; 请当前磁道已读扇区数
ok3_read:
	mov sread,ax        ; 保存当前磁道已读扇区数
	shl cx,#9           ; 上次已读扇区数 * 512 字节
	add bx,cx           ; 调整当前段内数据开始位置
	jnc rp_read         ; 若小于 64kB 边界值，则跳至 rp_read 处，继续读取数据
	; 调整当前段，为读下一段数据做准备
	mov ax,es
	add ax,#0x1000      ; 将段基址调整为指向下一个 64kB 段内存
	mov es,ax
	xor bx,bx
	jmp rp_read

; 读当前磁道上指定开始扇区和需读扇区的数据到 es:bx 开始处
; al - 需读扇区数；   es:bx - 缓冲区开始位置
read_track:     // 读磁道
	push ax
	push bx
	push cx
	push dx
	mov dx,track        ; 取当前磁道号
	mov cx,sread        ; 取当前磁道上已读扇区数
	inc cx              ; cl = 开始读扇区
	mov ch,dl           ; ch = 当前磁道号
	mov dx,head         ; 取当前磁头号
	mov dh,dl           ; dh = 磁头号
	mov dl,#0           ; dl = 驱动器号 （0 表示当前驱动器）
	and dx,#0x0100      ; 磁头号不大于 1
	mov ah,#2           ; al = 2 ，去读磁盘扇区功能号
	int 0x13
	jc bad_rt           ; 若出错，跳至 bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret

; 执行驱动器复位操作（磁盘中断功能号0），再跳转到 read_track 处重试。
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
; 这个子程序用于关闭软驱的马达，这样我进入内核后它处于已知状态，以后也就无需担心它了。
kill_motor:
	push dx
	mov dx,#0x3f2       ; 软驱控制卡的驱动端口，只写
	mov al,#0           ; A 驱动器，关闭 FDC，禁止 DMA 和中断请求，关闭马达
	outb
	pop dx
	ret

sectors:                ; 存放当前每个磁道的扇区数
	.word 0             ; 磁道扇区数

msg1:
	.byte 13,10         ; \r\n 的 ascii 码
	.ascii "Loading system ..."
	.byte 13,10,13,10

.org 508                ; 引导区的末尾 BIOS 用以识别引导区
root_dev:               ; 存放根文件所在的设备号 (init/main.c 中有用到)
	.word ROOT_DEV
boot_flag:              ; 硬盘有效标识
	.word 0xAA55        ; 扇区的最后两个字节，否则会打出非引导设备

.text
endtext:
.data
enddata:
.bss
endbss:
