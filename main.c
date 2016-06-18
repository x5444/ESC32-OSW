#include <stdint.h>
#include <math.h>
#include <stm32f10x.h>
#include <misc.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_tim.h>

#include "res.h"
#include "fet.h"


int _sbrk;
int __errno;

#define F_CPU 72000000


float offset;


int main(void) {

	// Enable Clocks
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);


	resInit();
	fetInit();

	float a = cosf(0) * 0.8f;
	float b = sinf(0) * 0.8f;
	fetSetPos(a, b);

	for(int i=0; i<1000000; i++);

	resCalibrate();

	for(int i=0; i<1000000; i++);

	offset = (360.0f - resGetAngle());

	fetSetDq(0.8f, 0.0f);


	SysTick_Config(F_CPU / 10000);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = SysTick_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);


	while(1){

		//float x = (360.0f - resGetAngle()) - offset;

		//for(int i=0; i<50; i++);

		//x = (x < 0) ? x + 360 : x;
		//a = cosf(2*(3.1415f / 180.0f) * x + 1.5) * 0.8f;
		//b = sinf(2*(3.1415f / 180.0f) * x + 1.5) * 0.8f;

		//fetSetPos(a, b);

	}
}


void SysTick_Handler(void){
	float angle = resGetAngle() - offset;
	fetUpdate(angle);
}



#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  while (1) {
  }
}
#endif
