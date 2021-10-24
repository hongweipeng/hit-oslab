//
// Created by admin on 2021/10/24.
//

#define __LIBRARY__
#include <unistd.h>
#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

_syscall2(int, whoami,char*,name,unsigned int,size);

char msg[24] = "root"; //23个字符 +'\0' = 24 ，假设当前用户为 root

int sys_whoami(char* name, unsigned int size)
/***
function:将msg拷贝到name指向的用户地址空间中,确保不会对name越界访存(name的大小由size说明)
return: 拷贝的字符数。如果size小于需要的空间,则返回“­-1”,并置errno为EINVAL。
****/
{
    //msg的长度大于 size
    int len = 0;
    for(;msg[len]!='\0';len++);
    if(len > size)
    {
        return -(EINVAL);
    }
    int i = 0;
    //把msg 输出至 name
    for(i=0; i<size; i++)
    {
        put_fs_byte(msg[i],name+i);
        if(msg[i] == '\0') break; //字符串结束
    }
    return i;
}
