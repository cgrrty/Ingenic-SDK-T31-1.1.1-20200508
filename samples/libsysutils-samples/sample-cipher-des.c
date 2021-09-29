/*
 * Ingenic CIPHER DES test.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sysutils/su_base.h>
#include <sysutils/su_cipher.h>


int main(int argc,char *argv[])
{
	int ret = 0;

	ret = SU_CIPHER_DES_Init();
	if(ret)
	{
		printf("SU_CIPHER_DES_Init error!\n");
		return -1;
	}
	ret = SU_CIPHER_DES_Test();
	if(ret)
	{
		printf("SU_CIPHER_DES_Test error!\n");
		return -1;
	}
	ret = SU_CIPHER_DES_Exit();
	if(ret)
	{
		printf("SU_CIPHER_DES_Exit error!\n");
		return -1;
	}

	return ret;
}
