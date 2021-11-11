#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		(C) 1991 Linus Torvalds
 */
/*
* 这个字符串头文件以内嵌函数的形式定义了所有字符串操作函数。使用 gcc 时，同时
* 假定了 ds=es=数据空间，这应该是常规的。绝大多数字符串函数都是经手工进行大量
* 优化的，尤其是函数 strtok、strstr、str[c]spn。它们应该能正常工作，但却不是那
* 么容易理解。所有的操作基本上都是使用寄存器集来完成的，这使得函数即快有整洁。
* 所有地方都使用了字符串指令，这又使得代码“稍微”难以理解?
*
* (C) 1991 Linus Torvalds
*/


/**
 * 将一个字符串(src) 拷贝到另一个字符串(dest)，直到遇到 NULL 字符后停止
 * @param dest - 目标字符串指针
 * @param src - 源字符串指针
 * %0 - esi(src)，%1 - edi(dest)
 * @return
 */
extern inline char * strcpy(char * dest,const char *src)
{
__asm__("cld\n"             // 清方向位
	"1:\tlodsb\n\t"         // 加载 DS:[esi] 处 1 字节 -> al，并更新 esi
	"stosb\n\t"             // 存储字节al->ES:[edi]，并更新edi
	"testb %%al,%%al\n\t"   // 判断刚刚存储的字节是不是 0
	"jne 1b"                // 不是则跳转标号处，否则结束
	::"S" (src),"D" (dest));
return dest;                // 返回目标字符串指针
}


/**
 * 拷贝源字符串(src) 的 count 个字节到目标字符串(desc)
 * 如果源字符串长度小于 count 个字节，就附加空字符 NULL 到目标字符串
 * @param dest - 目标字符串指针
 * @param src - 源字符串指针
 * @param count - 拷贝的字节数
 * %0 - esi(src)，%1 - edi(dest)，%2 - ecx(count)
 * @return
 */
static inline char * strncpy(char * dest,const char *src,int count)
{
__asm__("cld\n"             // 清方向位
	"1:\tdecl %2\n\t"       // 寄存器 ecx-- (count--)
	"js 2f\n\t"             // 如果 count < 0 则跳转到标号 2 处
	"lodsb\n\t"             // 取 ds:[esi] 处 1 字节 -> al,并且 esi++
	"stosb\n\t"             // 存储该字节 -> es:[edi], 并且 edi++
	"testb %%al,%%al\n\t"   // 该字节是 0 ？
	"jne 1b\n\t"            // 不是，则跳转到标号 1 处，继续拷贝
	"rep\n\t"               // 否则，在目标串中存放剩余个数的空字符
	"stosb\n"
	"2:"
	::"S" (src),"D" (dest),"c" (count));
return dest;                // 返回目的字符串指针
}


/**
 * 将源字符串（src）拷贝到目的字符串的末尾处
 * @param dest - 目标字符串指针
 * @param src - 源字符串指针
 * %0 - esi(src)，%1 - edi(dest)，%2 - eax(0)，%3 - ecx(-1)
 * @return
 */
extern inline char * strcat(char * dest,const char * src)
{
__asm__("cld\n\t"           // 清方向位
	"repne\n\t"             // 比较 al 与 es:[edi]字节，并且 edi++
	"scasb\n\t"             // 直到找到目标字符串中 NULL 字符的位置，此时 edi 已经指向后 1 个字节
	"decl %1\n"             // 让 es:[edi] 指向该 NULL 字符的位置，即 edi = edi - 1
	"1:\tlodsb\n\t"         // 取源字符串字节 ds:[esi]->al，并 esi++
	"stosb\n\t"             // 将该字节存到 es:[edi]，并 edi++
	"testb %%al,%%al\n\t"   // 该字节是0？
	"jne 1b"                // 不是，则向后跳转到标号1 处继续拷贝，否则结束
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff));
return dest;                // 返回目的字符串指针
}


/**
 * 将源字符串（src）的 count 个字节拷贝到目标字符串（dest）的末尾处，最后添一空字符 NULL
 * @param dest - 目标字符串指针
 * @param src  - 源字符串指针
 * @param count - 拷贝的字节数
 * %0 - esi(src)，%1 - edi(dest)，%2 - eax(0)，%3 - ecx(-1)，%4 - (count)
 * @return
 */
