#include "res.h"
#include <math.h>
#include <misc.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_tim.h>


volatile struct ringBuffer phase1Dat;
volatile struct ringBuffer phase2Dat;

uint16_t sinTab[] = {50, 65, 79, 90, 98,100, 98, 90, 79, 65, 50, 35, 21, 10,  2,  0,  2, 10, 21, 35};


void ringBufferInit(volatile struct ringBuffer *b){
	for(int i=0; i<sizeof(b->data)/sizeof(int16_t); i++){
		b->data[i] = 0;
	}
	b->ptr = 0;
	b->accumulator = 0;
}


void ringBufferUpdate(volatile struct ringBuffer *b, int16_t newVal){
	int16_t oldVal = b->data[b->ptr];
	b->data[b->ptr] = newVal;
	b->ptr += 1;
	b->ptr %= sizeof(b->data)/sizeof(int16_t);

	b->accumulator -= oldVal;
	b->accumulator += newVal;
}



void resInit(void){
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 1;

	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_Cmd(ADC1, ENABLE);

	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1));

	ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = ADC1_2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);


	ringBufferInit(&phase1Dat);
	ringBufferInit(&phase2Dat);

}


float resGetAngle(void){
	int32_t a1 = phase1Dat.accumulator;
	int32_t a2 = phase2Dat.accumulator;

	float res = 0.0f;

	if( a1 > 0 && a2 > 0 ){
		res = 90.0f;
	}else if( a1 > 0 && a2 < 0){
		res = 270.0f;
	}else if( a1 < 0 && a2 > 0){
		res = 90.0f;
	}else if( a1 < 0 && a2 < 0){
		res = 270.0f;
	}else{
		res = 0.0f;
	}

	res += (180.0f / 3.1415f) * atanf( ((float)a1) / ((float)a2) ) - 148.0f;

	res = (res < 0) ? (res + 360.0f) : res;

	return res;
}


void ADC1_2_IRQHandler(void)
{
	static int state = 0;
	static int sinIdx = 0;

	static int16_t phase1y[2] = {0, 0};
	static int16_t phase1x[2] = {0, 0};

	static int16_t phase2y[2] = {0, 0};
	static int16_t phase2x[2] = {0, 0};

	uint16_t adcVal = (uint16_t)ADC_GetConversionValue(ADC1);
	uint32_t corr;
	int cmpIdx;

	switch(state){
		case 0:
			phase1x[1] = phase1x[0];
			phase1x[0] = (int16_t)adcVal;
			phase1y[1] = phase1y[0];
			phase1y[0] = (phase1y[1] >> 2) + (phase1y[1] >> 1) + phase1x[0] - phase1x[1];

			cmpIdx = (sinIdx - 7);
			cmpIdx = (cmpIdx < 0) ? cmpIdx + sizeof(sinTab)/sizeof(uint16_t) : cmpIdx;
			corr = (((int32_t)sinTab[cmpIdx])-50) * phase1y[0];

			ringBufferUpdate(&phase1Dat, corr);
			ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_28Cycles5);

			state = 1;
			break;

		case 1:
			phase2x[1] = phase2x[0];
			phase2x[0] = (int16_t)adcVal;
			phase2y[1] = phase2y[0];
			phase2y[0] = (phase2y[1] >> 2) + (phase2y[1] >> 1) + phase2x[0] - phase2x[1];

			cmpIdx = (sinIdx - 7);
			cmpIdx = (cmpIdx < 0) ? cmpIdx + sizeof(sinTab)/sizeof(uint16_t) : cmpIdx;
			corr = (((int32_t)sinTab[cmpIdx])-50) * phase2y[0];

			sinIdx++;
			sinIdx %= sizeof(sinTab)/sizeof(uint16_t);
			TIM_SetCompare1(TIM1, sinTab[sinIdx++]);

			ringBufferUpdate(&phase2Dat, corr);
			ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5);

			state = 0;
			break;

		default:
			break;
	}

	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}
