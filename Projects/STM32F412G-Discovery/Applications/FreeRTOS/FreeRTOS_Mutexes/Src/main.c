/**
  ******************************************************************************
  * @file    FreeRTOS/FreeRTOS_Mutexes/Src/main.c
  * @author  MCD Application Team
  * @version V1.0.2
  * @date    04-November-2016
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright � 2016 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define mutexSHORT_DELAY     ((uint32_t) 20)
#define mutexNO_DELAY        ((uint32_t) 0)
#define mutexTWO_TICK_DELAY  ((uint32_t) 2)

/* Private variables ---------------------------------------------------------*/
static osMutexId osMutex;

/* Variables used to detect and latch errors */
__IO uint32_t HighPriorityThreadCycles = 0, MediumPriorityThreadCycles = 0, LowPriorityThreadCycles = 0;

/* Handles of the two higher priority tasks, required so they can be resumed
   (unsuspended) */
static osThreadId osHighPriorityThreadHandle, osMediumPriorityThreadHandle;

/* Private function prototypes -----------------------------------------------*/
static void MutexHighPriorityThread(void const *argument);
static void MutexMediumPriorityThread(void const *argument);
static void MutexLowPriorityThread(void const *argument);
static void SystemClock_Config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch
       - Systick timer is configured by default as source of time base, but user 
         can eventually implement his proper time base source (a general purpose 
         timer for example or other time source), keeping in mind that Time base 
         duration should be kept 1ms since PPP_TIMEOUT_VALUEs are defined and 
         handled in milliseconds basis.
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();

  /* Configure the System clock to 100 MHz */
  SystemClock_Config();

  /* Initialize LEDs */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED4);
  BSP_LED_Init(LED3);

  /* Create the mutex  */
  osMutexDef(osMutex);
  osMutex = osMutexCreate(osMutex(osMutex));

  if (osMutex != NULL)
  {
    /* Define and create the high priority thread */
    osThreadDef(MutHigh, MutexHighPriorityThread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE);
    osHighPriorityThreadHandle = osThreadCreate(osThread(MutHigh), NULL);

    /* Define and create the medium priority thread */
    osThreadDef(MutMedium, MutexMediumPriorityThread, osPriorityLow, 0, configMINIMAL_STACK_SIZE);
    osMediumPriorityThreadHandle = osThreadCreate(osThread(MutMedium), NULL);

    /* Define and create the low priority thread */
    osThreadDef(MutLow, MutexLowPriorityThread, osPriorityIdle, 0, configMINIMAL_STACK_SIZE);
    osThreadCreate(osThread(MutLow), NULL);
  }

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  for (;;);
}

/**
  * @brief  Mutex High Priority Thread.
  * @param  argument: Not used
  * @retval None
  */
static void MutexHighPriorityThread(void const *argument)
{
  /* Just to remove compiler warning */
  (void) argument;

  for (;;)
  {
    /* The first time through the mutex will be immediately available, on
    subsequent times through the mutex will be held by the low priority thread
    at this point and this Take will cause the low priority thread to inherit
    the priority of this tadhr.  In this case the block time must be
    long enough to ensure the low priority thread will execute again before the
    block time expires.  If the block time does expire then the error
    flag will be set here */
    if (osMutexWait(osMutex, mutexTWO_TICK_DELAY) != osOK)
    {
      /* Toggle LED3 to indicate error */
      BSP_LED_Toggle(LED3);
    }

    /* Ensure the other thread attempting to access the mutex
    are able to execute to ensure they either block (where a block
    time is specified) or return an error (where no block time is
    specified) as the mutex is held by this task */
    osDelay(mutexSHORT_DELAY);

    /* We should now be able to release the mutex .
    When the mutex is available again the medium priority thread
    should be unblocked but not run because it has a lower priority
    than this thread.  The low priority thread should also not run
    at this point as it too has a lower priority than this thread */
    if (osMutexRelease(osMutex) != osOK)
    {
      /* Toggle LED3 to indicate error */
      BSP_LED_Toggle(LED3);
    }

    /* Keep count of the number of cycles this thread has performed */
    HighPriorityThreadCycles++;
    BSP_LED_Toggle(LED1);

    /* Suspend ourselves to the medium priority thread can execute */
    osThreadSuspend(NULL);
  }
}

/**
  * @brief  Mutex Medium Priority Thread.
  * @param  argument: Not used
  * @retval None
  */
