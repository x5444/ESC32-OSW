#ifndef __FET_H__
#define __FET_H__

#define F_CPU 72000000 //TODO

#define FET_A_L_PORT		GPIOA
#define FET_B_L_PORT		GPIOB
#define FET_C_L_PORT		GPIOB
#define FET_A_H_PORT		GPIOB
#define FET_B_H_PORT		GPIOB
#define FET_C_H_PORT		GPIOB

#define FET_A_L_PIN		GPIO_Pin_7 // TIM3_CH2
#define FET_B_L_PIN		GPIO_Pin_0 // TIM3_CH3
#define FET_C_L_PIN		GPIO_Pin_1 // TIM3_CH4
#define FET_A_H_PIN		GPIO_Pin_6 // TIM4_CH1
#define FET_B_H_PIN		GPIO_Pin_7 // TIM4_CH2
#define FET_C_H_PIN		GPIO_Pin_8 // TIM4_CH3

void fetInit(void);
void fetSetPos(float alpha, float beta);
void fetSetDq(float Id, float Iq);
void fetUpdate(float angle);

#endif // __FET_H__