static inline char * strncat(char * dest,const char * src,int count)
{
__asm__("cld\n\t"           // 清方向位
	"repne\n\t"             // 比较 al 与 es:[edi]字节，并且 edi++
	"scasb\n\t"             // 直到找到目标字符串中 NULL 字符的位置，此时 edi 已经指向后 1 个字节
	"decl %1\n\t"           // 让 es:[edi] 指向该 NULL 字符的位置，即 edi = edi - 1
	"movl %4,%3\n"          // 保存要复制的字节数 ecx
	"1:\tdecl %3\n\t"       // 寄存器 ecx--
	"js 2f\n\t"             // 如果 ecx 为 0 ，跳转至标号 2 处
	"lodsb\n\t"             // 取 ds:[esi] 处的 1 字节->al，esi++
	"stosb\n\t"             // 存储到es:[edi]处，edi++
	"testb %%al,%%al\n\t"   // 该字节值为0？
	"jne 1b\n"              // 不是则向后跳转到标号1 处，继续复制
	"2:\txorl %2,%2\n\t"    // 将 al 清零
	"stosb"                 // 存到 es:[edi] 处
	::"S" (src),"D" (dest),"a" (0),"c" (0xffffffff),"g" (count)
	);
return dest;                // 返回目的字符串指针
}

/**
 * 将一个字符串与另一个字符串进行比较
 * @param cs - 字符串1
 * @param ct - 字符串2
 * %0 - eax(__res)返回值，%1 - edi(csrc)字符串1 指针，%2 - esi(ct)字符串2 指针
 * @return   1: cs > ct
 *           0: cs == ct
 *          -1: cs < ct
 */
extern inline int strcmp(const char * cs,const char * ct)
{
register int __res ;
__asm__("cld\n"             // 清方向位
	"1:\tlodsb\n\t"         // 取字符串2 的字节 ds:[esi] 处的 1 字节 -> al，并且 esi++
	"scasb\n\t"             // al 与字符串1 的字节 es:[edi] 作比较，并且edi++
	"jne 2f\n\t"            // 如果不相等，则向前跳转到标号 2
	"testb %%al,%%al\n\t"   // 该字节是0 值字节吗（字符串结尾）
	"jne 1b\n\t"            // 不是，则向后跳转到标号1，继续比较
	"xorl %%eax,%%eax\n\t"  // 是，则返回值 eax 清零
	"jmp 3f\n"              // 向前跳转到标号3，结束
	"2:\tmovl $1,%%eax\n\t" // eax 中置 1
	"jl 3f\n\t"             // 若前面比较中串2 字符 < 串1 字符，则返回正值，结束
	"negl %%eax\n"          // 否则 eax = -eax，返回负值，结束
	"3:"
	:"=a" (__res):"D" (cs),"S" (ct));
return __res;               // 返回比较结果
}

/**
 * 字符串 1 与字符串 2 的前 count 个字符进行比较
 * @param cs - 字符串1
 * @param ct - 字符串2
 * @param count - 比较的字符数
 * %0 - eax(__res)返回值，%1 - edi(csrc)串1 指针，%2 - esi(ct)串2 指针，%3 - ecx(count)
 * @return   1: cs > ct
 *           0: cs == ct
 *          -1: cs < ct
 */
static inline int strncmp(const char * cs,const char * ct,int count)
{
register int __res ;        // __res 是寄存器变量(eax)
__asm__("cld\n"             // 清方向位
	"1:\tdecl %3\n\t"       // count--
	"js 2f\n\t"             // 如果 count < 0 ，则跳转到标号 2
	"lodsb\n\t"             // 取字符串2 的字节 ds:[esi] 处的 1 字节 -> al，并且 esi++
	"scasb\n\t"             // al 与字符串1 的字节 es:[edi] 作比较，并且edi++
	"jne 3f\n\t"            // 如果不相等，则向前跳转到标号 3
	"testb %%al,%%al\n\t"   // 该字符是NULL 字符吗？
	"jne 1b\n"              // 不是，则向后跳转到标号 1，继续比较
	"2:\txorl %%eax,%%eax\n\t"  // 是NULL 字符，则eax 清零（返回值）
	"jmp 4f\n"              // 向前跳转到标号4，结束
	"3:\tmovl $1,%%eax\n\t" // eax 中置1
	"jl 4f\n\t"             // 如果前面比较中串2 字符 < 串2 字符，则返回 1，结束
	"negl %%eax\n"          // 否则eax = -eax，返回负值，结束
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count));
return __res;               // 返回比较结果
}

