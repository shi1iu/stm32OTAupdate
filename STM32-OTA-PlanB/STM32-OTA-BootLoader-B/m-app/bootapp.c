#include "bootapp.h"
#include "main.h"
#include "stdio.h"
#include "shell_port.h"

static void BOOTDisplay()
{
	 printf("\r\n");
	 printf("***********************************\r\n");
	 printf("*                                 *\r\n");
	 printf("*    STM32        BootLoader      *\r\n");
	 printf("*                                 *\r\n");
	 printf("***********************************\r\n");
	
	printf(">**** Pressing any key to enter boot mode ****\r\n");	
	printf("Enter the countdown 5 seconds \r\n");
}


void BOOTCountdown(void)
{
	int countdown = 5;
  uint32_t last_tick = HAL_GetTick();
	BOOTDisplay();
    while (countdown > 0)
    {
        if (g_uart_has_data)
        {
            printf("\r\n Successfully entered boot mode \r\n");
						break;
        }
        if (HAL_GetTick() - last_tick >= 1000)
        {
            last_tick += 1000;
            if (countdown > 0)
            {
                printf("%d...\r\n", countdown);
            }
						countdown--;
        }
        
        HAL_Delay(10);
    }
	
}