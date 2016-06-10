#include <stdint.h>
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

uint16_t test1 = 123;
int test2 = 321;

#define FET_A_L_PORT		GPIOA
#define FET_B_L_PORT		GPIOB
#define FET_C_L_PORT		GPIOB
#define FET_A_H_PORT		GPIOB
#define FET_B_H_PORT		GPIOB
#define FET_C_H_PORT		GPIOB

#define FET_A_L_PIN		GPIO_Pin_7
#define FET_B_L_PIN		GPIO_Pin_0
#define FET_C_L_PIN		GPIO_Pin_1
#define FET_A_H_PIN		GPIO_Pin_6
#define FET_B_H_PIN		GPIO_Pin_7
#define FET_C_H_PIN		GPIO_Pin_8


int main(void) {

	test1 ++;
	test2 ++;
	test2 = test1;

	// Enable Clocks
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	// Setup Timer
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef TIM_OCInitStructure;

	//uint16_t PrescalerValue = (uint16_t) (72000000 / 24000000) - 1;
	TIM_TimeBaseStructure.TIM_Period = 100;
	TIM_TimeBaseStructure.TIM_Prescaler = 1;//PrescalerValue;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_Pulse = 0;

	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;

	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Set;

	TIM_OC1Init(TIM1, &TIM_OCInitStructure);

	TIM_CCPreloadControl(TIM1, ENABLE);
	TIM_ARRPreloadConfig(TIM1, ENABLE);

	TIM_Cmd(TIM1, ENABLE);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);
	TIM_GenerateEvent(TIM1, TIM_EventSource_COM);

	for(int i=0; i<1000000; i++);

	resInit();
	fetInit();

	for(int i=0; i<1000000; i++);

	float a = cosf(0) * 0.8f;
	float b = sinf(0) * 0.8f;
	fetSetPos(a, b);

	for(int i=0; i<1000000; i++);
	float offset = (360.0f - resGetAngle());

	float phi = 0;
	int i = 0;
	while(1){
		for(int i=0; i<50000; i++);
		float x = (360.0f - resGetAngle()) - offset;
		x = (x < 0) ? x + 360 : x;
		a = cosf(2 * (3.1415f / 180.0f) * x + 1.5f) * 0.8f;
		b = sinf(2 * (3.1415f / 180.0f) * x + 1.5f) * 0.8f;
		fetSetPos(a, b);
		//setVect(i%100, 100-(i%100));
		i++;
		phi += 0.1;

		int y = TIM3->CNT;
		int z = TIM4->CNT;
	}

}



#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line) {
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  while (1) {
  }
}
#endif
