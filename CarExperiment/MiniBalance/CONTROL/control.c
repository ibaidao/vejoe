#include "control.h"	
#include "filter.h"	
#include "Check_OverLoad.h"

struct Control_Info _control_Info = {0,0,0,0,0,0,0,0,0,0,0};

int lastEncoderLeft=0,lastEncoderRight=0,deltaEncoderLeft=0,deltaEncoderRight=0;
int absEncoderLeft = 0, absEncoderRight = 0;
int last_absEncoderLeft = 0, last_absEncoderRight = 0;
int delta_absEncoderLeft = 0, delta_absEncoderRight = 0;
int targetEncoderLeft = 0, targetEncoderRight = 0;
int targetYawn = 0;
int Balance_Pwm,Velocity_Pwm,Turn_Pwm;
int PosLeft_Pwm, PosRight_Pwm;
u8 Flag_Target;
u32 Flash_R_Count;
int Voltage_Temp,Voltage_Count,Voltage_All;

void _get_Encoder(void)
{
	absEncoderLeft = Read_ABS_Value_TIM2();
	absEncoderRight = Read_ABS_Value_TIM4();
	delta_absEncoderLeft = absEncoderLeft - last_absEncoderLeft;
	delta_absEncoderRight = absEncoderRight - last_absEncoderRight;
}

void _get_Voltage(void)
{
	Voltage_Temp=Get_battery_volt();		                                //=====读取电池电压		
	Voltage_Count++;                                                    //=====平均值计数器
	Voltage_All+=Voltage_Temp;                                          //=====多次采样累积
	if(Voltage_Count==100) Voltage=Voltage_All/100,Voltage_All=0,Voltage_Count=0;//=====求平均值		
}

void _get_Temperature(void)
{
	Temperature=Read_Temperature();      //===读取MPU6050内置温度传感器数据，近似表示主板温度。
}


/**************************************************************************
函数功能：所有的控制代码都在这里面
         5ms定时中断由MPU6050的INT引脚触发
         严格保证采样和数据处理的时间同步				 
**************************************************************************/
int EXTI2_IRQHandler(void) 
{ 
	if(INT==0)		
	{   
		EXTI_ClearITPendingBit(EXTI_Line2);  //清除中断标志位     

		_get_Encoder();
		
		Flag_Target=!Flag_Target;
		if(Flag_Target == 1)
		{
			_get_Voltage();
			_get_Temperature();
			Read_Distane();
		}		
	}       	
	return 0;	  
} 

/**************************************************************************
函数功能：直立PD控制
入口参数：角度、角速度
返回  值：直立控制PWM
**************************************************************************/
float static_kp = 0.5, static_ki = 0.5;
int Balance(float Angle,float Gyro)
{  
	float Bias;
	int balance;
	//static int sum_offset = 0;

	Bias = Angle - Zhongzhi;   
	
	//if(Angle > 270)
	//Bias = Zhongzhi - (360 - Angle); 	
	//else if(Angle < 90)	
	//Bias = Angle - Zhongzhi;                     		//===求出平衡的角度中值 和机械相关
	//sum_offset += Bias;
	//if(sum_offset > 1000)sum_offset = 1000;
	//else if(sum_offset < -1000)sum_offset = -1000;
	
	balance = Balance_Kp * Bias + Gyro * Balance_Kd;
	//balance = Balance_Kp * Bias + static_ki* sum_offset + Gyro * Balance_Kd;   	//===计算平衡控制的电机PWM  PD控制   kp是P系数 kd是D系数 
	return balance;
}

