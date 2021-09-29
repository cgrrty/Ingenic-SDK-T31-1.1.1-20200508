#include <sysutils/su_base.h>
#include <stdio.h>
#include <string.h>

int main()
{
	SUDevID dev;

	memset(&dev, 0, sizeof(SUDevID));
	SU_Base_GetDevID(&dev);
	printf("CHIP ID: %s\n", dev.chr);

	return 0;
}
