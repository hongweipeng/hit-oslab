#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
# 如果你要使用 RAM 盘设备的话，就
# 定义块的大小
RAMDISK =  #-DRAMDISK=512

AS86	=as86 -0 -a			# 8086 汇编编译器，见列表后的介绍。后带的参数含义分别是
							# -0: 生成8086 目标程序；-a 生成与 gas 和 gld 部分兼容的代码
LD86	=ld86 -0			# 8086 汇编连接器，参数含义同 连接器 as86

AS	=as						# GNU 汇编编译器和连接器，见列表后的介绍
LD	=ld

# 下面是 GNU 链接器运行时用到的选项
LDFLAGS	=-m elf_i386 -Ttext 0 -e startup_32

# gcc 是 GNU C 程序编译器。对于 UNIX 类的脚本(script)程序而言，
# 在引用定义的标识符时，需在前面加上 $ 符号并用括号括住标识符。
CC	=gcc-3.4 -march=i386 $(RAMDISK)
# 选项含义为：-Wall 打印所有警告信息；-O 对代码进行优化
CFLAGS	=-m32 -g -Wall -O2 -fomit-frame-pointer 

# 下面 cpp 是 gcc 的预处理程序。预处理程序用于进行程序中的宏替换处理、条件编译处理以及
# 包含进指定文件的内容，即把使用 `#include` 指定的文件包含进来。源程序文件中所有以符号 `#`
# 开始的行均需要预处理器进行处理。处理了包括 `#define #if #ifdef #endif` 等
# -nostdinc -Iinclude 参数含义是不要搜索标准头文件目录的文件，即不使用系统 `/usr/include/`
# 目录下的头文件，而是使用 `-I` 选项指定的目录或者是在当前目录里搜索头文件。
CPP	=cpp -nostdinc -Iinclude

#
# ROOT_DEV specifies the default root-device when making the image.
# This can be either FLOPPY, /dev/xxxx or empty, in which case the
# default of /dev/hd6 is used by 'build'.
#
# ROOT_DEV 指定在创建内核镜像（image）文件时使用的默认根文件系统所在的设备，
# 这可以时软盘（FLOPPY）、/dev/xxxx 或者空着；当空着时， build 程序(tools/目录中)
# 就使用默认值 /dev/hd6
ROOT_DEV= #FLOPPY

# 下面是 kernel 目录、mm 目录和 fs 目录所产生的目标代码文件。为了方便引用这里将用
# ARCHIVES (归档文件)标识符表示
ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o

# 块和字符设备库文件。 `.a` 表示该文件时归档文件，也即包含有许多可执行二进制子程序
# 集合的库文件，通常是用 GNU 的 ar 程序生成。ar 是 GNU 的二进制文件处理程序，用于
# 创建、修改以及从归档文件中抽取文件。
DRIVERS =kernel/blk_drv/blk_drv.a kernel/chr_drv/chr_drv.a

# 数学运算库文件
MATH	=kernel/math/math.a

# 由 `lib/` 目录中的文件所编译生成的通用库文件
LIBS	=lib/lib.a


# 下面是 make 老式的隐式后缀规则。该行指示 make 利用下面的命令将所有的 `.c` 文件编译生成
# `.s` 汇编程序。`:` 表示下面的该规则的命令。整句表示让 gcc 采用前面 CFLAGS 所指定的选项
# 以及仅使用 `include/` 目录中的头文件，在适当地编译后不进行汇编就停止 (-S)，从而产生于输
# 入地各个 C 文件对应的汇编语言形式的代码文件。默认情况下所产生的汇编程序文件是原 C 文件名
# 去掉 `.c` 后再加上 `.s` 后缀。`-o` 表示输出文件的形式，`$*.s` 或 (`$@`) 是自动目标变量，
# `$<` 代表第一个先决条件，这里既是符合条件 `*.c` 的文件。
# 下面这 3 个不同规则分别用于不同的操作要求。若目标文件不是 .s 文件，而源文件是 .c 文件则会
# 使用第一个规则；若目录是 .o,而源文件是 .s，则使用第二个规则；若目标文件是 .o 文件而原文件
# 是 c 文件，则使用第三个规则。
.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<

# 将所有的 .s 文件文件编译成 .o 文件
.s.o:
	$(AS)  -o $*.o $<

# 将所有 .c 文件变成成 .o 文件
.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<

# 下面 `all` 表示创建 Makefile 所知的最顶层目标，这里既是 Image 文件。这里生成的 Image 文件
# 既是引导启动盘映像文件 bootimage。若将其写入软盘就可以使用该软盘引导 Linux 系统了。
all:	Image

# 目标文件 Image 是由冒号后面 4 个元素产生，其中 tools/build 工具程序将 bootsect、setup 和 system 文件
# 以  $(ROOT_DEV) 为根文件系统设备组装成内核映像文件 Image。 sync 同步命令是迫使缓冲块数据立即写盘并更新
# 超级块。
Image: boot/bootsect boot/setup tools/system tools/build
	cp -f tools/system system.tmp
	strip system.tmp
	objcopy -O binary -R .note -R .comment system.tmp tools/kernel
	tools/build boot/bootsect boot/setup tools/kernel $(ROOT_DEV) > Image
	rm system.tmp
	rm tools/kernel -f
	sync

# disk 这个目标由 Image 产生。dd 为 UNIX 标准命令：复制一个文件，根据选项进行转换和格式化。bs=表示一次读/写的
# 字节数；if=表示输入的文件；of=表示输出的文件。这里 /dev/fd0 是设备文件
disk: Image
	dd bs=8192 if=Image of=/dev/fd0

BootImage: boot/bootsect boot/setup tools/build
	tools/build boot/bootsect boot/setup none $(ROOT_DEV) > Image
	sync

