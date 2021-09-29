#ifndef __AES_TEST_H__
#define __AES_TEST_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define JZAES_IOC_MAGIC  'A'
#define IOCTL_AES_GET_PARA					_IOW(JZAES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_AES_START_EN_PROCESSING		_IOW(JZAES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_AES_START_DE_PROCESSING		_IOW(JZAES_IOC_MAGIC, 112, unsigned int)

typedef enum jz_aes_status {
	JZ_AES_STATUS_PREPARE = 0,
	JZ_AES_STATUS_WORKING,
	JZ_AES_STATUS_DONE,
} JZ_AES_STATUS;

typedef enum IN_UNF_CIPHER_WORK_MODE_E
{
	IN_UNF_CIPHER_WORK_MODE_ECB = 0x0,
	IN_UNF_CIPHER_WORK_MODE_CBC = 0x1,
	IN_UNF_CIPHER_WORK_MODE_OTHER = 0x2
}IN_UNF_CIPHER_WORK_MODE;

struct aes_para {
	unsigned int status;
	unsigned int enworkmode;
	unsigned int aeskey[4];
	unsigned int aesiv[4]; // when work mode is cbc, the parameter must be setted.
	unsigned char *src;
	unsigned char *dst;
	unsigned int datalen; // it should be 16bytes aligned
	unsigned int donelen; // The length of the processed data.
};

#endif