/**
 * 在字符串中寻找第一个匹配的字符
 * @param s - 字符串指针
 * @param c - 要匹配的字符
 * %0 - eax(__res)，%1 - esi(字符串指针s)，%2 - eax(字符c)
 * @return 返回字符串中第一次出现匹配字符的指针。若没有找到匹配的字符，则返回空指针
 */
static inline char * strchr(const char * s,char c)
{
register char * __res ;
__asm__("cld\n\t"           // 清方向位
	"movb %%al,%%ah\n"      // 将字符 c 保存到 ah
	"1:\tlodsb\n\t"         // 取字符串中字符 ds:[esi] -> al，并且 esi++
	"cmpb %%ah,%%al\n\t"    // 字符串中字符 al 与指定字符 ah 相比较
	"je 2f\n\t"             // 若相等，则向前跳转到标号 2 处
	"testb %%al,%%al\n\t"   // al 中字符是 NULL 字符吗？（字符串结尾？）
	"jne 1b\n\t"            // 若不是，则向后跳转到标号1，继续比较
	"movl $1,%1\n"          // 是，则说明没有找到匹配字符，esi 置1
	"2:\tmovl %1,%0\n\t"    // 将指向匹配字符后一个字节处的指针值放入 eax
	"decl %0"               // 将指针调整为指向匹配的字符
	:"=a" (__res):"S" (s),"0" (c));
return __res;               // 返回指针
}

/**
 * 寻找字符串中指定字符最后一次出现的地方。（反向搜索字符串）
 * @param s - 字符串指针
 * @param c - 要匹配的字符
 * %0 - edx(__res)，%1 - edx(0)，%2 - esi(字符串指针s)，%3 - eax(字符c)
 * @return 返回字符串中最后一次出现匹配字符的指针。若没有找到匹配的字符，则返回空指针
 */
static inline char * strrchr(const char * s,char c)
{
register char * __res;      // __res 是寄存器变量(edx)
__asm__("cld\n\t"           // 清方向位
	"movb %%al,%%ah\n"      // 将字符 c 保存到 ah
	"1:\tlodsb\n\t"         // 取字符串中字符 ds:[esi] -> al，并且 esi++
	"cmpb %%ah,%%al\n\t"    // 字符串中字符 al 与指定字符 ah 作比较
	"jne 2f\n\t"            // 若不相等，则向前跳转到标号 2 处
	"movl %%esi,%0\n\t"     // 将字符指针保存到 edx 中
	"decl %0\n"             // 指针后退一位，指向字符串中匹配字符处, 即 edx = edx - 1
	"2:\ttestb %%al,%%al\n\t"   // 比较的字符是0 吗（到字符串尾）？
	"jne 1b"                // 不是则向后跳转到标号 1 处，继续比较
	:"=d" (__res):"0" (0),"S" (s),"a" (c));
return __res;               // 返回指针
}

/**
 * 在字符串 1 中寻找第 1 个字符串序列，该字符序列的任何字符都包含在字符串 2 中
 * @param cs - 字符串1 指针
 * @param ct - 字符串2 指针
 * %0 - esi(__res)，%1 - eax(0)，%2 - ecx(-1)，%3 - esi(串1 指针csrc)，%4 - (串2 指针ct)
 * @return - 返回字符串1 中包含字符串2 中任何字符的首个字符序列的长度值
 */
extern inline int strspn(const char * cs, const char * ct)
{
register char * __res;      // __res 是寄存器变量(esi)
__asm__("cld\n\t"           // 清方向位
	"movl %4,%%edi\n\t"     // 字符串2 指针放入edi 中
	"repne\n\t"             // 比较 al (al的值是0) 与 es:[edi]字节，并且 edi++
	"scasb\n\t"             // 直到找到字符串2中 NULL 字符的位置，此时 edi 已经指向后 1 个字节
	"notl %%ecx\n\t"        // ecx 中每位取反
	"decl %%ecx\n\t"        // ecx = ecx - 1 得到了字符串 2 的长度值
	"movl %%ecx,%%edx\n"    // 将串2 的长度值暂放入edx 中
	"1:\tlodsb\n\t"         // 取 串1 字符 ds:[esi] -> al，并且 esi++
	"testb %%al,%%al\n\t"   // 该字符等于0 值吗（串1 结尾）？
	"je 2f\n\t"             // 如果是，则向前跳转到标号2 处
	"movl %4,%%edi\n\t"     // 取串2 头指针放入edi 中
	"movl %%edx,%%ecx\n\t"  // 再将串2 的长度值放入ecx 中
	"repne\n\t"             // 比较al 与串2 中字符 es:[edi]，并且 edi++
	"scasb\n\t"             // 如果不相等就继续比较
	"je 1b\n"               // 如果相等，则向后跳转到标号1 处
	"2:\tdecl %0"           // esi--，指向最后一个包含在串2 中的字符
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	);
return __res-cs;            // 返回字符序列的长度值
}

