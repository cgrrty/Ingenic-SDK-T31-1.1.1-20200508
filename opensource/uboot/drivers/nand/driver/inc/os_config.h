#include <config.h>

#ifdef CONFIG_JZ4780
#define CONFIG_SOC_4780
#elif defined(CONFIG_JZ4775)
#define CONFIG_SOC_4775
#elif defined(CONFIG_M150)
#define CONFIG_SOC_M150
#elif defined(CONFIG_M200)
#define CONFIG_SOC_M200
#elif defined(CONFIG_T15)
#define CONFIG_SOC_T15
#elif defined(CONFIG_T10)
#define CONFIG_SOC_T10
#endif
