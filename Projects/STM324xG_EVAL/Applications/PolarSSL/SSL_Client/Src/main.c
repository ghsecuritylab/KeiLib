/**
  ******************************************************************************
  * @file    PolarSSL/SSL_Client/Src/main.c
  * @author  MCD Application Team
  * @version V1.3.6
  * @date    04-November-2016
  * @brief   Main program 
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
#include "ethernetif.h"
#include "lwip/netif.h"
#include "app_ethernet.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
struct netif gnetif; /* network interface structure */
/* Semaphore to signal Ethernet Link state update */
osSemaphoreId Netif_LinkSemaphore = NULL;
/* Ethernet link thread Argument */
struct link_str link_arg;

UART_HandleTypeDef UartHandle;
RNG_HandleTypeDef  RngHandle;

/* Private function prototypes -----------------------------------------------*/
static void SystemClock_Config(void);
static void StartThread(void const * argument);
static void ToggleLed4(void const * argument);
static void BSP_Config(void);
static void Netif_Config(void);

#ifdef __GNUC__
  /* With GCC/RAISONANCE, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
  /* STM32F4xx HAL library initialization:
       - Configure the Flash prefetch, instruction and Data caches
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Global MSP (MCU Support Package) initialization
     */
  HAL_Init();
  
  /* Configure the system clock to 168 MHz */
  SystemClock_Config();
  
  /* Init task */
  osThreadDef(Start, StartThread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE *2);
  osThreadCreate(osThread(Start), NULL);
  
  /* Start the scheduler */
  vTaskStartScheduler();
  
  /* We should never get here as control is now taken by the scheduler */
  for( ;; );
}

/**
  * @brief  Start thread
  * @param  argument: not used
  * @retval None
  */
static void StartThread(void const * argument)
{ 
  /* Initialize LCD and LEDs */
  BSP_Config();
  
  /* Create tcp_ip stack thread */
  tcpip_init(NULL, NULL);
  
  /* Initialize the LwIP stack */
  Netif_Config();
  
  /* Notify user about the network interface config */
  User_notification(&gnetif);
  
  /* Start SSL Client task : Connect to SSL server and provide the SSL handshake protocol */
  osThreadDef(Client, ssl_client, osPriorityAboveNormal, 0, configMINIMAL_STACK_SIZE * 20);
  osThreadCreate(osThread(Client), NULL);
  
#ifdef USE_DHCP
  /* Start DHCPClient */
  osThreadDef(DHCP, DHCP_thread, osPriorityBelowNormal, 0, configMINIMAL_STACK_SIZE * 2);
  osThreadCreate(osThread(DHCP), &gnetif);
#endif
  
  /* Start toogleLed4 task : Toggle LED4  every 250ms */
  osThreadDef(LED4, ToggleLed4, osPriorityLow, 0, configMINIMAL_STACK_SIZE);
  osThreadCreate(osThread(LED4), NULL);
  
  for( ;; )
  {
    /* Delete the start Thread */ 
    osThreadTerminate(NULL);
  }
}

/**
  * @brief  Network interface configuration
  * @param  None
  * @retval None
  */
static void Netif_Config(void)
{
  ip_addr_t ipaddr;
  ip_addr_t netmask;
  ip_addr_t gw;	

#ifdef USE_DHCP
  ipaddr.addr = 0;
  netmask.addr = 0;
  gw.addr = 0;
#else
  /* IP address default setting */
  IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
  IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
  IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif

  /* - netif_add(struct netif *netif, struct ip_addr *ipaddr,
  struct ip_addr *netmask, struct ip_addr *gw,
  void *state, err_t (* init)(struct netif *netif),
  err_t (* input)(struct pbuf *p, struct netif *netif))
  
  Adds your network interface to the netif_list. Allocate a struct
  netif and pass a pointer to this structure as the first argument.
  Give pointers to cleared ip_addr structures when using DHCP,
  or fill them with sane numbers otherwise. The state pointer may be NULL.
  
  The init function pointer must point to a initialization function for
  your ethernet netif interface. The following code illustrates it's use.*/
  
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);
  
  /* Registers the default network interface */
  netif_set_default(&gnetif);
  
  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }
  
  /* Set the link callback function, this function is called on change of link status */
  netif_set_link_callback(&gnetif, ethernetif_update_config);
  
  /* Create a binary semaphore used for informing ethernetif of frame reception */
  osSemaphoreDef(Netif_SEM);
  Netif_LinkSemaphore = osSemaphoreCreate(osSemaphore(Netif_SEM) , 1 );
  
  link_arg.netif = &gnetif;
  link_arg.semaphore = Netif_LinkSemaphore;
  /* Create the Ethernet link handler thread */
  osThreadDef(LinkThr, ethernetif_set_link, osPriorityNormal, 0, configMINIMAL_STACK_SIZE);
  osThreadCreate(osThread(LinkThr), &link_arg);
}

