#ifndef _ND_MATH_H_
#define _ND_MATH_H_

static inline int div_s64_32(unsigned long long dividend, int divisor)
{
	int result = 0;
	int i = 0;

	if(dividend >> 32 == 0)
		result = (unsigned int)dividend / divisor;
	else{
		while( (divisor * (i++) + divisor -1) < dividend);
		result = i - 1;
	}
		
	return result;
}

#endif
