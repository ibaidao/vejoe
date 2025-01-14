#include "Config.h"
#include "Device.h"
#include "PCB.h"
#include "Tools.h"
#include "Components.h"
#include "APP.h"

int main(void)
{	
	//失能JTAG调试模式（GPIOA的15口跟PWM复用）
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,  ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);	
	//每秒1000次中断（1ms一次）
	Config_TIMER(TIMER_3,1,1000);
	//霍尔编码器初始化
	GPIOConfigStruct hallEncoderLeft = {ChannelA,{0,1},2},
									 hallEncoderRight= {ChannelB,{6,7},2};
	HallEncoderInit(&hallEncoderLeft,1,HallEncoderLeftWheel);
	HallEncoderInit(&hallEncoderRight,1,HallEncoderRightWheel);
	HallSpeedInitial();	
	//OLED初始化
	OLED_Init(ChannelC,15,0,13,14);
	//电机初始化
	GPIOConfigStruct motorGPIOConfig[2] = {
		{ChannelC,{8,9,12},3},
		{ChannelA,{15},1},
	};
	WheelGPIOInfo wheelGPIOConfig[2] = {
		{ChannelC,8,ChannelC,9},
		{ChannelA,15,ChannelC,12}
	};
	GPIOConfigStruct pwmGPIOConfig[1] = {
		{ChannelA,{8,11},2},
	};
	PWMBalanceCarInitial(motorGPIOConfig,2,wheelGPIOConfig,pwmGPIOConfig,1,7199,0);	
	//AD转换模块初始化
	StructAdcChannelInfo adcChannelInfo[6] = {
		{GPIO_Pin_3,ADC_Channel_3,GPIOA},
		{GPIO_Pin_4,ADC_Channel_4,GPIOA},
		{GPIO_Pin_5,ADC_Channel_5,GPIOA},
		{GPIO_Pin_6,ADC_Channel_6,GPIOA},
		{GPIO_Pin_7,ADC_Channel_7,GPIOA},
		{GPIO_Pin_4,ADC_Channel_14,GPIOC}
	};
	DeviceADCInitial(adcChannelInfo,6);
	//电机安全检测初始化
	StructMotorSafeInfo motorSafeInfo[2] = {
		{2500,500,1400,0.001, 14, 9},
		{2500,500,1400,0.001, 14, 9}
	};
	motorSafetyCheckInitital(motorSafeInfo,2,1,3,2);
	//采集通道数据
	ReadOffsetCurrentValue(0, 2);
	ReadOffsetCurrentValue(1, 3);	
	// PID 控制
	appLocationStable(15000,-15000);	
	//局部变量
	int encoderLeft, encoderRight, speedLeft, speedRight;
	u16 batteryVol, leftCur, rightCur;
	while(1)
	{
		//编码器数值显示
		encoderLeft = Read_ABS_Value(HallEncoderLeftWheel);
		encoderRight = Read_ABS_Value(HallEncoderRightWheel);
		OLED_ShowNumber2(00,00,encoderLeft,6,12);
		OLED_ShowNumber2(60,00,encoderRight,6,12);
		//速度显示
		speedLeft = getHallChangeSpeed(HallEncoderLeftWheel);
		speedRight = getHallChangeSpeed(HallEncoderRightWheel);		
		showSpeedValue(speedLeft,speedRight);
		//电机安全检测
		FilterADCValue();
		RefreshVolCurValue();
		GeneralSafetyCheck();
		//显示电流电压值
		GetVolCurValue(&batteryVol,&leftCur,&rightCur);
		ShowVolCurValue(batteryVol,leftCur,rightCur);
	}
}

//UINT32 g_FCY = 36000000;
//UINT8 Memory_Buf[3000];

//void MotorTest(void);

//INT32 g_Encoder = 0;
//INT32 g_Speed = 0;
//UINT16 g_ADValue1 = 0;
//UINT16 g_ADValue2 = 0;
//UINT16 g_current = 0;
//UINT16 g_pwm = 0;
//struct _PID_Info g_PID1;
//struct _PID_Info g_PID2;
//UINT16 g_Target_Current;
//INT32 g_Target_Speed;
//INT32 g_Target_Encoder;

//int main(void)
//{
//	CreatBuffer((UINT8*)Memory_Buf, sizeof(Memory_Buf));
//	My_memset(Memory_Buf, sizeof(Memory_Buf), 0x00);