/**************************************************************************
函数功能：速度PI控制 修改前进后退速度，请修Target_Velocity，比如，改成60就比较慢了
入口参数：左轮编码器、右轮编码器
返回  值：速度控制PWM
**************************************************************************/
int velocity(int encoder_left,int encoder_right)
{  
	static float Velocity,Encoder_Least,Encoder,Movement;
	static float Encoder_Integral,Target_Velocity;
	//=============遥控前进后退部分=======================// 
	if(Flag_sudu==10)  Target_Velocity=80;				//转弯时，用更慢模式
	else if(Bi_zhang!=0&&Flag_sudu==1)  Target_Velocity=55;                 //如果进入避障模式,自动进入低速模式
	else            Target_Velocity=110;   
	
	Target_Velocity = 10;
	
	if(1==Flag_Qian)    	Movement=-Target_Velocity/Flag_sudu;	         //===前进标志位置1 
	else if(1==Flag_Hou)	Movement=Target_Velocity/Flag_sudu;         //===后退标志位置1
	else  Movement=0;	
	if(Bi_zhang==1&&Flag_Left!=1&&Flag_Right!=1)        //进入避障模式
	{
		if(Distance<500)  Movement=-Target_Velocity/Flag_sudu;
	}	
	else if(Bi_zhang==2&&Flag_Left!=1&&Flag_Right!=1)        //进入跟随模式
	{
		if(Distance>100&&Distance<300)  Movement=Target_Velocity/Flag_sudu;
	}
	//=============速度PI控制器=======================//	
	Encoder_Least = (encoder_left+encoder_right)-0;                    //===获取最新速度偏差==测量速度（左右编码器之和）-目标速度（此处为零） 
	Encoder *= 0.8;		                                                //===一阶低通滤波器       
	Encoder += Encoder_Least*0.2;	                                    //===一阶低通滤波器    
	Encoder_Integral +=Encoder;                                       //===积分出位移 积分时间：10ms
	Encoder_Integral=Encoder_Integral-Movement;                       //===接收遥控器数据，控制前进后退
	if(Encoder_Integral>10000)  	Encoder_Integral=10000;             //===积分限幅
	if(Encoder_Integral<-10000)	Encoder_Integral=-10000;              //===积分限幅	
	Velocity=Encoder*Velocity_Kp+Encoder_Integral*Velocity_Ki;        //===速度控制	

	if(Flag_Hover==1)
	{
		Zhongzhi=-Encoder/10-Encoder_Integral/300;
	}

	if(Turn_Off(Angle_Balance,Voltage)==1||Flag_Stop==1)   Encoder_Integral=0;      //===电机关闭后清除积分
	return Velocity;
}

/**************************************************************************
函数功能：转向控制  修改转向速度，请修改Turn_Amplitude即可
入口参数：左轮编码器、右轮编码器、Z轴陀螺仪
返回  值：转向控制PWM
**************************************************************************/
int turn(int encoder_left,int encoder_right,float gyro)//转向控制
{
	static float Turn_Target,Turn,Encoder_temp,Turn_Convert=0.9,Turn_Count;
	float Turn_Amplitude=50/Flag_sudu,Kp=52,Kd=0;     
	//=============遥控左右旋转部分=======================//
	if(1==Flag_Left||1==Flag_Right)                      //这一部分主要是根据旋转前的速度调整速度的起始速度，增加小车的适应性
	{
		if(++Turn_Count==1)
		Encoder_temp=myabs(encoder_left+encoder_right);
		Turn_Convert=50/Encoder_temp;
		if(Turn_Convert<0.6)Turn_Convert=0.6;
		if(Turn_Convert>3)Turn_Convert=3;
	}	
	else
	{
		Turn_Convert=0.9;
		Turn_Count=0;
		Encoder_temp=0;
	}			
	if(1==Flag_Left)	  Turn_Target-=Turn_Convert;
	else if(1==Flag_Right)Turn_Target+=Turn_Convert; 
	else Turn_Target=0;

	if(Turn_Target>Turn_Amplitude)  Turn_Target=Turn_Amplitude;    //===转向速度限幅
	if(Turn_Target<-Turn_Amplitude) Turn_Target=-Turn_Amplitude;
	if(Flag_Qian==1||Flag_Hou==1)  Kd=0.5;        
	else Kd=0;   //转向的时候取消陀螺仪的纠正 有点模糊PID的思想
	//=============转向PD控制器=======================//
	Turn=-Turn_Target*Kp-gyro*Kd;                 //===结合Z轴陀螺仪进行PD控制
	return Turn;
}

