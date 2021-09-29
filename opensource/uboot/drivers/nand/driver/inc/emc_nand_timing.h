#ifndef _EMC_NAND_TIMING_H_
#define _EMC_NAND_TIMING_H_
typedef struct __emc_toggle_timing {
        unsigned int tRPRE;	/* ... duration/width/time */
        unsigned int tDS;	/* ... duration/width/time */
        unsigned int tWPRE;	/* ... duration/width/time */
        unsigned int tDQSRE;	/* ... duration/width/time */
        unsigned int tWPST;	/* ... duration/width/time */
} emc_toggle_timing;


#endif /* _EMC_NAND_TIMING_H_ */