//	IO_Init();									//初始化IO端口
//	Config_SysTick(1000);			//系统节拍定时器
//	Config_TIMER(TIMER_2, 3, 3000);
//	//Timer_Register(TIMER_2, Get_Adc);
//	SysTick_Register(MotorTest);
//	Config_LED(1, SYS_TIMER);
//	Register_LED(0, Set_Led2, LED_BLINK, 1000);
//	
//	HBDC_Config(1, 20000);
//	
//	Encoder_Init_TIM4();
//	
//	Add_ADC_CH(0, GPIOA, GPIO_Pin_5, ADC_Channel_5);
//	Add_ADC_CH(1, GPIOA, GPIO_Pin_6, ADC_Channel_6);
//	Enable_ADC();
//	
//	Config_Current_Manager(1, SYS_TIMER);
//	Register_Current(0, Get_Current_Sample, 5.371);	//  1000*3.3/4096/(0.025*6)
//	//获得初始化静态AD值（对应电流为0），此时保持所有电机关闭。
//	while(!Start_Current_Manager());

//	Config_Encoder_Manager(1, SYS_TIMER);
//	Register_Encoder(0, Get_Encoder);
//	while(!Start_Encoder_Manager());
//	
//	Config_PID(&g_PID1, 0.5, 0.02, 0.0, 99, 0);
//	Config_PID(&g_PID2, 0.25, 0.01, 0.0, 99, 0);
//	g_Target_Current = 72;
//	g_Target_Speed = 30;
//	g_Target_Encoder = 8800;
//	
//	//待补充：中断优先级设定
//	
//	while(1)
//	{
//		//Set_Led2(Get_SW1() == Get_SW2());
//		//Set_Led1(Get_SW3() == Get_SW4());
//		/*if(Get_SW1())
//		{
//			HBDC_MoveForward(g_pwm);
//		}
//		else if(Get_SW2())
//		{
//			HBDC_MoveForward(g_pwm);
//		}
//		else if(Get_SW3())
//		{
//			HBDC_MoveForward(70);
//		}
//		else if(Get_SW4())
//		{
//			HBDC_MoveForward(90);
//		}
//		else
//		{
//			HBDC_Stop();
//		}
//		
//		g_ADValue1 = Get_ADC_Value(0);
//		g_ADValue2 = Get_ADC_Value(1);*/
//		g_current = Get_Current(0);
//		g_Encoder = Get_Position(0);
//		g_Speed = Get_Speed(0);
//	}
//}

//void MotorTest(void)
//{
//	static UINT16 tick = 0;
//	static UINT8 tt = 0;
//	tt++;
//	if(tt < 10) return;
//	tt = 0;

//	tick++;
//	if(tick < 100) g_pwm = 0;
//	else if(tick < 600)
//	{
//		g_Target_Speed = 15;
//		if(g_Speed < g_Target_Speed)
//		{
//			if(g_pwm < 97) g_pwm++;
//		}
//		else
//		{
//			if(g_pwm > 0) g_pwm--;
//		}
//		HBDC_MoveForward(g_pwm);
//	}
//	else if(tick < 700)
//	{
//		HBDC_Stop();
//		g_pwm = 0;
//	}
//	else if(tick < 1100)
//	{
//		g_Target_Speed = -15;
//		if(g_Speed > g_Target_Speed)
//		{
//			if(g_pwm < 97) g_pwm++;
//		}
//		else
//		{
//			if(g_pwm > 0) g_pwm--;
//		}
//		HBDC_MoveBackward(g_pwm);
//	}
//	else if(tick < 1200)
//	{
//		HBDC_Stop();
//		g_pwm = 0;
//	}
//	else if(tick < 1600)
//	{
//		g_Target_Speed = 30;
//		if(g_Speed < g_Target_Speed)
//		{
//			if(g_pwm < 97) g_pwm++;
//		}
//		else
//		{
//			if(g_pwm > 0) g_pwm--;
//		}
//		HBDC_MoveForward(g_pwm);
//	}
//	else if(tick < 1700)
//	{
//		HBDC_Stop();
//		g_pwm = 0;
//	}
//	else if(tick < 2100)
//	{
//		g_Target_Speed = -30;
//		if(g_Speed > g_Target_Speed)
//		{
//			if(g_pwm < 97) g_pwm++;
//		}
//		else
//		{
//			if(g_pwm > 0) g_pwm--;
//		}
//		HBDC_MoveBackward(g_pwm);
//	}
//	else if(tick < 2200)
//	{
//		HBDC_Stop();
//		g_pwm = 0;
//	}
//	else if(tick < 3200)
//	{
//		HBDC_Stop();
//		tick = 0;
//	}
//	
//	
//	/*if(Get_SW1())
//	{		
//		g_pwm = (UINT16)Get_PID_Output(&g_PID1, g_Target_Speed - g_Speed);
//	}
//	else if(Get_SW2())
//	{		
//		g_pwm = (UINT16)Get_PID_Output(&g_PID1, g_Target_Current - g_current);
//	}
//	
//	if(g_Encoder >= 8800) g_pwm = 0;
//	else if(g_Encoder >= 8780) g_Target_Speed = 1;
//	else if(g_Encoder >= 8750) g_Target_Speed = 5;
//	else if(g_Encoder >= 8700) g_Target_Speed = 20;*/
//}


