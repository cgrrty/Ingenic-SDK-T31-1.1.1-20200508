#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "test_des.h"
int main(int argc, const char* argv[])
{
	struct des_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 1 * 1024 * 1024;
	int i = 0,j = 0;
	int ret = 0;
	struct timeval tv0, tv1;
	struct timezone tz0, tz1;
	int fd = open("/dev/des", 0);
	if(fd < 0)
        {
		printf("Failed to open /dev/des\n");
		return 0;
	}
	src = (char*)malloc(len * 3 * sizeof(char));
	if(!src)
        {
		printf("Failed to malloc buffer!\n");
		goto exit;
	}
	dst = src + len;
	check = dst + len;
	srand((int)(time(0)));
	memset(src, 0, len * 3);

	for(i = 0; i < len; i++)
        {
		src[i] =  rand();
	}
	memset(&para, 0, sizeof(para));

	for(i = 0; i < 6; i++)
        {
		para.deskeys[i] = 0x01020304;
	}
        for(i = 0; i < 2; i++)
        {
	        para.desiv[i] = 0x12345678;
	}
	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_ECB;
	para.src = (char *)src;
	para.dst = (char *)dst;
	para.datalen = len * sizeof(char);
        para.algorithms = JZ_DES_ALGORITHME_TDES;
	i = 1000;
	while(i--)
	{
                printf("No %d.\n",i);
		/* encryption */
		gettimeofday(&tv0, &tz0);
		ret = ioctl(fd, IOCTL_DES_START_EN_PROCESSING, &para);
		gettimeofday(&tv1, &tz1);
		printf("Encrypt ret = %d, time = %ld us, donelen = %d\n", ret, (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec, para.donelen);
		/* decryption */
		para.src = (char*)dst;
		para.dst = (char*)check;
		gettimeofday(&tv0, &tz0);
		ret = ioctl(fd, IOCTL_DES_START_DE_PROCESSING, &para);
		gettimeofday(&tv1, &tz1);
	        printf("Decrypt ret = %d, time = %ld us, donelen = %d\n", ret, (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec, para.donelen);
                #if 0
                for(j = 0; j < len; j++)
                {
                        printf("src[%d] = %d, dst[%d] = %d, check[%d] = %d\n",j, src[j], j, dst[j], j, check[j]);
                }
                #endif
                for(j = 0; j < len; j++)
                {
                        if(src[j] != check[j])
                        {
                                printf("src[%d] = 0x%02x, check[%d] = 0x%02x\n",j, src[j], j, check[j]);
                                printf("----------DES fail-----------\n");
                                break;
                        }
                }
	}

	free(src);
exit:
	close(fd);
	return 0;
}
