#include "ymodem.h"
#include "main.h"
#include "stdio.h"


/* 发送指令 */
void send_command(unsigned char command)
{
	HAL_UART_Transmit(&huart1, (uint8_t *)&command,1, 0xFFFF);
	HAL_Delay(10);
}



/**
 * @bieaf 擦除页
 *
 * @param pageaddr  页起始地址	
 * @param num       擦除的页数
 * @return 1
 */
static int Erase_page(uint32_t pageaddr, uint32_t num)
{
	HAL_FLASH_Unlock();
	
	/* 擦除FLASH*/
	FLASH_EraseInitTypeDef FlashSet;
	FlashSet.TypeErase = FLASH_TYPEERASE_PAGES;
	FlashSet.PageAddress = pageaddr;
	FlashSet.NbPages = num;
	
	/*设置PageError，调用擦除函数*/
	uint32_t PageError = 0;
	HAL_FLASHEx_Erase(&FlashSet, &PageError);
	
	HAL_FLASH_Lock();
	return 1;
}



/**
 * @bieaf 写若干个数据
 *
 * @param addr       写入的地址
 * @param buff       写入数据的数组指针
 * @param word_size  长度
 * @return 
 */
static void WriteFlash(uint32_t addr, uint32_t * buff, int word_size)
{	
	/* 1/4解锁FLASH*/
	HAL_FLASH_Unlock();
	
	for(int i = 0; i < word_size; i++)	
	{
		/* 3/4对FLASH烧写*/
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4 * i, buff[i]);	
	}

	/* 4/4锁住FLASH*/
	HAL_FLASH_Lock();
}


/* 标记升级完成 */
void Set_Update_Down(void)
{
	unsigned int update_flag = 0xAAAAAAAA;				///< 对应bootloader的启动步骤
	WriteFlash((Application_2_Addr + Application_Size - 4), &update_flag,1 );
}



/* 临时存储的buff */
unsigned char save_buf[128] = {0};



/**
 * @bieaf CRC-16 校验
 *
 * @param addr 开始地址
 * @param num   长度
 * @param num   CRC
 * @return crc  返回CRC的值
 */
#define POLY        0x1021  
uint16_t crc16(unsigned char *addr, int num, uint16_t crc)  
{  
    int i;  
    for (; num > 0; num--)					/* Step through bytes in memory */  
    {  
        crc = crc ^ (*addr++ << 8);			/* Fetch byte from memory, XOR into CRC top byte*/  
        for (i = 0; i < 8; i++)				/* Prepare to rotate 8 bits */  
        {
            if (crc & 0x8000)				/* b15 is set... */  
                crc = (crc << 1) ^ POLY;  	/* rotate and XOR with polynomic */  
            else                          	/* b15 is clear... */  
                crc <<= 1;					/* just rotate */  
        }									/* Loop for 8 bits */  
        crc &= 0xFFFF;						/* Ensure CRC remains 16-bit value */  
    }										/* Loop until num=0 */  
    return(crc);							/* Return updated CRC */  
}



/**
 * @bieaf 获取数据包的类型, 顺便进行校验
 *
 * @param buf 开始地址
 * @param len 长度
 * @return 
 */
unsigned char Check_CRC(unsigned char* buf, int len)
{
	unsigned short crc = 0;
	
	/* 进行CRC校验 */
	if((buf[0]==0x00)&&(len >= 133))
	{
		crc = crc16(buf+3, 128, crc);
		if(crc != (buf[131]<<8|buf[132]))
		{
			return 0;///< 没通过校验
		}
		
		/* 通过校验 */
		return 1;
	}
}



/* 设置升级的步骤 */
static enum UPDATE_STATE update_state = TO_START;
void Set_state(enum UPDATE_STATE state)
{
	update_state = state;
}


/* 查询升级的步骤 */
unsigned char Get_state(void)
{
	return update_state;
}



unsigned char temp_buf[512] = {0};
unsigned char temp_len = 0;



/**
 * @bieaf YModem升级
 *
 * @param none
 * @return none
 */
void ymodem_fun(void)
{
	int i;
	if(Get_state()==TO_START)
	{
		send_command(CCC);
		HAL_Delay(1000);
	}
	if(Rx_Flag)    	// Receive flag
	{
		Rx_Flag=0;	// clean flag
				
		/* 拷贝 */
		temp_len = Rx_Len;
		for(i = 0; i < temp_len; i++)
		{
			temp_buf[i] = Rx_Buf[i];
		}
		
		switch(temp_buf[0])
		{
			case SOH:///<数据包开始
			{
				static unsigned char data_state = 0;
				static unsigned int app2_size = 0;
				if(Check_CRC(temp_buf, temp_len)==1)///< 通过CRC16校验
				{					
					if((Get_state()==TO_START)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 开始
					{
						printf("> Receive start...\r\n");

						Set_state(TO_RECEIVE_DATA);
						data_state = 0x01;						
						send_command(ACK);
						send_command(CCC);

						/* 擦除App2 */							
						Erase_page(Application_2_Addr, 40);
					}
					else if((Get_state()==TO_RECEIVE_END)&&(temp_buf[1] == 0x00)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 结束
					{
						printf("> Receive end...\r\n");

						Set_Update_Down();						
						Set_state(TO_START);
						send_command(ACK);
						HAL_NVIC_SystemReset();
					}					
					else if((Get_state()==TO_RECEIVE_DATA)&&(temp_buf[1] == data_state)&&(temp_buf[2] == (unsigned char)(~temp_buf[1])))///< 接收数据
					{
						printf("> Receive data bag:%d byte\r\n",data_state * 128);
						
						/* 烧录程序 */
						WriteFlash((Application_2_Addr + (data_state-1) * 128), (uint32_t *)(&temp_buf[3]), 32);
						data_state++;
						
						send_command(ACK);		
					}
				}
				else
				{
					printf("> Notpass crc\r\n");
				}
				
			}break;
			case EOT://数据包开始
			{
				if(Get_state()==TO_RECEIVE_DATA)
				{
					printf("> Receive EOT1...\r\n");
					
					Set_state(TO_RECEIVE_EOT2);					
					send_command(NACK);
				}
				else if(Get_state()==TO_RECEIVE_EOT2)
				{
					printf("> Receive EOT2...\r\n");
					
					Set_state(TO_RECEIVE_END);					
					send_command(ACK);
					send_command(CCC);
				}
				else
				{
					printf("> Receive EOT, But error...\r\n");
				}
			}break;	
		}
	}
}



