#include "usartx.h"

u8 Usart2_Receive;

/**************************************************************************
函数功能：串口3初始化
入口参数： bound:波特率
返回  值：无
**************************************************************************/
void uart1_init(u32 bound)
{  	 
	//GPIO端口设置
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	//
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);	//使能GPIO时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);	//使能USART时钟
	//USART_TX  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //PA2
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	//USART_RX	  
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//PA3
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	//UsartNVIC 配置
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0 ;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		//子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器
	//USART 初始化设置
	USART_InitStructure.USART_BaudRate = bound;//串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
	USART_Init(USART1, &USART_InitStructure);     //初始化串口3
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//开启串口接受中断
	USART_Cmd(USART1, ENABLE);                    //使能串口3 
}

/**************************************************************************
函数功能：串口2接收中断
入口参数：无
返回  值：无
**************************************************************************/
u8 Receive[50];
uint8_t g_bluetooth_flag = 0;

void USART1_IRQHandler(void)
{	
	static u8 i = 0, j = 0, Flag_CSB, Flag_PID;
	static	int uart_receive=0;//蓝牙接收相关变量
	static float Data;
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) //接收到数据
	{	  
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
		g_bluetooth_flag = 1;
		uart_receive=USART_ReceiveData(USART1); 
		Usart2_Receive=uart_receive;

		if(uart_receive==0x59)  Flag_sudu=2;  //低速挡（默认值）
		if(uart_receive==0x58)  Flag_sudu=1;  //高速档 

		if(uart_receive==0x62)Flag_Hover=!Flag_Hover;
		if(uart_receive==0x61)Flag_CSB=!Flag_CSB;
		if(Bi_zhang!=0)
		{		
			Bi_zhang=Flag_CSB+1;
		}

		if(uart_receive>10)  //默认使用app
		{			
			Remoter_Ch1=0,Remoter_Ch2=0; //APP接收到信号之后 屏蔽航模遥控
			if(uart_receive==0x5A)	Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////刹车
			else if(uart_receive==0x41)	Flag_Qian=1,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////前
			else if(uart_receive==0x45)	Flag_Qian=0,Flag_Hou=1,Flag_Left=0,Flag_Right=0;//////////////后
			else if(uart_receive==0x42||uart_receive==0x43||uart_receive==0x44)	
								Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=1;  //左
			else if(uart_receive==0x46||uart_receive==0x47||uart_receive==0x48)	    //右
								Flag_Qian=0,Flag_Hou=0,Flag_Left=1,Flag_Right=0;
			else Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////刹车
		}
		if(uart_receive<10)     //备用app为：MiniBalanceV1.0  因为MiniBalanceV1.0的遥控指令为A~H 其HEX都小于10
		{		
			Remoter_Ch1=0,Remoter_Ch2=0; //APP接收到信号之后 屏蔽航模遥控			
			if(uart_receive==0x00)	Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////刹车
			else if(uart_receive==0x01)	Flag_Qian=1,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////前
			else if(uart_receive==0x05)	Flag_Qian=0,Flag_Hou=1,Flag_Left=0,Flag_Right=0;//////////////后
			else if(uart_receive==0x02||uart_receive==0x03||uart_receive==0x04)	
								Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=1;  //左
			else if(uart_receive==0x06||uart_receive==0x07||uart_receive==0x08)	    //右
								Flag_Qian=0,Flag_Hou=0,Flag_Left=1,Flag_Right=0;
			else Flag_Qian=0,Flag_Hou=0,Flag_Left=0,Flag_Right=0;//////////////刹车
		}	

		if(Usart2_Receive==0x7B) Flag_PID=1;  //指令起始位
		if(Usart2_Receive==0x7D) Flag_PID=2;  //指令停止位
		if(Flag_PID==1)                      //记录参数
		{
			Receive[i]=Usart2_Receive;
			i++;
		}
		if(Flag_PID==2)   //执行
		{
			Show_Data_Mb=i;
			if(Receive[3]==0x50) 	  PID_Send=1;     //获取设备参数
			else if(Receive[3]==0x57)Flash_Send=1;   //掉电保存参数
			else if(Receive[1]!=0x23)                    //更新PID参数
			{								
				for(j=i;j>=4;j--)
				{
					Data+=(Receive[j-1]-48)*pow(10,i-j);
				}
				switch(Receive[1])
				{
					case 0x30:  Balance_Kp=Data/100;break;
					case 0x31:  Balance_Kd=Data/100;break;
					case 0x32:  Velocity_Kp=Data/100;break;
					case 0x33:  Velocity_Ki=Data/100;break;
					case 0x34:  break;
					case 0x35:  break;
					case 0x36:  break;
					case 0x37:  break;
					case 0x38:  break;
				}
			}				 
			Flag_PID=0;   //相关标志位清零
			i=0;
			j=0;
			Data=0;
			memset(Receive, 0, sizeof(u8)*50);
		} 
	}  											 
} 