/**************************************************************************
函数功能：赋值给PWM寄存器
入口参数：左轮PWM、右轮PWM
返回  值：无
**************************************************************************/
void Set_Pwm(int moto1,int moto2)
{
	if(moto1>0)	AIN2=1, AIN1=0;
	else AIN2=0, AIN1=1;
	PWMA=myabs(moto1);
	if(moto2>0)	BIN1=1,	BIN2=0;
	else        BIN1=0,	BIN2=1;
	PWMB=myabs(moto2);
}
/**************************************************************************
函数功能：限制PWM赋值 
入口参数：无
返回  值：无
**************************************************************************/
void Xianfu_Pwm(void)
{	
	int Amplitude=6900;    //===PWM满幅是7200 限制在6900
	if(Flag_Qian==1)  Moto1+=DIFFERENCE;  //DIFFERENCE是一个衡量平衡小车电机和机械安装差异的一个变量。直接作用于输出，让小车具有更好的一致性。
	if(Flag_Hou==1)   Moto2-=DIFFERENCE;
	if(Moto1<-Amplitude) Moto1=-Amplitude;	
	if(Moto1>Amplitude)  Moto1=Amplitude;	
	if(Moto2<-Amplitude) Moto2=-Amplitude;	
	if(Moto2>Amplitude)  Moto2=Amplitude;
}
/**************************************************************************
函数功能：按键修改小车运行状态 
入口参数：无
返回  值：无
**************************************************************************/
void Key(void)
{	
	u8 tmp,tmp2;
	tmp=click_N_Double(50); 
	if(tmp==1)Flag_Stop=!Flag_Stop;//单击控制小车的启停
	else if(tmp==2)Flag_Show=!Flag_Show;//双击控制小车的显示状态
	tmp2=Long_Press(); 
	if(tmp2==1) Bi_zhang=!Bi_zhang;		//长按控制小车是否进入超声波避障模式 
}

/**************************************************************************
函数功能：异常关闭电机
入口参数：倾角和电压
返回  值：1：异常  0：正常
**************************************************************************/
uint8_t g_lift_motor_error = 0, g_right_motor_error = 0;
u8 Turn_Off(float angle, int voltage)
{
	u8 temp;
	//
	Output_FeedbackSig(0, &g_lift_motor_error, 0, 0, 0);
	Output_FeedbackSig(1, &g_right_motor_error, 0, 0, 0);

	if(angle < (-40+Zhongzhi) || angle > (40+Zhongzhi) || 1 == Flag_Stop || voltage < 1110 ||
		g_lift_motor_error >= 1 || g_right_motor_error >= 1)	//电池电压低于11.1V关闭电机
	{	                                                 		//===倾角大于40度关闭电机
		temp=1;                                            		//===Flag_Stop置1关闭电机
		AIN1=0;                                            
		AIN2=0;
		BIN1=0;
		BIN2=0;
	}
	else
	temp=0;
	return temp;			
}
	
