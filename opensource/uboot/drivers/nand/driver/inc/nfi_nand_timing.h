#ifndef _NFI_NAND_TIMING_H_
#define _NFI_NAND_TIMING_H_
typedef struct __onfi_toggle_timing {
        unsigned int tRPRE;	/* ... duration/width/time */
        unsigned int tDS;	/* ... duration/width/time */
        unsigned int tWPRE;	/* ... duration/width/time */
        unsigned int tDQSRE;	/* ... duration/width/time */
        unsigned int tWPST;	/* ... duration/width/time */
} nfi_toggle_timing;

typedef struct __nfi_onfi_timing {
        unsigned int tDQSD;	/* ... duration/width/time */
        unsigned int tDQSS;	/* ... duration/width/time */
        unsigned int tCK;	/* ... duration/width/time */
        unsigned int tDQSHZ;	/* ... duration/width/time */
        unsigned int tDQSCK;	/* ... duration/width/time */
} nfi_onfi_timing;

#endif /* _NFI_NAND_TIMING_H_ */
