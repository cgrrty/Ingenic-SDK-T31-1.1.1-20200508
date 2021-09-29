/**
 * nanddebug.c
 **/
#include <stdarg.h>
//#include <stdio.h>
//#include <stdarg.h>
enum {
	DUG_LEVEL_INFO = 1,
	DUG_LEVEL_WARNING,
	DUG_LEVEL_DEBUG,
	DUG_LEVEL_ERROR,
	DUG_LEVEL_SYSINFO,
};

int nm_dbg_level = DUG_LEVEL_WARNING;
int utils_dbg_level = DUG_LEVEL_WARNING;
int libops_dbg_level = DUG_LEVEL_WARNING;

extern int vsprintf(char *str,char *fmt,va_list ap);
extern void serial_putc (const char c);

char buf[256];
int __ndprint(char *fmt, ...)
{
	char *p = buf;
	va_list ap;

	va_start(ap, fmt);
	vsprintf(p, fmt, ap);
	va_end(ap);
	while(*p)
		serial_putc(*p++);

	return 0;
}

void nd_dump_stack(void) {
}
