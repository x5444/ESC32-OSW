#include "res.h"
#include <math.h>
#include <misc.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_adc.h>
#include <stm32f10x_tim.h>
#include <stm32f10x_dma.h>



// x = linspace(0,2*pi, 21);
// y = round(0.5*(71000/100)*(1 + sin(x(1:end-1))));
// str=''; for i=1:numel(y), str=[str ', ' num2str(y(i))]; end, disp(str);
static const uint32_t sinTab[] = {355, 465, 564, 642, 693, 710, 694, 643, 565, 466, 356, 246, 147, 69, 18, 0, 17, 68,146, 245};

volatile uint32_t adcBuffer[40 * sizeof(sinTab)/sizeof(uint32_t)];
int resCalibOffset0 = 0;
int resCalibOffset1 = 0;



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
	TIM_TimeBaseInitStructure.TIM_Period = (uint16_t)(F_CPU / 100000)-1; // 100kHz
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

	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_28Cycles5);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_28Cycles5);

	ADC_DMACmd(ADC1, ENABLE);

	ADC_Cmd(ADC1, ENABLE);

	ADC_ExternalTrigConvCmd(ADC1, ENABLE);

	ADC_ResetCalibration(ADC1);
	while(ADC_GetResetCalibrationStatus(ADC1));
	ADC_StartCalibration(ADC1);
	while(ADC_GetCalibrationStatus(ADC1));


	// Configure DMA
	// - Circularly copy entries from ADC1 to the adcBuffer using DMA1 Ch1
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
	DMA_Cmd(DMA1_Channel1, ENABLE);
}



int inRange(int a, int N){
	while(a<0){
		a += N;
	}
	return a%N;
}



void resCalibrate(void){
	int numRuns = sizeof(sinTab)/sizeof(uint32_t);
	int max0val = 0;
	int max0idx = 0;
	int max1val = 0;
	int max1idx = 0;

	for(int run=0; run<numRuns; run++){

		uint32_t numEl = sizeof(adcBuffer)/(2*sizeof(uint32_t));
		int32_t startIdx = (numEl - (int)DMA_GetCurrDataCounter(DMA1_Channel1));
		startIdx = (startIdx < 0) ? startIdx + 2*numEl : startIdx;
		startIdx -= startIdx % 2;

		int32_t res0 = 0;
		int32_t res1 = 0;

		int32_t acc0 = 0;
		int32_t acc1 = 0;

		for(int i=2; i<(numEl/2); i+=2){
			res0 = 0.75*res0 + adcBuffer[inRange(startIdx+i+0, 2*numEl)] - adcBuffer[inRange(startIdx+i-2, 2*numEl)];
			res1 = 0.75*res1 + adcBuffer[inRange(startIdx+i+1, 2*numEl)] - adcBuffer[inRange(startIdx+i-1, 2*numEl)];

			acc0 = acc0 + res0 * ((int32_t)sinTab[(i/2 + run + startIdx/2)%(sizeof(sinTab)/sizeof(uint32_t))] - 355);
			acc1 = acc1 + res1 * ((int32_t)sinTab[(i/2 + run + startIdx/2)%(sizeof(sinTab)/sizeof(uint32_t))] - 355);
		}

		acc0 = (acc0 < 0) ? -acc0 : acc0;
		acc1 = (acc1 < 0) ? -acc1 : acc1;

		if(acc0 > max0val){
			max0val = acc0;
			max0idx = run;
		}

		if(acc1 > max1val){
			max1val = acc1;
			max1idx = run;
		}
	}

	resCalibOffset0 = max0idx;
	resCalibOffset1 = max1idx;
}



float resGetAngle(void){
	float angle = 0;

	uint32_t numEl = sizeof(adcBuffer)/(2*sizeof(uint32_t));
	int32_t startIdx = (numEl - (int)DMA_GetCurrDataCounter(DMA1_Channel1));
	startIdx = (startIdx < 0) ? startIdx + 2*numEl : startIdx;
	startIdx -= startIdx % 2;

	int32_t res0 = 0;
	int32_t res1 = 0;

	int32_t acc0 = 0;
	int32_t acc1 = 0;

	int32_t p0 = 0;
	int32_t p1 = 0;

	for(int i=2; i<(numEl/2); i+=2){
		res0 = 0.75*res0 + adcBuffer[inRange(startIdx+i+0, 2*numEl)] - adcBuffer[inRange(startIdx+i-2, 2*numEl)];
		res1 = 0.75*res1 + adcBuffer[inRange(startIdx+i+1, 2*numEl)] - adcBuffer[inRange(startIdx+i-1, 2*numEl)];

		p0 = p0 + res0 * ((int32_t)sinTab[(i/2 + resCalibOffset0 + startIdx/2)%(sizeof(sinTab)/sizeof(uint32_t))] - 355);
		p1 = p1 + res1 * ((int32_t)sinTab[(i/2 + resCalibOffset1 + startIdx/2)%(sizeof(sinTab)/sizeof(uint32_t))] - 355);
	}

	acc0 = p0;
	acc1 = p1;


	if( acc0 > 0 && acc1 > 0 ){
		angle = 90.0f;
	}else if( acc0 > 0 && acc1 < 0){
		angle = 270.0f;
	}else if( acc0 < 0 && acc1 > 0){
		angle = 90.0f;
	}else if( acc0 < 0 && acc1 < 0){
		angle = 270.0f;
	}else{
		angle = 0.0f;
	}

	angle += (180.0f / 3.1415f) * atanf( ((float)acc0) / ((float)acc1) );

	angle = (angle < 0) ? (angle + 360.0f) : angle;

	return angle;
}
