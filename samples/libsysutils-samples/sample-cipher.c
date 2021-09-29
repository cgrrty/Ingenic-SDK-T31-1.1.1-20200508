/*
 * Ingenic CIPHER SDK test.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co.,Ltd
 * Author: Elvis <huan.wang@ingenic.com>
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


#define TAG "CipherTest"

#define PROCESSING_SIZE		1024*1024

static int cipher_fp = -1;
int fd_ori = -1;
int fd_enc = -1;
int fd_dec = -1;
unsigned int *src = NULL;
unsigned int *dst = NULL;
unsigned int read_size = 0;
unsigned int write_size = 0;
struct timeval tv_begin, tv_end;

static int cipher_fp_02 = -1;
int fd_ori_02 = -1;
int fd_enc_02 = -1;
int fd_dec_02 = -1;
unsigned int *src_02 = NULL;
unsigned int *dst_02 = NULL;
unsigned int read_size_02 = 0;
unsigned int write_size_02 = 0;

IN_UNF_CIPHER_CTRL Ctrl_t = {
	/*KEY: 0x2b7e1516 0x28aed2a6 0xabf71588 0x09cf4f3c*/
	/*IV : 0x00010203 0x04050607 0x08090a0b 0x0c0d0e0f*/
	.key[0] = 0x2b7e1516,
	.key[1] = 0x28aed2a6,
	.key[2] = 0xabf71588,
	.key[3] = 0x09cf4f3c,
	.IV[0]  = 0x00010203,
	.IV[1]  = 0x04050607,
	.IV[2]  = 0x08090a0b,
	.IV[3]  = 0x0c0d0e0f,
	.enAlg  = IN_UNF_CIPHER_ALG_AES,
	.enBitWidth = IN_UNF_CIPHER_BIT_WIDTH_128BIT,
	.enWorkMode = IN_UNF_CIPHER_WORK_MODE_CBC,
	/*.enWorkMode = IN_UNF_CIPHER_WORK_MODE_ECB,*/
	.enKeyLen   = IN_UNF_CIPHER_KEY_AES_128BIT,
	.enDataLen  = PROCESSING_SIZE/* 1M */
};

IN_UNF_CIPHER_CTRL Ctrl_t_02 = {
	/*KEY: 0x2b7e1516 0x28aed2a6 0xabf71588 0x09cf4f3c*/
	/*IV : 0x00010203 0x04050607 0x08090a0b 0x0c0d0e0f*/
	.key[0] = 0x2b7e1516,
	.key[1] = 0x28aed2a6,
	.key[2] = 0xabf71588,
	.key[3] = 0x09cf4f3c,
	.IV[0]  = 0x00010203,
	.IV[1]  = 0x04050607,
	.IV[2]  = 0x08090a0b,
	.IV[3]  = 0x0c0d0e0f,
	.enAlg  = IN_UNF_CIPHER_ALG_AES,
	.enBitWidth = IN_UNF_CIPHER_BIT_WIDTH_128BIT,
	/*.enWorkMode = IN_UNF_CIPHER_WORK_MODE_CBC,*/
	.enWorkMode = IN_UNF_CIPHER_WORK_MODE_ECB,
	.enKeyLen   = IN_UNF_CIPHER_KEY_AES_128BIT,
	.enDataLen  = PROCESSING_SIZE/* 1M */
};

