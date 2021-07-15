

操作系统（哈工大李治军老师）的超清版本，课件可在下方链接获取。
慕课网: http://www.feemic.cn/mooc/icourse163/1002692015#。
百度云链接：https://pan.baidu.com/s/1h2aEk6A_DGpXkZvRtNmeUw 提取码：qoll
配套实验课：https://www.shiyanlou.com/courses/115

实验环境在ubuntu 20.04上：
下载成功以后进入相关目录，并尝试如下命令通过Bochs启动Linux 0.11操作系统。

```
cd oslab
./run
```

如出现如下错误

```
./bochs/bochs-gdb: error while loading shared libraries: libSM.so.6: cannot open shared object file: No such file or directory
```

则显示系统缺少相关的链接库，通过以下命令下载

```
sudo apt-get install libsm6:i386
```

再次运行如出现如下错误

```
./bochs/bochs-gdb: error while loading shared libraries: libX11.so.6: cannot open shared object file: No such file or directory
```

则通过如下命令下载对应缺失的库

```
sudo apt-get install libx11-6:i386
```

再次运行如出现如下错误

```
./bochs/bochs-gdb: error while loading shared libraries: libXpm.so.4: cannot open shared object file: No such file or directory
```

则通过如下命令下载对应缺失的库

```
sudo apt-get install libxpm4:i386
```
经过上述步骤，再次运行，成功


