#ifndef __RES_H__
#define __RES_H__

#include <stdint.h>

#define F_CPU 72000000

struct ringBuffer{
	int16_t data[100];
	uint8_t ptr;
	int32_t accumulator;
};

void resInit(void);
float resGetAngle(void);

#endif //__RES_H__