/**************************************************************************
函数功能：获取角度 三种算法经过我们的调校，都非常理想 
入口参数：获取角度的算法 1：DMP  2：卡尔曼 3：互补滤波
返回  值：无
**************************************************************************/
void Get_Angle(u8 way)
{ 
	float Accel_Y,Accel_Angle,Accel_Z,Gyro_X,Gyro_Z;
	Temperature=Read_Temperature();      //===读取MPU6050内置温度传感器数据，近似表示主板温度。
	if(way==1)                           //===DMP的读取在数据采集中断读取，严格遵循时序要求
	{	
		Read_DMP();                      //===读取加速度、角速度、倾角
		Angle_Balance=-Roll;             //===更新平衡倾角
		Gyro_Balance=-gyro[0];            //===更新平衡角速度
		Gyro_Turn=gyro[2];               //===更新转向角速度
		Acceleration_Z=accel[2];         //===更新Z轴加速度计
	}			
	else
	{
		Gyro_X=(I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_XOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_XOUT_L);    //读取Y轴陀螺仪
		Gyro_Z=(I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_ZOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_GYRO_ZOUT_L);    //读取Z轴陀螺仪
		Accel_Y=(I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_YOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_YOUT_L); //读取X轴加速度计
		Accel_Z=(I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_ZOUT_H)<<8)+I2C_ReadOneByte(devAddr,MPU6050_RA_ACCEL_ZOUT_L); //读取Z轴加速度计
		if(Gyro_X>32768)  Gyro_X-=65536;                       //数据类型转换  也可通过short强制类型转换
		if(Gyro_Z>32768)  Gyro_Z-=65536;                       //数据类型转换
		if(Accel_Y>32768) Accel_Y-=65536;                      //数据类型转换
		if(Accel_Z>32768) Accel_Z-=65536;                      //数据类型转换
		Gyro_Balance=Gyro_X;                                   //更新平衡角速度
		Accel_Angle=atan2(Accel_Y,Accel_Z)*180/PI;             //计算倾角	
		Gyro_X=Gyro_X/16.4;                                    //陀螺仪量程转换	
		if(Way_Angle==2)Kalman_Filter(Accel_Angle,Gyro_X);	   //卡尔曼滤波	
		else if(Way_Angle==3)Yijielvbo(Accel_Angle,Gyro_X);    //互补滤波
		Angle_Balance=angle;                                   //更新平衡倾角
		Gyro_Turn=Gyro_Z;                                      //更新转向角速度
		Acceleration_Z=Accel_Z;                                //===更新Z轴加速度计	
	}
}
/**************************************************************************
函数功能：绝对值函数
入口参数：int
返回  值：unsigned int
**************************************************************************/
int myabs(int a)
{ 		   
	int temp;
	if(a<0)  temp=-a;  
	else temp=a;
	return temp;
}
/**************************************************************************
函数功能：检测小车是否被拿起
入口参数：int
返回  值：unsigned int
**************************************************************************/
int Pick_Up(float Acceleration,float Angle,int encoder_left,int encoder_right)
{ 		   
	static u16 flag,count0,count1,count2;
	if(flag==0)                                                                   //第一步
	{
		if(myabs(encoder_left)+myabs(encoder_right)<30)                         //条件1，小车接近静止
		count0++;
		else 
		count0=0;		
		if(count0>10)				
		flag=1,count0=0; 
	} 
	if(flag==1)                                                                  //进入第二步
	{
		if(++count1>200)       count1=0,flag=0;                                 //超时不再等待2000ms
		if(Acceleration>26000&&(Angle>(-20+Zhongzhi))&&(Angle<(20+Zhongzhi)))   //条件2，小车是在0度附近被拿起
		flag=2; 
	} 
	if(flag==2)                                                                  //第三步
	{
		if(++count2>100)       count2=0,flag=0;                                   //超时不再等待1000ms
		if(myabs(encoder_left+encoder_right)>135)                                 //条件3，小车的轮胎因为正反馈达到最大的转速   
		{
			flag=0;                                                                                     
			return 1;                                                               //检测到小车被拿起
		}
	}
	return 0;
}
/**************************************************************************
函数功能：检测小车是否被放下
入口参数：int
返回  值：unsigned int
**************************************************************************/
int Put_Down(float Angle,int encoder_left,int encoder_right)
{ 		   
	static u16 flag,count;	 
	if(Flag_Stop==0)                           //防止误检      
	return 0;	                 
	if(flag==0)                                               
	{
		if(Angle>(-10+Zhongzhi)&&Angle<(10+Zhongzhi)&&encoder_left==0&&encoder_right==0)         //条件1，小车是在0度附近的
		flag=1; 
	} 
	if(flag==1)                                               
	{
		if(++count>50)                                          //超时不再等待 500ms
		{
			count=0;flag=0;
		}
		if(encoder_left<-3&&encoder_right<-3&&encoder_left>-60&&encoder_right>-60)                //条件2，小车的轮胎在未上电的时候被人为转动  
		{
			flag=0;
			flag=0;
			return 1;                                             //检测到小车被放下
		}
	}
	return 0;
}


/**************************************************************************
函数功能：采集遥控器的信号
入口参数：无
返回  值：无
**************************************************************************/
void  Get_MC6(void)
{ 
	if(Flag_Left==0&&Flag_Right==0)
	{	
		if((Remoter_Ch1>1650&&Remoter_Ch1<2100)||(Remoter_Ch1>21650&&Remoter_Ch1<22100))	Flag_Qian=1,Flag_Hou=0,Flag_sudu=1;//////////////前
		else if((Remoter_Ch1<1350&&Remoter_Ch1>900) ||(Remoter_Ch1<21350&&Remoter_Ch1>20900))	Flag_Qian=0,Flag_Hou=1,Flag_sudu=1;//////////////后
		else if ((Remoter_Ch1>1350&&Remoter_Ch1<1650) ||(Remoter_Ch1>21350&&Remoter_Ch1<21650))	Flag_Qian=0,Flag_Hou=0;//////////////停
	}
	if(Flag_Qian==0&&Flag_Hou==0)
	{	
		if((Remoter_Ch2>1650&&Remoter_Ch2<2100)||(Remoter_Ch2>21650&&Remoter_Ch2<22100))Flag_Left=1,Flag_Right=0,Flag_sudu=1;//////////////左
		else if((Remoter_Ch2<1350&&Remoter_Ch2>900) ||(Remoter_Ch2<21350&&Remoter_Ch2>20900))Flag_Left=0,Flag_Right=1,Flag_sudu=1;//////////////右
		else if ((Remoter_Ch2>1350&&Remoter_Ch2<1650) ||(Remoter_Ch2>21350&&Remoter_Ch2<21650))Flag_Left=0,Flag_Right=0;//////////////停
	}
}	

