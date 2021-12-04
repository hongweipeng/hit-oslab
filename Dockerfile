FROM ubuntu:20.04
COPY resource/gcc-3.4-ubuntu.tar.gz /root/
COPY resource/hit-oslab-linux-20110823.tar.gz /root/
RUN echo "deb http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" > /etc/apt/sources.list \
&& echo "deb-src http://mirrors.aliyun.com/ubuntu/ focal main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb-src http://mirrors.aliyun.com/ubuntu/ focal-security main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb-src http://mirrors.aliyun.com/ubuntu/ focal-updates main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb-src http://mirrors.aliyun.com/ubuntu/ focal-proposed main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list \
&& echo "deb-src http://mirrors.aliyun.com/ubuntu/ focal-backports main restricted universe multiverse" >> /etc/apt/sources.list \
# 支持 i386 架构
&& dpkg --add-architecture i386 \
# 查看支持的架构
&& dpkg --print-architecture \
&& dpkg --print-foreign-architectures \
# 为了静默安装 x-window-system-core ，需要设置环境变量 DEBIAN_FRONTEND \
&& export DEBIAN_FRONTEND=noninteractive \
# 更新软件源
&& apt update \
# 常用软件
&& apt install -y \
gcc gcc-multilib gdb make man vim openssh-server \
# x-window-system-core 桌面环境
x-window-system-core \
# 图形界面测试  执行：xarclock
xarclock \
# vim 中可输入中文
ttf-wqy-microhei ttf-wqy-zenhei \
# 编译oslab需要的
# as86 汇编指令集
bin86 \
# ./bochs/bochs-gdb: error while loading shared libraries: libSM.so.6: cannot open shared object file: No such file or directory
libsm6:i386 \
# ./bochs/bochs-gdb: error while loading shared libraries: libX11.so.6: cannot open shared object file: No such file or directory
libx11-6:i386 \
# ./bochs/bochs-gdb: error while loading shared libraries: libXpm.so.4: cannot open shared object file: No such file or directory
libxpm4:i386 \
# 安装 gcc-3.4
&& cd /root/ \
&& tar -zxvf gcc-3.4-ubuntu.tar.gz \
&& cd gcc-3.4 \
&& ./inst.sh amd64 \
# 编译测试
&& cd /root/ \
&& tar -zxvf hit-oslab-linux-20110823.tar.gz \
&& cd oslab/linux-0.11/ \
&& make all \
# 清理
&& apt clean \
# 设置 Shell 字符编码
&& echo "alias rm='rm -i'" >> /root/.bashrc \
&& echo "alias cp='cp -i'" >> /root/.bashrc \
&& echo "alias mv='mv -i'" >> /root/.bashrc \
&& echo "export LESSCHARSET=utf-8" >> /root/.bashrc \
# sshd 服务
&& sed -i "s/#Port.*/Port 6222/g" /etc/ssh/sshd_config \
&& sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config \
# SSH login fix. Otherwise user is kicked off after login
&& sed -i 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' /etc/pam.d/sshd \
&& echo "root:oslab" | chpasswd
