
#ifdef CONFIG_DWC_DEBUG
#define dwc_debug(fmt, args...)			\
	do {					\
		printf(fmt, ##args);		\
	} while (0)
#else
#define dwc_debug(fmt, args...)			\
	do {					\
	} while (0)
#endif
