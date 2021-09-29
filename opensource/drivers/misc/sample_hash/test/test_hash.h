#ifndef __JZ_HASH_H__
#define __JZ_HASH_H__

#define JZHASH_IOC_MAGIC                    'H'
#define IOCTL_HASH_GET_PARA                 _IOW(JZHASH_IOC_MAGIC, 110, unsigned int)
#define IOCTL_HASH_MD5                      _IOW(JZHASH_IOC_MAGIC, 111, unsigned int)
#define IOCTL_HASH_SHA1                     _IOW(JZHASH_IOC_MAGIC, 112, unsigned int)
#define IOCTL_HASH_SHA224                   _IOW(JZHASH_IOC_MAGIC, 113, unsigned int)
#define IOCTL_HASH_SHA256                   _IOW(JZHASH_IOC_MAGIC, 114, unsigned int)
#define IOCTL_HASH_SHA384                   _IOW(JZHASH_IOC_MAGIC, 115, unsigned int)
#define IOCTL_HASH_SHA512                   _IOW(JZHASH_IOC_MAGIC, 116, unsigned int)

typedef enum {
    JZ_HASH_DIGEST_MODE_MD5     = 0x0,
    JZ_HASH_DIGEST_MODE_SHA_1   = 0x1,
    JZ_HASH_DIGEST_MODE_SHA_224 = 0x2,
    JZ_HASH_DIGEST_MODE_SHA_256 = 0x3,
    JZ_HASH_DIGEST_MODE_SHA_384 = 0x4,
    JZ_HASH_DIGEST_MODE_SHA_512 = 0x5
}JZ_HASH_DIGEST_MODE_E;

typedef struct hash_para{
    unsigned int digest_mode;
    unsigned char *src;
    unsigned char *dst;
    unsigned int plaintext_len;
    unsigned int crypttext_len;
}hash_para_t;



#endif