static void MutexMediumPriorityThread(void const *argument)
{
  /* Just to remove compiler warning */
  (void) argument;

  for (;;)
  {
    /* This thread will run while the high-priority thread is blocked, and the
    high-priority thread will block only once it has the mutex - therefore
    this call should block until the high-priority thread has given up the
    mutex, and not actually execute past this call until the high-priority
    thread is suspended */
    if (osMutexWait(osMutex, osWaitForever) == osOK)
    {
      if (osThreadGetState(osHighPriorityThreadHandle) != osThreadSuspended)
      {
        /* Did not expect to execute until the high priority thread was
        suspended.
        Toggle LED3 to indicate error */
        BSP_LED_Toggle(LED3);
      }
      else
      {
        /* Give the mutex back before suspending ourselves to allow
        the low priority thread to obtain the mutex */
        if (osMutexRelease(osMutex) != osOK)
        {
          /* Toggle LED3 to indicate error */
          BSP_LED_Toggle(LED3);
        }
        osThreadSuspend(NULL);
      }
    }
    else
    {
      /* We should not leave the osMutexWait() function
      until the mutex was obtained.
      Toggle LED3 to indicate error */
      BSP_LED_Toggle(LED3);
    }

    /* The High and Medium priority threads should be in lock step */
    if (HighPriorityThreadCycles != (MediumPriorityThreadCycles + 1))
    {
      /* Toggle LED3 to indicate error */
      BSP_LED_Toggle(LED3);
    }

    /* Keep count of the number of cycles this task has performed so a
    stall can be detected */
    MediumPriorityThreadCycles++;
    BSP_LED_Toggle(LED2);
  }
}

/**
  * @brief  Mutex Low Priority Thread.
  * @param  argument: Not used
  * @retval None
  */
static void MutexLowPriorityThread(void const *argument)
{
  /* Just to remove compiler warning */
  (void) argument;

  for (;;)
  {
    /* Keep attempting to obtain the mutex.  We should only obtain it when
    the medium-priority thread has suspended itself, which in turn should only
    happen when the high-priority thread is also suspended */
    if (osMutexWait(osMutex, mutexNO_DELAY) == osOK)
    {
      /* Is the haigh and medium-priority threads suspended? */
      if ((osThreadGetState(osHighPriorityThreadHandle) != osThreadSuspended) || (osThreadGetState(osMediumPriorityThreadHandle) != osThreadSuspended))
      {
        /* Toggle LED3 to indicate error */
        BSP_LED_Toggle(LED3);
      }
      else
      {
        /* Keep count of the number of cycles this task has performed
        so a stall can be detected */
        LowPriorityThreadCycles++;
        BSP_LED_Toggle(LED4);

        /* We can resume the other tasks here even though they have a
        higher priority than the this thread. When they execute they
        will attempt to obtain the mutex but fail because the low-priority 
        thread is still the mutex holder.  this thread will then inherit 
        the higher priority.  The medium-priority thread will block indefinitely
        when it attempts to obtain the mutex, the high-priority thread will only
        block for a fixed period and an error will be latched if the
        high-priority thread has not returned the mutex by the time this
        fixed period has expired */
        osThreadResume(osMediumPriorityThreadHandle);
        osThreadResume(osHighPriorityThreadHandle);

        /* The other two tasks should now have executed and no longer
        be suspended */
        if ((osThreadGetState(osHighPriorityThreadHandle) == osThreadSuspended) || (osThreadGetState(osMediumPriorityThreadHandle) == osThreadSuspended))
        {
          /* Toggle LED3 to indicate error */
          BSP_LED_Toggle(LED3);
        }

        /* Release the mutex, disinheriting the higher priority again */
        if (osMutexRelease(osMutex) != osOK)
        {
          /* Toggle LED3 to indicate error */
          BSP_LED_Toggle(LED3);
        }
      }
    }

#if configUSE_PREEMPTION == 0
    {
      taskYIELD();
    }
#endif
  }
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 100000000
  *            HCLK(Hz)                       = 100000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 2
  *            APB2 Prescaler                 = 1
  *            HSE Frequency(Hz)              = 8000000
  *            PLL_M                          = 8
  *            PLL_N                          = 200
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 3
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;

  /* Enable Power Control clock */
  __HAL_RCC_PWR_CLK_ENABLE();

  /* The voltage scaling allows optimizing the power consumption when the device is 
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 200;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 2;
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  
  if(ret != HAL_OK)
  {
    while(1) { ; } 
  }

  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
     clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;  
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
  if(ret != HAL_OK)
  {
    while(1) { ; }  
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {}
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