static void test_cipher_encrypt()
{
	int ret = -1;
	fd_ori = open("./cipher-ori", O_RDWR, 0777);
	if (fd_ori < 0) {
		printf( "open cipher-ori failed\n");
		return;
	}
	fd_enc = open("./cipher-enc", O_RDWR | O_CREAT, 0777);
	if (fd_enc < 0) {
		printf( "open cipher-enc failed\n");
		return;
	}

	while((read_size = read(fd_ori, src, PROCESSING_SIZE))) {
		gettimeofday(&tv_begin, NULL);
		ret = SU_CIPHER_Encrypt(cipher_fp, src, dst, read_size);
		if (ret) {
			printf("SU_CIPHER_Encrypt error~\n");
			goto EXIT;
		}
		gettimeofday(&tv_end, NULL);
		printf("EN :Processing data spend (%ld)s(%ld)ms\n", tv_end.tv_sec - tv_begin.tv_sec,
														 (tv_end.tv_usec - tv_begin.tv_usec)/1000);
		if (!(write_size = write(fd_enc, dst, read_size))) {
			printf("write error!\n");
		}
	}
EXIT:
	close(fd_ori);
	close(fd_enc);
}
static void test_cipher_decrypt()
{
	int ret = -1;
	fd_enc = open("./cipher-enc", O_RDWR | O_CREAT, 0777);
	if (fd_enc < 0) {
		printf( "open cipher-enc failed\n");
		return;
	}

	fd_dec = open("./cipher-dec", O_RDWR | O_CREAT, 0777);
	if (fd_dec < 0) {
		printf( "open cipher-dec failed\n");
		return;
	}

	memset(src, 0, PROCESSING_SIZE);
	memset(dst, 0, PROCESSING_SIZE);
	while((read_size = read(fd_enc, src, PROCESSING_SIZE))) {
		gettimeofday(&tv_begin, NULL);
		ret = SU_CIPHER_Decrypt(cipher_fp, src, dst, read_size);
		if (ret) {
			printf("SU_CIPHER_Encrypt error~\n");
			goto EXIT;
		}
		gettimeofday(&tv_end, NULL);
		printf("DE :Processing data spend (%ld)s(%ld)ms\n", tv_end.tv_sec - tv_begin.tv_sec,
														 (tv_end.tv_usec - tv_begin.tv_usec)/1000);
		if (!(write_size = write(fd_dec, dst, read_size))) {
			printf("write error!\n");
		}
	}

EXIT:
	close(fd_enc);
	close(fd_dec);
}
/****************************************************************/
static void test_cipher_encrypt_02()
{
	int ret = -1;
	fd_ori_02 = open("./cipher-ori-02", O_RDWR, 0777);
	if (fd_ori_02 < 0) {
		printf( "open cipher-ori-02 failed\n");
		return;
	}
	fd_enc_02 = open("./cipher-enc-02", O_RDWR | O_CREAT, 0777);
	if (fd_enc_02 < 0) {
		printf( "open cipher-enc failed\n");
		return;
	}

	while((read_size_02 = read(fd_ori_02, src_02, PROCESSING_SIZE))) {
		gettimeofday(&tv_begin, NULL);
		ret = SU_CIPHER_Encrypt(cipher_fp_02, src_02, dst_02, read_size_02);
		if (ret) {
			printf("SU_CIPHER_Encrypt error~\n");
			goto EXIT;
		}
		gettimeofday(&tv_end, NULL);
		printf("EN :Processing 1M data spend (%ld)s(%ld)ms\n", tv_end.tv_sec - tv_begin.tv_sec,
														 (tv_end.tv_usec - tv_begin.tv_usec)/1000);
		if (!(write_size_02 = write(fd_enc_02, dst_02, read_size_02))) {
			printf("write error!\n");
		}
	}

EXIT:
	close(fd_ori_02);
	close(fd_enc_02);
}
static void test_cipher_decrypt_02()
{
	int ret = -1;
	fd_enc_02 = open("./cipher-enc-02", O_RDWR | O_CREAT, 0777);
	if (fd_enc_02 < 0) {
		printf( "open cipher-enc failed\n");
		return;
	}

	fd_dec_02 = open("./cipher-dec-02", O_RDWR | O_CREAT, 0777);
	if (fd_dec_02 < 0) {
		printf( "open cipher-dec failed\n");
		return;
	}

	memset(src_02, 0, PROCESSING_SIZE);
	memset(dst_02, 0, PROCESSING_SIZE);
	while((read_size_02 = read(fd_enc_02, src_02, PROCESSING_SIZE))) {
		gettimeofday(&tv_begin, NULL);
		ret = SU_CIPHER_Decrypt(cipher_fp_02, src_02, dst_02, read_size_02);
		if (ret) {
			printf("SU_CIPHER_Encrypt error~\n");
			goto EXIT;
		}
		gettimeofday(&tv_end, NULL);
		printf("DE :Processing 1M data spend (%ld)s(%ld)ms\n", tv_end.tv_sec - tv_begin.tv_sec,
														 (tv_end.tv_usec - tv_begin.tv_usec)/1000);
		if (!(write_size_02 = write(fd_dec_02, dst_02, read_size_02))) {
			printf("write error!\n");
		}
	}

EXIT:
	close(fd_enc_02);
	close(fd_dec_02);
}
void * thread1_fun(void *arg)
{
	int ret = -1;
	cipher_fp = SU_CIPHER_CreateHandle();
	if (cipher_fp < 0) {
		printf("SU_CIPHER_CreateHandle error~\n");
		goto PT_EXIT;
	}

	ret = SU_CIPHER_ConfigHandle(cipher_fp, &Ctrl_t);
	if (ret) {
		printf("SU_CIPHER_ConfigHandle error~\n");
		goto PT_EXIT;
	}

	test_cipher_encrypt();
	test_cipher_decrypt();

PT_EXIT:
	pthread_exit((void*)1);
}

