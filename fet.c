#include "fet.h"
#include <math.h>
#include <misc.h>
#include <stm32f10x_dbgmcu.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_tim.h>



const BitAction FET_A_H_States[] = {1, 1, 0, 0, 0, 1};
const BitAction FET_B_H_States[] = {0, 1, 1, 1, 0, 0};
const BitAction FET_C_H_States[] = {0, 0, 0, 1, 1, 1};
const BitAction FET_A_L_States[] = {0, 0, 1, 1, 1, 0};
const BitAction FET_B_L_States[] = {1, 0, 0, 0, 1, 1};
const BitAction FET_C_L_States[] = {1, 1, 1, 0, 0, 0};

float fetD = 0;
float fetQ = 0;



void fetUpdate(float angle){
	angle = (3.1415f/180.0f) * angle;
	float cosAngle = cosf( angle );
	float sinAngle = sinf( angle );

	// Ia | = | cos -sin | * Id |
	// Ib |   | sin  cos |   Iq |
	float alpha = cosAngle * fetD - sinAngle * fetQ;
	float beta  = sinAngle * fetD + cosAngle * fetQ;

	fetSetPos(alpha, beta);
};



void fetSetDq(float Id, float Iq){
	fetD = Id;
	fetQ = Iq;
}



void fetSetPos(float alpha, float beta){

	// Convert the vector (alpha, beta) into length len and angle omega
	float omega = (180.0f/3.1415f) * atan2f(beta, alpha);
	omega = (omega < 0) ? omega + 360 : omega;
	omega = (omega >= 360) ? omega - 360 : omega;
	float len = sqrtf( alpha*alpha + beta*beta );

	// Determine the vectors and their duration
	int baseVect = (int)(omega / 60.0f);
	omega -= 60.0f * baseVect;
	float fraction = omega / 60.0f;

	// Calculate Switching Times
	int timePeriod = TIM3->ARR;
	int timeSliceMotorOn = (int)((float)timePeriod * len);
	int timeSliceMotorOffHalf = (timePeriod - timeSliceMotorOn) / 2;
	int timeSliceFirstOn = (int)((1.0f - fraction) * (float)timeSliceMotorOn);
	int timeSliceSecondOn = (int)(fraction * (float)timeSliceMotorOn);

	// Swap the vectors to always have the first vector having two high side fets on
	int v1, v2;
	int v1len = FET_A_H_States[baseVect]       + FET_B_H_States[baseVect]       + FET_C_H_States[baseVect];
	int v2len = FET_A_H_States[(baseVect+1)%6] + FET_B_H_States[(baseVect+1)%6] + FET_C_H_States[(baseVect+1)%6];
	if(v1len < v2len){
		v1 = baseVect;
		v2 = (baseVect + 1) % 6;
	}else{
		v1 = (baseVect + 1) % 6;
		v2 = baseVect;

		int buf = timeSliceSecondOn;
		timeSliceSecondOn = timeSliceFirstOn;
		timeSliceFirstOn = buf;
	}

	// Calculate actual timer values
	int cmpA = 0;
	int cmpB = 0;
	int cmpC = 0;

	if(FET_A_H_States[v1] == 1){
		cmpA = timeSliceMotorOffHalf;
	}else if(FET_B_H_States[v1] == 1){
		cmpB = timeSliceMotorOffHalf;
	}else if(FET_C_H_States[v1] == 1){
		cmpC = timeSliceMotorOffHalf;
	}

	if(FET_A_H_States[v1] == 0 && FET_A_H_States[v2] == 1){
		cmpA = timeSliceMotorOffHalf + timeSliceFirstOn;
	}else if(FET_B_H_States[v1] == 0 && FET_B_H_States[v2] == 1){
		cmpB = timeSliceMotorOffHalf + timeSliceFirstOn;
	}else if(FET_C_H_States[v1] == 0 && FET_C_H_States[v2] == 1){
		cmpC = timeSliceMotorOffHalf + timeSliceFirstOn;
	}

	if(FET_A_H_States[v2] == 0){
		cmpA = timeSliceMotorOffHalf + timeSliceFirstOn + timeSliceSecondOn;
	}else if(FET_B_H_States[v2] == 0){
		cmpB = timeSliceMotorOffHalf + timeSliceFirstOn + timeSliceSecondOn;
	}else if(FET_C_H_States[v2] == 0){
		cmpC = timeSliceMotorOffHalf + timeSliceFirstOn + timeSliceSecondOn;
	}

	__disable_irq();

	TIM_SetCompare2(TIM3, cmpA);
	TIM_SetCompare1(TIM4, cmpA);

	TIM_SetCompare3(TIM3, cmpB);
	TIM_SetCompare2(TIM4, cmpB);

	TIM_SetCompare4(TIM3, cmpC);
	TIM_SetCompare3(TIM4, cmpC);

	__enable_irq();

}



void fetInit(void){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = FET_A_L_PIN;
	GPIO_Init(FET_A_L_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_A_L_PORT, FET_A_L_PIN, 0);

	GPIO_InitStructure.GPIO_Pin = FET_B_L_PIN;
	GPIO_Init(FET_B_L_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_B_L_PORT, FET_B_L_PIN, 0);

	GPIO_InitStructure.GPIO_Pin = FET_C_L_PIN;
	GPIO_Init(FET_C_L_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_C_L_PORT, FET_C_L_PIN, 0);

	GPIO_InitStructure.GPIO_Pin = FET_A_H_PIN;
	GPIO_Init(FET_A_H_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_A_H_PORT, FET_A_H_PIN, 0);

	GPIO_InitStructure.GPIO_Pin = FET_B_H_PIN;
	GPIO_Init(FET_B_H_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_B_H_PORT, FET_B_H_PIN, 0);

	GPIO_InitStructure.GPIO_Pin = FET_C_H_PIN;
	GPIO_Init(FET_C_H_PORT, &GPIO_InitStructure);
	GPIO_WriteBit(FET_C_H_PORT, FET_C_H_PIN, 0);

	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_Period = (uint16_t)(F_CPU / 2000)-1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned1;

	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);

	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;

	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC2Init(TIM3, &TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OC1Init(TIM4, &TIM_OCInitStructure);

	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC3Init(TIM3, &TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OC2Init(TIM4, &TIM_OCInitStructure);

	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC4Init(TIM3, &TIM_OCInitStructure);
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Set;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
	TIM_OC3Init(TIM4, &TIM_OCInitStructure);

	TIM_SelectMasterSlaveMode(TIM3, TIM_MasterSlaveMode_Enable);
	TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Enable);
	TIM_SelectSlaveMode(TIM4, TIM_SlaveMode_Gated);
	TIM_SelectInputTrigger(TIM4, TIM_TS_ITR2);

	DBGMCU_Config(DBGMCU_TIM3_STOP, DISABLE);
	DBGMCU_Config(DBGMCU_TIM4_STOP, DISABLE);

	TIM_Cmd(TIM4, ENABLE);
	TIM_Cmd(TIM3, ENABLE);

	TIM_SetCompare2(TIM3, 1000);
	TIM_SetCompare1(TIM4, 1000);

	TIM_SetCompare3(TIM3, 20000);
	TIM_SetCompare2(TIM4, 20000);

	TIM_SetCompare4(TIM3, 19000);
	TIM_SetCompare3(TIM4, 19000);

	TIM_GenerateEvent(TIM3, TIM_EventSource_COM);
}
