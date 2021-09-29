#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test_hash.h"

typedef unsigned int UINT4;
typedef unsigned char* POINTER;

static unsigned char PADDING[128] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

typedef struct {
	unsigned char* buffer;		/*plain text */
	unsigned int plain_len;     /*plain text after padding*/
	unsigned int crypt_len;     /*crypt text length*/
	UINT4 count[4];             /*number of bits:md5 sha1 sha224 sha256:2^64  sha384 sha512 2^128*/
	UINT4 state[16];            /*crypt text*/
} HASH_CTX;

static void HASH_memcpy(POINTER output, POINTER input, unsigned int len)
{
	unsigned int i;
	for (i = 0; i < len; i++)
		output[i] = input[i];
}

static void HASH_Encode(unsigned char* output, UINT4* input, unsigned int len,unsigned int mode)
{
	unsigned int i, j;
    unsigned char tmp;
	for (i = 0, j = 0; j < len; i++, j += 4) {
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
	//md5 bit length sequence is reverse
	if(mode != IOCTL_HASH_MD5){
		for(i = 0; i < len/2; i++){
			tmp = output[i];
			output[i] = output[len - i - 1];
			output[len - i - 1] = tmp;
		}
	}
}

void HASHInit(HASH_CTX *context)
{
	memset(context->state, 0 , sizeof(context->state));
	memset(context->count, 0 , sizeof(context->count));
	context->buffer = NULL;
	context->crypt_len = 0;
	context->plain_len = 0;
}

int main(int argc,const char *argv[])
{
    hash_para_t para;
	HASH_CTX hash;
	HASHInit(&hash);
	int i = 0;
	if(argc != 3){
		printf("###############################\n");
		printf("./xxx [Digest mode] [plain text]\n");
		printf("###############################\n");
		return 0;
	}
    unsigned int mode = 0;
	const char *_mode = argv[1];
	if(0 == strcmp(_mode,"md5")){
		hash.crypt_len = 4;
		mode = IOCTL_HASH_MD5;
	}else if(0 == strcmp(_mode,"sha1")){
		hash.crypt_len = 5;
		mode = IOCTL_HASH_SHA1;
	}else if(0 == strcmp(_mode,"sha224")){
		hash.crypt_len = 7;
		mode = IOCTL_HASH_SHA224;
	}else if(0 == strcmp(_mode,"sha256")){
		hash.crypt_len = 8;
		mode = IOCTL_HASH_SHA256;
	}else if(0 == strcmp(_mode,"sha384")){
		hash.crypt_len = 12;
		mode = IOCTL_HASH_SHA384;
	}else if(0 == strcmp(_mode,"sha512")){
		hash.crypt_len = 16;
		mode = IOCTL_HASH_SHA512;
	}else{
		printf("###############################\n");
		printf("[Digest format]:\n\tmd5\n\tsha1\n\tsha224\n\tsha256\n\tsha384\n\tsha512\n");
		printf("###############################\n");
		return 0;
	}
	unsigned char bits_64[8];
	unsigned char bits_128[16];
	unsigned int plainlen = strlen(argv[2]);

	/*number of bits */
	if ((hash.count[0] += ((UINT4)plainlen << 3)) < ((UINT4)plainlen << 3))
		hash.count[1] = ((UINT4)plainlen >> 29);
	//printf("bit length = %d %d\n",hash.count[1],hash.count[0]);

	/*padding bits*/
	unsigned int padlen = 0, index = 0;
	//MD5 SHA1 SHA224 SHA256  512bit * n          64byte *n     bitlength:8bytes
	//SHA384 SHA512           1024bit * n         128byte * n   bitlength:16bytes
	if(mode == IOCTL_HASH_MD5 || mode == IOCTL_HASH_SHA1 || mode == IOCTL_HASH_SHA224 || mode == IOCTL_HASH_SHA256){
		index = (unsigned int)(plainlen & 0x3f);//64byte * n + index
		padlen = (plainlen % 64) < 56 ? (56 - index) : (120 - index);
		HASH_Encode(bits_64, hash.count, 8,mode);
		 hash.plain_len = plainlen + padlen + 8;
	}else if(mode == IOCTL_HASH_SHA384 || mode == IOCTL_HASH_SHA512){
		index = (unsigned int)(plainlen & 0x7F);
		padlen = (plainlen % 128) < 112 ? (112 - index) : (240 - index);
		HASH_Encode(bits_128, hash.count, 16,mode);
		 hash.plain_len = plainlen + padlen + 16;
	}
	//printf("plain_len = %d padlen = %d\n",hash.plain_len,padlen);
	hash.buffer = (unsigned char*)malloc((hash.plain_len) * sizeof(unsigned char));
	memset(hash.buffer,0,sizeof(hash.buffer));

	HASH_memcpy((POINTER)hash.buffer, (POINTER)argv[2], plainlen);
	HASH_memcpy((POINTER)&hash.buffer[plainlen], PADDING, padlen);
	if(mode == IOCTL_HASH_SHA384 || mode == IOCTL_HASH_SHA512)
		HASH_memcpy((POINTER)&hash.buffer[plainlen + padlen], bits_128, 16);
	else
		HASH_memcpy((POINTER)&hash.buffer[plainlen + padlen], bits_64, 8);

    int ret = 0;
    int fd= open("/dev/hash",0);
    if(fd < 0){
        printf("Failed to open /dev/hash\n");
        return 0;
    }
    para.digest_mode = mode;
    para.src = (POINTER)hash.buffer;
    para.dst = (POINTER)hash.state;
	para.plaintext_len = hash.plain_len;
	ret = ioctl(fd,mode,&para);

    for(i = 0; i < hash.crypt_len; i++){
        printf("%x",hash.state[i]);
    }
	printf("\n");
	free(hash.buffer);
    close(fd);
    return 0;
}