void * thread2_fun(void *arg)
{
	int ret = -1;
	cipher_fp_02 = SU_CIPHER_CreateHandle();
	if (cipher_fp_02) {
		printf("SU_CIPHER_CreateHandle error~\n");
		goto PT_EXIT;
	}

	ret = SU_CIPHER_ConfigHandle(cipher_fp_02, &Ctrl_t_02);
	if (ret) {
		printf("SU_CIPHER_ConfigHandle error~\n");
		goto PT_EXIT;
	}

	test_cipher_encrypt_02();
	test_cipher_decrypt_02();
PT_EXIT:
	pthread_exit((void*)1);
}

void cipher_encrypt_decrypt_test()
{
	pthread_t pt1, pt2;
	int ret = -1;

	ret = pthread_create(&pt1, NULL, thread1_fun, NULL);
	if (ret != 0)
		printf("can't create pthread pt1\n");
	ret = pthread_create(&pt2, NULL, thread2_fun, NULL);
	if (ret != 0)
		printf("can't create pthread pt2\n");

	ret = pthread_join(pt1, NULL);
	if (ret != 0)
		printf("can't join pthread pt1\n");
	ret = pthread_join(pt2, NULL);
	if (ret != 0)
		printf("can't join pthread pt1\n");

	ret = SU_CIPHER_DestroyHandle(cipher_fp);
	if (ret != 0)
		printf("SU_CIPHER_DestroyHandle [%d] error\n", cipher_fp);
	ret = SU_CIPHER_DestroyHandle(cipher_fp_02);
	if (ret != 0)
		printf("SU_CIPHER_DestroyHandle [%d] error\n", cipher_fp_02);
}

int main(int argc,char *argv[])
{
	int ret;
	pthread_t pt1, pt2;

	src = (unsigned int *)malloc(PROCESSING_SIZE);
	if (src == NULL) {
		printf( "malloc src failed\n");
		goto FREE;
	}
	dst = (unsigned int *)malloc(PROCESSING_SIZE);
	if (dst == NULL) {
		printf( "malloc dst failed\n");
		goto FREE;
	}
	src_02 = (unsigned int *)malloc(PROCESSING_SIZE);
	if (src_02 == NULL) {
		printf( "malloc src failed\n");
		goto FREE;
	}
	dst_02 = (unsigned int *)malloc(PROCESSING_SIZE);
	if (dst_02 == NULL) {
		printf( "malloc dst failed\n");
		goto FREE;
	}
	memset(src, 0, PROCESSING_SIZE);
	memset(dst, 0, PROCESSING_SIZE);
	memset(src_02, 0, PROCESSING_SIZE);
	memset(dst_02, 0, PROCESSING_SIZE);

	ret = SU_CIPHER_Init();
	if (ret < 0) {
		printf( "SU_CIPHER_Init failed\n");
		goto FREE;
	}

	ret = pthread_create(&pt1, NULL, thread1_fun, NULL);
	if (ret != 0)
		printf("can't create pthread pt1\n");
	ret = pthread_create(&pt2, NULL, thread2_fun, NULL);
	if (ret != 0)
		printf("can't create pthread pt2\n");

	ret = pthread_join(pt1, NULL);
	if (ret != 0)
		printf("can't join pthread pt1\n");
	ret = pthread_join(pt2, NULL);
	if (ret != 0)
		printf("can't join pthread pt1\n");

	ret = SU_CIPHER_DestroyHandle(cipher_fp);
	if (ret != 0)
		printf("SU_CIPHER_DestroyHandle [%d] error\n", cipher_fp);
	ret = SU_CIPHER_DestroyHandle(cipher_fp_02);
	if (ret != 0)
		printf("SU_CIPHER_DestroyHandle [%d] error\n", cipher_fp_02);

	cipher_fp = -1;
	cipher_fp_02 = -1;
	ret = SU_CIPHER_Exit();
	if (ret)
		printf("SU_CIPHER_Exit : return %d\n", ret);
FREE:
	free(src);
	free(dst);
	free(src_02);
	free(dst_02);

	return 0;
}

