#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "test_aes.h"

int main(int argc, const char* argv[])
{
	struct aes_para para;
	char *src = NULL;
	char *dst = NULL;
	char *check = NULL;
	unsigned int len = 1 * 1024 * 1024;
	int i = 0;
	int ret = 0;
	struct timeval tv0, tv1;
	struct timezone tz0, tz1;
	int fd = open("/dev/aes", 0);
	if(fd < 0){
		printf("Failed to open /dev/aes\n");
		return 0;
	}

	src = malloc(len * 3);
	if(!src){
		printf("Failed to malloc buffer!\n");
		goto exit;
	}

	dst = src + len;
	check = dst + len;

	srand((int)(time(0)));
	memset(src, 0, len*3);
	for(i = 0; i < len; i++){
		src[i] =  rand() % 256;
	}

	memset(&para, 0, sizeof(para));
	for(i = 0; i < 4; i++){
		para.aeskey[i] = 0x01020304;
	}

	para.enworkmode = IN_UNF_CIPHER_WORK_MODE_ECB;
	para.src = src;
	para.dst = dst;
	para.datalen = len;

	i = 500;
	while(i--){
	/* encryption */
	gettimeofday(&tv0, &tz0);
	para.status = 0;
	ret = ioctl(fd, IOCTL_AES_START_EN_PROCESSING, &para);
	gettimeofday(&tv1, &tz1);
	printf("index = %d ret = %d, time = %ld us, donelen = %d\n",i, ret, (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec, para.donelen);

	/* decryption */
	para.status = 0;
	para.src = dst;
	para.dst = check;
	gettimeofday(&tv0, &tz0);
	ret = ioctl(fd, IOCTL_AES_START_DE_PROCESSING, &para);
	gettimeofday(&tv1, &tz1);
	printf("index = %d ret = %d, time = %ld us, donelen = %d\n",i, ret, (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec, para.donelen);
	}

	for(i = 0; i < len; i++){
		if(src[i] != check[i]){
			printf("src[%d] = 0x%02x, check[%d] = 0x%02x\n",i, src[i], i, check[i]);
			break;
		}
	}

	free(src);
exit:
	close(fd);
	return 0;
}
