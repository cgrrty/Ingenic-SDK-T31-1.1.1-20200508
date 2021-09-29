#ifndef _RLE_LOGO_H
#define _RLE_LOGO_H
#include <rle_boot_logo.h>
#define RLE_LOGO_DEFAULT_ADDR  rle_default_logo_addr	//need to fixed!
#if !defined(CONFIG_LCD_INFO_BELOW_LOGO)
#define  BMP_LOGO_HEIGHT  panel_info.vl_row
#define  BMP_LOGO_WIDTH   panel_info.vl_col
#else
#define  BMP_LOGO_HEIGHT  0
#define  BMP_LOGO_WIDTH   0
#endif
#endif
