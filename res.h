#ifndef __RES_H__
#define __RES_H__

#include <stdint.h>

#define F_CPU 72000000

void resInit(void);
float resGetAngle(void);
void resCalibrate(void);

#endif //__RES_H__