# 由 tools 目录下的 build.c 生成执行程序 build
tools/build: tools/build.c
	gcc $(CFLAGS) \
	-o tools/build tools/build.c

# 利用上卖弄给出的 `.s.o` 规则生成 head.o 目标文件
boot/head.o: boot/head.s
	gcc-3.4 -m32 -g -I./include -traditional -c boot/head.s
	mv head.o boot/

# 表示 tools 目录中的 system 文件由冒号后面所列的元素生成
# 最后的 > System.map 表示 gld 需要将连接映像重定向存放再 System.map 文件中。
# 管旭 System.map 文件的用途见注释后的说明。
tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(DRIVERS) $(MATH) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(DRIVERS) \
	$(MATH) \
	$(LIBS) \
	-o tools/system 
	nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map 

# 数学协处理函数 math.a ，进入 kernel/math 目录，执行 make 工具程序
kernel/math/math.a: FORCE
	(cd kernel/math; make)

# 生成块设备库文件 blk_drv.a
kernel/blk_drv/blk_drv.a: FORCE
	(cd kernel/blk_drv; make)

# 生成字符设备函数文件 chr_dry.a
kernel/chr_drv/chr_drv.a: FORCE
	(cd kernel/chr_drv; make)

# 生成内核目标模块 kernel.o
kernel/kernel.o: FORCE
	(cd kernel; make)

# 内存管理模块 mm.o
mm/mm.o: FORCE
	(cd mm; make)

# 文件系统目标模块 fs.o
fs/fs.o: FORCE
	(cd fs; make)

# 库函数 lib.a
lib/lib.a: FORCE
	(cd lib; make)

# 使用 8086 汇编连接器对 setup.s 文件进行编译生成 setup 文件
# -s 表示去除目标文件中的符号信息
boot/setup: boot/setup.s
	$(AS86) -o boot/setup.o boot/setup.s
	$(LD86) -s -o boot/setup boot/setup.o

boot/bootsect:	boot/bootsect.s
	$(AS86) -o boot/bootsect.o boot/bootsect.s
	$(LD86) -s -o boot/bootsect boot/bootsect.o

# 下面四行的作用是在 bootsect.s 程序开头添加
# 一行有关 system 文件长度信息。方法是首先生成含有 `SYSSIZE = system 文件实际长度`
# 一行信息的tmp.s 文件，然后将bootsect.s 文件添加在其后。取得system 长度的方法是：
# 首先利用命令ls 对system 文件进行长列表显示，用grep 命令取得列表行上文件字节数字段
# 信息，并定向保存在tmp.s 临时文件中。cut 命令用于剪切字符串，tr 用于去除行尾的回车符。
# 其中：(实际长度 + 15)/16 用于获得用‘节’表示的长度信息。1 节 = 16 字节。
# 注意，这是 linux 0.11 之前使用的方法（获取 system 模块长度并添加到 bootsect.s 中）。后续
# 不再使用这个方法了，而是直接再 bootsect.s 程序开始处给出了 system 模块的一个最大默认值
tmp.s:	boot/bootsect.s tools/system
	(echo -n "SYSSIZE = (";ls -l tools/system | grep system \
		| cut -c25-31 | tr '\012' ' '; echo "+ 15 ) / 16") > tmp.s
	cat boot/bootsect.s >> tmp.s

# 当执行'make clean'时，就会执行98--103 行上的命令，去除所有编译连接生成的文件。
# 'rm'是文件删除命令，选项-f 含义是忽略不存在的文件，并且不显示删除信息
clean:
	rm -f Image System.map tmp_make core boot/bootsect boot/setup
	rm -f init/*.o tools/system tools/build boot/*.o
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

# 该规则将首先执行上面的clean 规则，然后对linux/目录进行压缩，生成
# backup.Z 压缩文件。'cd .. '表示退到linux/的上一级（父）目录；
# 'tar cf - linux'表示对linux/目录执行tar 归档程序。-cf 表示需要创建
# 新的归档文件 '| compress -'表示将tar 程序的执行通过管道操作('|')
# 传递给压缩程序compress，并将压缩程序的输出存成backup.Z 文件。
backup: clean
	(cd .. ; tar cf - linux | compress16 - > backup.Z)
	sync

# 该目标或规则用于各文件之间的依赖关系。创建的这些依赖关系是为了给make 用来确定是否需要要
# 重建一个目标对象的。比如当某个头文件被改动过后，make 就通过生成的依赖关系，重新编译与该
# 头文件有关的所有*.c 文件。具体方法如下：
# 使用字符串编辑程序sed 对Makefile 文件（这里即是自己）进行处理，输出为删除Makefile
# 文件中'### Dependencies'行后面的所有行（下面从118 开始的行），并生成tmp_make
# 临时文件（也即110 行的作用）。然后对init/目录下的每一个C 文件（其实只有一个文件
# main.c）执行gcc 预处理操作，-M 标志告诉预处理程序输出描述每个目标文件相关性的规则，
# 并且这些规则符合make 语法。对于每一个源文件，预处理程序输出一个make 规则，其结果
# 形式是相应源程序文件的目标文件名加上其依赖关系--该源文件中包含的所有头文件列表。
# 111 行中的$$i 实际上是$($i)的意思。这里$i 是这句前面的shell 变量的值。
# 然后把预处理结果都添加到临时文件tmp_make 中，然后将该临时文件复制成新的Makefile 文件。
dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

# Force make run into subdirectories even no changes on source
FORCE:

### Dependencies:
init/main.o: init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/signal.h include/asm/system.h \
  include/asm/io.h include/stddef.h include/stdarg.h include/fcntl.h
