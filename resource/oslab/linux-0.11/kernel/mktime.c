/*
 *  linux/kernel/mktime.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <time.h>

/*
 * This isn't the library routine, it is only used in the kernel.
 * as such, we don't care about years<1970 etc, but assume everything
 * is ok. Similarly, TZ etc is happily ignored. We just do everything
 * as easily as possible. Let's find something public for the library
 * routines (although I think minix times is public).
 */
/*
 * PS. I hate whoever though up the year 1970 - couldn't they have gotten
 * a leap-year instead? I also hate Gregorius, pope or no. I'm grumpy.
 */
/**
 * 这不是库函数，它仅供内核使用。因此我们不关系小于 1970 年的年份等，但假定一些均正常。
 * 同样，时间区域 TZ 问题也先忽略。我们只是尽可能简单地处理问题。最好能找到一些公开的库函数
 * （尽管我认为 minix 的时间函数是公开的）。
 *
 * 另外，我恨那个设置 1970 年开始的人 - 难道他们就不能选择从一个闰年开始？
 */
#define MINUTE 60               // 1 分钟的秒数
#define HOUR (60*MINUTE)        // 1 小时的秒数
#define DAY (24*HOUR)           // 1 天的秒数
#define YEAR (365*DAY)          // 1 年的秒数

/* interestingly, we assume leap-years */
/* 有趣的是我们考虑进了闰年 */
// 下面以年为界限，定义了每个月开始的秒数时间
static int month[12] = {
	0,
	DAY*(31),
	DAY*(31+29),
	DAY*(31+29+31),
	DAY*(31+29+31+30),
	DAY*(31+29+31+30+31),
	DAY*(31+29+31+30+31+30),
	DAY*(31+29+31+30+31+30+31),
	DAY*(31+29+31+30+31+30+31+31),
	DAY*(31+29+31+30+31+30+31+31+30),
	DAY*(31+29+31+30+31+30+31+31+30+31),
	DAY*(31+29+31+30+31+30+31+31+30+31+30)
};

/**
 * 该函数计算从 1970 年 1 月 1 日 0 时起开机到当日经过的秒数，作为开始时间。
 * 参数 tm 中各字段已经在 init/main.c 中被赋值，信息取自 CMOS
 * @param tm
 * @return
 */
long kernel_mktime(struct tm * tm)
{
	long res;
	int year;

	// 首先计算 1970 年到现在经过的年数。因为时 2 位表示方式，所以会有 2000 年问题。我们可以
	// 简单在最前面加一条语句来解决这个问题：
	// if (tm->tm_year < 70) tm->tm_year += 100;
	// 由于 UNIX 记年份 y 时从 1970 年算起。到 1972 年就是一个闰年，因此过 3 年 (71, 72, 73)
	// 就是第一个闰年，这样从 1970 年开始的闰年计算方法就应该为 1 + (y - 3) / 4，即为 (y + 1) / 4。
	// res = 这些年经过的秒数之间 + 每个闰年时多一天的秒数时间 + 当年到当月时的秒数。
	// 另外，month[] 数组中一斤挂载 2 月份的天数中包含进了闰年时的天数，即 2 月份天数多算了 1 天。
	// 因此，若当年不是若年且当前月份大于 2 月份的话，我们就要减去这天。因为从 1970 年算起，所以当年
	// 是闰年的判断方法是 (y + 2) 能被 4 除尽。若不能除尽（有余数）就不是闰年。
	year = tm->tm_year - 70;
/* magic offsets (y+1) needed to get leapyears right.*/
/* 为了获得正确的闰年数，这里需要这样一个魔幻值 (y + 1) */
	res = YEAR*year + DAY*((year+1)/4);
	res += month[tm->tm_mon];
/* and (y+2) here. If it wasn't a leap-year, we have to adjust */
/* 以及 (y + 2) ，如果 (y + 2) 不是闰年，那么我们就必须进行跳转（减去一天的秒数时间） */
	if (tm->tm_mon>1 && ((year+2)%4))
		res -= DAY;
	res += DAY*(tm->tm_mday-1);     // 再加上本月过去的天数的秒数时间
	res += HOUR*tm->tm_hour;        // 再加上当前过去的小时的秒数时间
	res += MINUTE*tm->tm_min;       // 再加上 1 小时内过去的分钟数的秒数时间
	res += tm->tm_sec;              // 再加上 1 分钟内已过的时间
	return res;                     // 即等于从 1970 年以来经过的秒数时间
}
