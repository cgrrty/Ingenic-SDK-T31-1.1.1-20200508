#ifndef __DES_TEST_H__
#define __DES_TEST_H__

#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define JZDES_IOC_MAGIC  'D'
#define IOCTL_DES_GET_PARA		        _IOW(JZDES_IOC_MAGIC, 110, unsigned int)
#define IOCTL_DES_START_EN_PROCESSING		_IOW(JZDES_IOC_MAGIC, 111, unsigned int)
#define IOCTL_DES_START_DE_PROCESSING		_IOW(JZDES_IOC_MAGIC, 112, unsigned int)


#define miscdev_to_desops(mdev) (container_of(mdev, struct des_operation, des_dev))

typedef enum IN_UNF_CIPHER_WORK_MODE_E
{
	IN_UNF_CIPHER_WORK_MODE_ECB = 0x0,
	IN_UNF_CIPHER_WORK_MODE_CBC = 0x1,
	IN_UNF_CIPHER_WORK_MODE_OTHER = 0x2
}IN_UNF_CIPHER_WORK_MODE;

typedef enum JZ_DES_ALGORITHME_E{
	JZ_DES_ALGORITHME_SDES = 0,
	JZ_DES_ALGORITHME_TDES = 1,
	JZ_DES_ALGORITHME_OTHER = 2
}JZ_DES_ALGORITHME;

struct des_para {
	unsigned int status;//WORK/STOP
	unsigned int enworkmode;//ECB/CBC
	unsigned int algorithms;//SDES/TDES
	unsigned int desiv[2]; //initial vector,cbc:must be setted.
	unsigned int deskeys[6];//SDES:1 TDES:3
	unsigned int datalen; // it should be 8byte aligned
	unsigned char *src;
	unsigned char *dst;
    unsigned int donelen; // The length of the processed data.
};

#endif