extern inline int strcspn(const char * cs, const char * ct)
{
register char * __res;
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	);
return __res-cs;
}

extern inline char * strpbrk(const char * cs,const char * ct)
{
register char * __res ;
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n\t"
	"decl %0\n\t"
	"jmp 3f\n"
	"2:\txorl %0,%0\n"
	"3:"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	);
return __res;
}

extern inline char * strstr(const char * cs,const char * ct)
{
register char * __res ;
__asm__("cld\n\t" \
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"	/* NOTE! This also sets Z if searchstring='' */
	"movl %%ecx,%%edx\n"
	"1:\tmovl %4,%%edi\n\t"
	"movl %%esi,%%eax\n\t"
	"movl %%edx,%%ecx\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 2f\n\t"		/* also works for empty string, see above */
	"xchgl %%eax,%%esi\n\t"
	"incl %%esi\n\t"
	"cmpb $0,-1(%%eax)\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"2:"
	:"=a" (__res):"0" (0),"c" (0xffffffff),"S" (cs),"g" (ct)
	);
return __res;
}

extern inline int strlen(const char * s)
{
register int __res ;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff));
return __res;
}

extern char * ___strtok;

extern inline char * strtok(char * s,const char * ct)
{
register char * __res ;
__asm__("testl %1,%1\n\t"
	"jne 1f\n\t"
	"testl %0,%0\n\t"
	"je 8f\n\t"
	"movl %0,%1\n"
	"1:\txorl %0,%0\n\t"
	"movl $-1,%%ecx\n\t"
	"xorl %%eax,%%eax\n\t"
	"cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"je 7f\n\t"			/* empty delimeter-string */
	"movl %%ecx,%%edx\n"
	"2:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 7f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 2b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 7f\n\t"
	"movl %1,%0\n"
	"3:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 5f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 3b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 5f\n\t"
	"movb $0,(%1)\n\t"
	"incl %1\n\t"
	"jmp 6f\n"
	"5:\txorl %1,%1\n"
	"6:\tcmpb $0,(%0)\n\t"
	"jne 7f\n\t"
	"xorl %0,%0\n"
	"7:\ttestl %0,%0\n\t"
	"jne 8f\n\t"
	"movl %0,%1\n"
	"8:"
	:"=b" (__res),"=S" (___strtok)
	:"0" (___strtok),"1" (s),"g" (ct)
	);
return __res;
}

/*
 * Changes by falcon<zhangjinw@gmail.com>, the original return value is static
 * inline ... it can not be called by other functions in another files.
 */

extern inline void * memcpy(void * dest,const void * src, int n)
{
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	);
return dest;
}

extern inline void * memmove(void * dest,const void * src, int n)
{
if (dest<src)
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src),"D" (dest)
	);
else
__asm__("std\n\t"
	"rep\n\t"
	"movsb"
	::"c" (n),"S" (src+n-1),"D" (dest+n-1)
	);
return dest;
}

static inline int memcmp(const void * cs,const void * ct,int count)
{
register int __res ;
__asm__("cld\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"movl $1,%%eax\n\t"
	"jl 1f\n\t"
	"negl %%eax\n"
	"1:"
	:"=a" (__res):"0" (0),"D" (cs),"S" (ct),"c" (count)
	);
return __res;
}

extern inline void * memchr(const void * cs,char c,int count)
{
register void * __res ;
if (!count)
	return NULL;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1f\n\t"
	"movl $1,%0\n"
	"1:\tdecl %0"
	:"=D" (__res):"a" (c),"D" (cs),"c" (count)
	);
return __res;
}

static inline void * memset(void * s,char c,int count)
{
__asm__("cld\n\t"
	"rep\n\t"
	"stosb"
	::"a" (c),"D" (s),"c" (count)
	);
return s;
}

#endif
