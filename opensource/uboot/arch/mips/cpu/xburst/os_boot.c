#include <config.h>
#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/arch/ost.h>

int spl_start_uboot(void)
{
	printf("mach spl_start_uboot ? no\n");
	return 0;
}

void spl_board_prepare_for_linux(void)
{
#ifdef CONFIG_PALLADIUM
	printf("mach spl_board_prepare_for_linux\n");
	void (*uboot)(void);
	uboot = (void (*)(void))0x80020000;
	(*uboot)();
#endif
}

int cleanup_before_linux (void)
{
	printf("mach cleanup_before_linux\n");
}
