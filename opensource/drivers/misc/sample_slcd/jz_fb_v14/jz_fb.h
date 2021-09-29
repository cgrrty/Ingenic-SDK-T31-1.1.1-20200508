#include <linux/fb.h>

#define NUM_FRAME_BUFFERS 3

#ifdef CONFIG_TWO_FRAME_BUFFERS
#undef NUM_FRAME_BUFFERS
#define NUM_FRAME_BUFFERS 2
#endif

#ifdef CONFIG_THREE_FRAME_BUFFERS
#undef NUM_FRAME_BUFFERS
#define NUM_FRAME_BUFFERS 3
#endif
#define PIXEL_ALIGN 4
#define MAX_DESC_NUM 3

/**
 * @next: physical address of next frame descriptor
 * @databuf: physical address of buffer
 * @id: frame ID
 * @cmd: DMA command and buffer length(in word)
 * @offsize: DMA off size, in word
 * @page_width: DMA page width, in word
 * @cpos: smart LCD mode is commands' number, other is bpp,
 * premulti and position of foreground 0, 1
 * @desc_size: alpha and size of foreground 0, 1
 */

struct dpu_dmmu_map_info {
	unsigned int addr;
	unsigned int len;
};

struct jzfb_framedesc {
	unsigned int NextDesAddr;
	unsigned int BufferAddr_RGB_Y;
	unsigned int Stride_RGB_Y;
	unsigned int ChainCfg;
	unsigned int InterruptCtrl;
	unsigned int BufferAddr_UV;
	unsigned int Stride_UV;
};

struct jzfb_frm_desc {
	unsigned int stride_rgb_y;
	unsigned int chain_cfg;
	unsigned int int_frm_end;
	unsigned int stride_uv;
	unsigned int select_nv12;
};

enum jzfb_format_order {
	FORMAT_X8R8G8B8 = 1,
	FORMAT_X8B8G8R8,
};

/**
 * @fg: foreground 0 or foreground 1
 * @bpp: foreground bpp
 * @x: foreground start position x
 * @y: foreground start position y
 * @w: foreground width
 * @h: foreground height
 */
struct jzfb_layer_t {
	u32 fg;
	u32 bpp;
	u32 x;
	u32 y;
	u32 w;
	u32 h;
};

/**
 *@decompress: enable decompress function, used by FG0
 *@block: enable 16x16 block function
 *@fg0: fg0 info
 *@fg1: fg1 info
 */
struct jzfb_osd_t {
	struct jzfb_layer_t layer[4];
};

struct jzfb {
	int is_lcd_en;		/* 0, disable  1, enable */
	int is_frm_end;		/* 1, frame end */
	int is_clk_en;		/* 0, disable  1, enable */
	int irq;		/* lcdc interrupt num */
	int open_cnt;
	int irq_cnt;
	int desc_num;
	char clk_name[16];
	char pclk_name[16];
	char pwcl_name[16];
	char irq_name[16];

	struct fb_info *fb;
	struct device *dev;
	struct jzfb_platform_data *pdata;
	void __iomem *base;
	struct resource *mem;

	size_t vidmem_size;
	void *vidmem[NUM_FRAME_BUFFERS];
	dma_addr_t vidmem_rgb_y_phys[NUM_FRAME_BUFFERS];
	dma_addr_t vidmem_uv_phys[NUM_FRAME_BUFFERS];

	int frm_size;
	int current_buffer;
	/* dma descriptor base address */
	struct jzfb_framedesc *framedesc[MAX_DESC_NUM];
	dma_addr_t framedesc_phys[NUM_FRAME_BUFFERS];

	wait_queue_head_t vsync_wq;
    struct semaphore dis_lock;
	unsigned int vsync_skip_map;	/* 10 bits width */
	int vsync_skip_ratio;

#define TIMESTAMP_CAP	16
	struct {
		volatile int wp; /* write position */
		int rp;	/* read position */
		u64 value[TIMESTAMP_CAP];
	} timestamp;

	struct mutex lock;
	struct mutex suspend_lock;

	enum jzfb_format_order fmt_order;	/* frame buffer pixel format order */
	struct jzfb_osd_t osd;	/* osd's config information */

	struct clk *clk;
	struct clk *pclk;
	struct clk *pwcl;
	struct clk *ahb;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int is_suspend;
	unsigned int pan_display_count;
	int blank;
	unsigned int pseudo_palette[16];
};

void jzfb_clk_enable(struct jzfb *jzfb);
void jzfb_clk_disable(struct jzfb *jzfb);
static inline unsigned long reg_read(struct jzfb *jzfb, int offset)
{
	return readl(jzfb->base + offset);
}

static inline void reg_write(struct jzfb *jzfb, int offset, unsigned long val)
{
	writel(val, jzfb->base + offset);
}

#define JZFB_SET_FRAME_CONFIG		_IOWR('F', 0x600, struct jzfb_framedesc **)

#define JZFB_GET_FRAME_CONFIG		_IOR('F', 0x610, struct jzfb_framedesc **)
#define JZFB_GET_MODE				_IOR('F', 0x611, struct fb_videomode)
#define JZFB_GET_FRM_END			_IOR('F', 0x613, struct jzfb_framedesc)

#define JZFB_ENABLE					_IOW('F', 0x620, int)
#define JZFB_DISABLE				_IOW('F', 0x621, int)
#define JZFB_SLCD_NEXT_FRM			_IOW('F', 0x623, int)

/* define in image_enh.c */
extern int jzfb_config_image_enh(struct fb_info *info);
extern int jzfb_image_enh_ioctl(struct fb_info *info, unsigned int cmd,
				unsigned long arg);
extern int update_slcd_frame_buffer(void);
extern int lcd_display_inited_by_uboot(void);