/**
  * @brief  Initializes the STM324xG-EVAL's LCD and LEDs resources.
  * @param  None
  * @retval None
  */
static void BSP_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
   
  /* Enable PB14 to IT mode: Ethernet Link interrupt */ 
  __HAL_RCC_GPIOB_CLK_ENABLE(); 
  GPIO_InitStructure.Pin = GPIO_PIN_14;
  GPIO_InitStructure.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStructure.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  /* Enable EXTI Line interrupt */
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0xF, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  
  /* Initialize RNG peripheral */
  HAL_RNG_Init(&RngHandle);

  /* UART configuration */
  UartHandle.Instance          = USART1;
  UartHandle.Init.BaudRate     = 9600;
  UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
  UartHandle.Init.StopBits     = UART_STOPBITS_1;
  UartHandle.Init.Parity       = UART_PARITY_NONE;
  UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  UartHandle.Init.Mode         = UART_MODE_TX;
  UartHandle.Init.OverSampling = UART_OVERSAMPLING_16;
    
  /* Initialize UART peripheral */
  HAL_UART_Init(&UartHandle);
  
  /* Configures COM1 port */
  BSP_COM_Init(COM1, &UartHandle);
  
  /* Configure LED1, LED2, LED3 and LED4 */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED2);
  BSP_LED_Init(LED3);
  BSP_LED_Init(LED4);
  
#ifdef USE_LCD

  /* Initialize the LCD */
  BSP_LCD_Init();
  
  BSP_LCD_SetFont(&Font20);
  
  /* Clear the LCD */
  BSP_LCD_Clear(LCD_COLOR_BLACK);
  
  /* Set the LCD Back Color */
  BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
  
  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  
  /* Display message on the LCD*/
  BSP_LCD_DisplayStringAtLine(0, (uint8_t*)"     STM32F417xx    ");
  BSP_LCD_DisplayStringAtLine(1, (uint8_t*)"   STM32F-4 Series  ");
  BSP_LCD_DisplayStringAtLine(2, (uint8_t*)"   SSL Client demo  ");
  BSP_LCD_DisplayStringAtLine(3, (uint8_t*)"   using HW Crypto  ");

#endif
}

/**
  * @brief  Toggle LED4 task
  * @param  pvParameters not used
  * @retval None
  */
static void ToggleLed4(void const * argument)
{
  for( ;; )
  {
    /* Toggle LED4 each 250ms */
    BSP_LED_Toggle(LED4);
    osDelay(250);
  }
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_14)
  {
    osSemaphoreRelease(Netif_LinkSemaphore);
  }
}

/**
  * @brief  Returns a random number.
  * @param  arg not used
  * @param  output random number
  * @param  output_len random number length
  * @retval 0
  */
int RandVal(void* arg, unsigned char *output, size_t output_len)
{
  uint32_t nbrng = 0;
  uint8_t offset = 0;
  
  nbrng = output_len;
  
  while(nbrng > 0)
  {
    /* Wait until random number is ready */  
    while(__HAL_RNG_GET_FLAG(&RngHandle, RNG_FLAG_DRDY)== RESET);
  
    /* Get the random number */
    *(output + offset) = HAL_RNG_GetRandomNumber(&RngHandle) && 0xFFFFFF00;
    
    offset++;
    nbrng --;
  }
 
  /* Return the random number */ 
  return(0);
}

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&UartHandle, (uint8_t *)&ch, 1, 0xFFFF); 

  return ch;
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 168000000
  *            HCLK(Hz)                       = 168000000
  *            AHB Prescaler                  = 1
  *            APB1 Prescaler                 = 4
  *            APB2 Prescaler                 = 2
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 25
  *            PLL_N                          = 360
  *            PLL_P                          = 2
  *            PLL_Q                          = 7
  *            VDD(V)                         = 3.3
  *            Main regulator output voltage  = Scale1 mode
  *            Flash Latency(WS)              = 5
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;

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
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  HAL_RCC_OscConfig(&RCC_OscInitStruct);
 
  /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 
  clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;  
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;  
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

  /* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported  */
  if (HAL_GetREVID() == 0x1001)
  {
    /* Enable the Flash prefetch */
    __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
*/
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  
  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
