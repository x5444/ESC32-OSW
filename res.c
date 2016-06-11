#include "res.h"
#include <math.h>
#include <misc.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_dma.h>


volatile struct ringBuffer phase1Dat;
volatile struct ringBuffer phase2Dat;

// x = linspace(0,2*pi, 101);
// y = round(0.5*(71000/500)*(1 + sin(x(1:end-1))));
// str=''; for i=1:numel(y), str=[str ', ' num2str(y(i))]; end, disp(str);
static const uint32_t sinTab[] = {71, 75, 80, 84, 89, 93, 97, 101, 105, 109, 113, 116, 120, 123, 126, 128, 131, 133, 135, 137, 139, 140, 141, 141, 142, 142, 142, 141, 141, 140, 139, 137, 135, 133, 131, 128, 126, 123, 120, 116, 113, 109, 105, 101, 97, 93, 89, 84, 80, 75, 71, 67, 62, 58, 53, 49, 45, 41, 37, 33, 29, 26, 22, 19, 16, 14, 11, 9, 7, 5, 3, 2, 1, 1, 0, 0, 0, 1, 1, 2, 3, 5, 7, 9, 11, 14, 16, 19, 22, 26, 29, 33, 37, 41, 45, 49, 53, 58, 62, 67};

uint32_t adcBuffer[1024];


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

	// Clock Config
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);


	// Configure GPIO
	GPIO_InitTypeDef GPIO_InitStructure;

	// - Analog Inputs ADC Ch1 and Ch2
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_Init(GPIOA, &GPIO_InitStructure);


	// - TIM2 Channel 3 PWM Output
	GPIO_PinRemapConfig(GPIO_PartialRemap2_TIM2, ENABLE);
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOB, &GPIO_InitStructure);


	// Configure Timer
	// - TIM2 Channel 3 PWM Output
	// - TIM2 Channel 2 to trigger the ADC
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_Period = (uint16_t)(F_CPU / 500000)-1; // 500kHz
	TIM_TimeBaseInitStructure.TIM_Prescaler = 0;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = TIM2->ARR / 2;
	TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC3Init(TIM2, &TIM_OCInitStructure);

	TIM_OC2Init(TIM2, &TIM_OCInitStructure);

	TIM_DMACmd(TIM2, TIM_DMA_Update, ENABLE);

	TIM_Cmd(TIM2, ENABLE);


	// Configure DMA
	// - Circularly copy entries from sinTab to Timer2 Update using DMA1 Ch2
	DMA_InitTypeDef DMA_InitStructure;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(TIM2->CCR3);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&sinTab;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
	DMA_InitStructure.DMA_BufferSize = sizeof(sinTab)/sizeof(uint32_t);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel2, &DMA_InitStructure);
	DMA_Cmd(DMA1_Channel2, ENABLE);


	// Configure ADC
	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = 2;
	ADC_Init(ADC1, &ADC_InitStructure);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_1Cycles5);

	ADC_DMACmd(ADC1, ENABLE);

	ADC_Cmd(ADC1, ENABLE);

	ADC_ExternalTrigConvCmd(ADC1, ENABLE);

	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1));


	// Configure DMA
	// - Circularly copy entries from ADC1 to the adcBuffer
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(ADC1->DR);
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)&adcBuffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize = sizeof(adcBuffer)/sizeof(uint32_t);
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
	DMA_Init(DMA1_Channel1, &DMA_InitStructure);

	DMA_ITConfig(DMA1_Channel1, DMA_IT_HT, ENABLE);

	DMA_Cmd(DMA1_Channel1, ENABLE);

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

//	ADC_ITConfig(ADC1, ADC_IT_EOC, ENABLE);
//	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5);
// ADC_SoftwareStartConvCmd(ADC1, ENABLE);
//
//	NVIC_InitTypeDef NVIC_InitStructure;
//	NVIC_InitStructure.NVIC_IRQChannel = ADC1_2_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//	NVIC_Init(&NVIC_InitStructure);


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

void DMA1_Channel1_IRQHandler(void){
	DMA_ClearITPendingBit(DMA1_IT_HT1);
	int i;
	for(i=0; i<1; i++);
	return;
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
