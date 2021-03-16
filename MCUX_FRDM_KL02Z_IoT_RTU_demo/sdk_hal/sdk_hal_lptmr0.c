/*! @file : sdk_hal_lptmr0.c
 * @author  Ernesto Andres Rincon Cruz
 * @version 1.0.0
 * @date    6/03/2021
 * @brief   Driver para Timer de bajo consumo LPTMR0
 * @details
 *
*/
/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "sdk_hal_lptmr0.h"
#include "fsl_common.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* Define LPTMR microseconds counts value */
#define LPTMR_USEC_COUNT 1000U

/* Get source clock for LPTMR driver */
#define LPTMR_SOURCE_CLOCK CLOCK_GetFreq(kCLOCK_LpoClk)
/*******************************************************************************
 * Private Prototypes
 ******************************************************************************/


/*******************************************************************************
 * External vars
 ******************************************************************************/


/*******************************************************************************
 * Local vars
 ******************************************************************************/
volatile uint32_t lptmr0_counter = 0U;

/*******************************************************************************
 * Private Source Code
 ******************************************************************************/
void LPTMR0_IRQHandler(void)
{
    LPTMR_ClearStatusFlags(LPTMR0, kLPTMR_TimerCompareFlag);
    lptmr0_counter++;
    /*
     * Workaround for TWR-KV58: because write buffer is enabled, adding
     * memory barrier instructions to make sure clearing interrupt flag completed
     * before go out ISR
     */
    __DSB();
    __ISB();
}

/*******************************************************************************
 * Public Source Code
 ******************************************************************************/
void lptmr0Init(void){
	lptmr_config_t lptmr0_config;

    /* Configure LPTMR */
    /*
     * lptmr0_config.timerMode = kLPTMR_TimerModeTimeCounter;
     * lptmr0_config.pinSelect = kLPTMR_PinSelectInput_0;
     * lptmr0_config.pinPolarity = kLPTMR_PinPolarityActiveHigh;
     * lptmr0_config.enableFreeRunning = false;
     * lptmr0_config.bypassPrescaler = true;
     * lptmr0_config.prescalerClockSource = kLPTMR_PrescalerClock_1;
     * lptmr0_config.value = kLPTMR_Prescale_Glitpaintch_0;
     */
    LPTMR_GetDefaultConfig(&lptmr0_config);

    /* Initialize the LPTMR */
    LPTMR_Init(LPTMR0, &lptmr0_config);

    /*
     * Set timer period.
     * Note : the parameter "ticks" of LPTMR_SetTimerPeriod should be equal or greater than 1.
    */
    LPTMR_SetTimerPeriod(LPTMR0, USEC_TO_COUNT(LPTMR_USEC_COUNT, LPTMR_SOURCE_CLOCK));

    /* Enable timer interrupt */
    LPTMR_EnableInterrupts(LPTMR0, kLPTMR_TimerInterruptEnable);

    /* Enable at the NVIC */
    EnableIRQ(LPTMR0_IRQn);

    /* Start counting */
    LPTMR_StartTimer(LPTMR0);

}

uint32_t lptmr0GetTimeValue(void){

	return(lptmr0_counter);
}

void lptmr0SetTimeValue(uint32_t timeValue){
	lptmr0_counter=timeValue;
}
