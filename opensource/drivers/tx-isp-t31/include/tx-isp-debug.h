#ifndef _TX_ISP_DEBUG_H_
#define _TX_ISP_DEBUG_H_

/* =================== switchs ================== */

/**
 * default debug level, if just switch ISP_WARNING
 * or ISP_INFO, this not effect DEBUG_REWRITE and
 * DEBUG_TIME_WRITE/READ
 **/
/* =================== print tools ================== */

#define ISP_INFO_LEVEL		0x0
#define ISP_WARNING_LEVEL	0x1
#define ISP_ERROR_LEVEL		0x2
#define ISP_PRINT(level, format, ...)			\
	isp_printf(level, format, ##__VA_ARGS__)
#define ISP_INFO(...) ISP_PRINT(ISP_INFO_LEVEL, __VA_ARGS__)
#define ISP_WARNING(...) ISP_PRINT(ISP_WARNING_LEVEL, __VA_ARGS__)
#define ISP_ERROR(...) ISP_PRINT(ISP_ERROR_LEVEL, __VA_ARGS__)

//extern unsigned int isp_print_level;
/*int isp_debug_init(void);*/
/*int isp_debug_deinit(void);*/
int isp_printf(unsigned int level, unsigned char *fmt, ...);
int get_isp_clk(void);
void *private_vmalloc(unsigned long size);
void private_vfree(const void *addr);
#endif /* _ISP_DEBUG_H_ */
