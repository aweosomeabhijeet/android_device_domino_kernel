/*
 * Asahi Kasei 3-axis Electronic Compass Sennsor Chip Driver 
 *
 * Copyright (C) 2008, Chi Mei Communication Systems, INC. (neillai@tp.cmcs.com.tw)
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 */
/* FIH, NicoleWeng, 2010/03/09 { */
#undef DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>

#include <linux/sched.h>
#include <linux/freezer.h>
#include <asm/ioctl.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <mach/gpio.h>
//#include <linux/gpio.h>

#include "LPSCM3602.h"
#include <linux/input.h>
#include <linux/miscdevice.h>
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
//#include <linux/elan_i2c.h>
#include <linux/st1332_i2c.h>              
#include <mach/vreg.h>  //Add for VREG_WLAN power in, 07/08
#include <mach/sc668.h> //Add for V_LCM_2.8V
#include <mach/msm_smd.h>
#include <linux/delay.h>
//read NV
u8 CM3623_PS_THD_VAL = 0x06;
#include "../../../arch/arm/mach-msm/proc_comm.h"
void get_proximity_threshold_from_NV(void){	
	unsigned bufferCAL[10];
	bufferCAL[0] = 8048;
	if(proc_comm_read_nv((unsigned*)bufferCAL)==0)
	{
		CM3623_PS_THD_VAL = bufferCAL[0]&0xff;
		printk("[cm3623]read NV =%d, CM3623_PS_THD_VAL = %d\n",bufferCAL[0], CM3623_PS_THD_VAL);
	}
	else
	{
		printk("[cm3623]read NV fail\n");
	}
}
void set_proximity_threshold_to_NV(unsigned nv_value){	
	unsigned NVdata[1];
	NVdata[0] = 8048;
	NVdata[1] = nv_value;
    proc_comm_write_nv((unsigned *) NVdata);	
}

//debug mask.
#include <linux/cmdbgapi.h>
#include <mach/msm_iomap.h>
//#include <mach/msm_smd.h>
static uint32_t cm3602_debug_mask = 0;
module_param_named(debug_mask, cm3602_debug_mask, uint, 0644);

struct cm3623_platform_data *cm3623;

static struct i2c_client *ALS1_client = NULL;
static struct i2c_client *ALS2_client = NULL;
static struct i2c_client *PS1_client = NULL;
static struct i2c_client *PS2_client = NULL;
static struct i2c_client *ARA_client = NULL;

#ifndef FTM
static int flag_cm3623_irq=0;
static int cm3623_irq=-1;
#endif
static char enablePS = 0;
static char enableALS = 0;
int cm3623_pid =0;
struct task_struct * cm3623_task_struct = NULL;
static int isFQC_Testing=0;

u8 als_msb = 0;
u8 als_lsb = 0;
//u16 als_data = 0;
int PID = 0;

#define myTest
#define DRIVER_NAME	"cm3623"

#if (0)
#define my_debug(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#else
#define my_debug(fmt, ...) \
	fih_printk(cm3602_debug_mask, FIH_DEBUG_ZONE_G0, pr_fmt(fmt), ##__VA_ARGS__)
	//({ if (0) printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__); 0; })	
#endif



#define DBG(f, x...) \
	my_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)
/*	
#define DBG(f, x...) \
	printk(DRIVER_NAME " [%s()]: " f, __func__,## x)
*/



/* FIH;Tiger;2010/4/10 { */
#ifndef FTM
#include <linux/moduleparam.h>

static int set_fih_in_call(const char *val, struct kernel_param *kp)
{
	char *endp;
	int  l;
	int  rv = 0;

	if (!val)
		return -EINVAL;
	l = simple_strtoul(val, &endp, 0);
	if (endp == val)
		return -EINVAL;

	*((int *)kp->arg) = l;

	if(l == 0) {
		notify_from_proximity(0);
	}

	return rv;
}

static int fih_in_call = 0; 
module_param_call(fih_in_call, set_fih_in_call, param_get_int, &fih_in_call, 0644);
#endif
static char fih_proximity_level = -1;
/* } FIH;Tiger;2010/4/10 */



static int cm3623_write_byte(struct i2c_client *client, unsigned char data)
{
	int ret=-1;
	unsigned char command;
	command = data;
	DBG("reg:0x%x, data:0x%x\n", client->addr, data );
	ret = i2c_master_send( client, &command, 1);
	if(ret !=1)
	{	
		//DBG("write %s fail(ret:%d)\n",client->name, ret) ;
		printk(KERN_ERR "%s: write %s fail(ret:%d)\n", __func__, client->name, ret);
		return -EIO;
	}
	return 0;	
}

/*
static unsigned char cm3623_read_byte(struct i2c_client *client)
{
	int ret = -1;
	unsigned char data1;	
	ret = i2c_master_recv(client, &data1, 1);
  	if(ret !=1)
	{	
		DBG("read %s data fail(ret:%d)\n",client->name, ret) ;
		return -EIO;
	}
	return data1;
}
*/
int cm3623_onoff_ALS(unsigned int cmd)
{
	u8 als_reg_set;

	if(POWER_OFF==cmd)
	{
		enableALS = 0;	
		als_reg_set =	(CM3623_ALSREG_INITIAL_VAL | 0x01);		

		/*Write shut down command*/
		cm3623_write_byte(ALS1_client, als_reg_set);		
		DBG( "CM3623_PSREG_VAL: 0x%x\n", CM3623_ALSREG_INITIAL_VAL|0x01);	
		DBG(" ALS off !\n");
		return 1;
	}
	else if(POWER_ON==cmd)
	{
    	enableALS = 1;	
    	als_reg_set =	CM3623_ALSREG_INITIAL_VAL ;

		/*Write initial command*/
		cm3623_write_byte(ALS1_client, als_reg_set);		
		DBG( "CM3623_PSREG_VAL: 0x%x\n", CM3623_ALSREG_INITIAL_VAL);	
		DBG(" ALS on !\n");
		return 1;
	}else
		return -EINVAL;
}
int cm3623_onoff_PS(unsigned int cmd)
{
	if(POWER_OFF==cmd)
	{			
		if(enablePS)
		{
			fih_proximity_level = -1;	
			cm3623_pid = 0;
			enablePS = 0;	
			/*Write shut down command*/
			cm3623_write_byte(PS1_client, CM3623_PSREG_INITIAL_VAL|0x01);			
			DBG( "CM3623_PSREG_VAL: 0x%x\n", CM3623_PSREG_INITIAL_VAL|0x01);	
#ifndef FTM		
			if(flag_cm3623_irq==1)
			{				
				disable_irq(cm3623_irq);
				flag_cm3623_irq=0;
				DBG( "disable_irq cm3623_irq=%d\n", cm3623_irq );
			}	
#endif			
			DBG(" PS off !\n");
		}
		else
		{
			DBG(" PS already off !\n");
		}
		return 1;
	}
	else if(POWER_ON==cmd)
	{
		if(!enablePS)
		{
			cm3623_task_struct = current;
			cm3623_pid = current->pid;
			enablePS = 1;
			DBG( "cm3623_pid: %d\n", cm3623_pid);
		
			cm3623_write_byte(PS1_client, CM3623_PSREG_INITIAL_VAL);
			DBG( "CM3623_PSREG_VAL: 0x%x\n", CM3623_PSREG_INITIAL_VAL);	
#ifndef FTM				
			if(flag_cm3623_irq==0)
			{
				enable_irq(cm3623_irq);	
				flag_cm3623_irq=1;				
				DBG("enable_irq cm3623_irq=%d\n", cm3623_irq );
			}
#endif    	
			DBG(" PS on !\n");
		}
		else
		{
			DBG(" PS already on !\n");
		}
		return 1;
	}else
		return -EINVAL;
}


int cm3623_onoff_control(unsigned int cmd)
{
	u8 als_reg_set, ps_reg_set;

	if(POWER_OFF==cmd)
	{
		als_reg_set =	(CM3623_ALSREG_INITIAL_VAL | 0x01);
		ps_reg_set = (CM3623_PSREG_INITIAL_VAL | 0x01);

		/*Write shut down command*/
		cm3623_write_byte(ALS1_client, als_reg_set);
		cm3623_write_byte(PS1_client, ps_reg_set);	
		DBG("cm3623 shut down !\n");
		return 1;
	}
	else if(POWER_ON==cmd)
	{
    	als_reg_set =	CM3623_ALSREG_INITIAL_VAL ;
		ps_reg_set = CM3623_PSREG_INITIAL_VAL ;

		/*Write initial command*/
		cm3623_write_byte(ALS1_client, als_reg_set);
		cm3623_write_byte(PS1_client, ps_reg_set);

		DBG("cm3623 wake up !\n");
		return 1;
	}else
		return -EINVAL;
}




#ifndef FTM
static irqreturn_t cm3623_isr( int irq, void * dev_id)
{
	int level;	
	gpio_direction_input(CM3623_PS_GPIO_OUT);
	level = gpio_get_value(CM3623_PS_GPIO_OUT);
	
	fih_proximity_level = level;		
	if(fih_in_call) {
		if(fih_proximity_level == 1) {
			notify_from_proximity(0);
		}
		else if(fih_proximity_level == 0) {
			notify_from_proximity(1);
		}
	}	
	
	/* implement fast path for ALS/PS */
	
	if(cm3623_pid) {
		struct siginfo info;
		info.si_signo = SIGUSR1;
		info.si_code = (int)fih_proximity_level;

		printk(KERN_INFO "Send PS signal to PID (%d)\n", cm3623_pid);		
		if(cm3623_task_struct != NULL)
		{	
			DBG( "Send PS signal to PID (%d)\n", cm3623_pid);		
			send_sig_info(SIGUSR1, &info, cm3623_task_struct);
		}
		else
		{
			DBG( "cm3623_task_struct is NULL\n");
		}
	}
	
	DBG("cm3623_isr is called! id=[%d], PS_out=%d\n",irq ,level);  
  
	return IRQ_HANDLED;
}
#endif

/* 
initial cm3623 chip, and check cm3623 is available
*/

static int cm3623_initchip(void)
{	  	
  	int count = 0;
  	unsigned char data1;   	  	
  	int ret =-1;
  	u8 als_reg_set=0x00;
  	u8 ps_thd=0x00;
  	u8 ps_reg_set=0x00;
  	DBG("\n");

  	while(!gpio_get_value(CM3623_PS_GPIO_OUT))
  	{  		
  		ret = i2c_master_recv(ARA_client, &data1, 1);
  		if(ret !=1)
		{	
			//DBG("read ARA fail(ret:%d)\n",ret) ;
			printk(KERN_ERR "%s: read ARA fail(ret:%d)\n", __func__,ret);
			if(count >= 5) return -EIO;
		}  
		
  		DBG("read ARA %d)\n",count++) ;  		
 	}
 	DBG("CM3623_PS_GPIO_OUT:%d)\n", gpio_get_value(CM3623_PS_GPIO_OUT)) ;
 	
  als_reg_set = 1 << CM3623_ALS_GAIN1_OFFSET /* ALS threshold window setting: +/- 1024 STEP */
           | 1 << CM3623_ALS_GAIN0_OFFSET
           | 0 << CM3623_ALS_THD1_OFFSET
           | 1 << CM3623_ALS_THD0_OFFSET  
           | 0 << CM3623_ALS_IT1_OFFSET		/* ALS intergration time setting: 100 ms 	*/
           | 0 << CM3623_ALS_IT0_OFFSET   
 //          | 0 << CM3623_ALS_WDM_OFFSET		/* ALS data mode setting: Byte mode 	*/
		   | 1 << CM3623_ALS_WDM_OFFSET		/* ALS data mode setting: Word mode 	*/
           | 1 << CM3623_ALS_SD_OFFSET;		/* ALS shut down enable */

  ps_thd = CM3623_PS_THD_VAL;		/* PS threshold value for PS interruption activity*/

  ps_reg_set = 0 << CM3623_PS_DR1_OFFSET      	/* IR LED ON/Off duty ratio setting: 1/160 	*/
          | 0 << CM3623_PS_DR0_OFFSET
          | 1 << CM3623_PS_IT1_OFFSET      	/* PS intergration time setting: 1.875 T 	*/
          | 1 << CM3623_PS_IT0_OFFSET
          | 0 << CM3623_PS_INTALS_OFFSET   	/* ALS INT disable	*/
          | 0 << CM3623_PS_INTPS_OFFSET    	/* PS INT disable 	*/
          | 0 << CM3623_PS_RES_OFFSET
          | 1 << CM3623_PS_SD_OFFSET;      	/* PS shut down enable */

  DBG("init_data=[0x%X] als_data=[0x%X] ps_thd=[0x%X] ps_data=[0x%X]\n", 
          CM3623_INITIAL_POL_DATA, als_reg_set, ps_thd, ps_reg_set);
//initial   
  	cm3623_write_byte(ALS2_client, CM3623_INITIAL_POL_DATA);//simple PS Logic Hight/Low mode
//init ALS  	
  	cm3623_write_byte(ALS1_client, als_reg_set);
//Set the threshold window for PS interruption activity as minimum value  	
  	cm3623_write_byte(PS2_client, CM3623_PS_THD_VAL);
//init ps  	
  	cm3623_write_byte(PS1_client, ps_reg_set);
  	
  	/*
  	command = CM3623_INITIAL_POL_DATA; 
  	ret = i2c_master_send(ALS2_client, &command, 1);
	if(ret !=1)
	{	
		DBG("write ALS2 fail(ret:%d)\n",ret) ;
		return -EIO;
	}	
//set ALS,PS	
	command = 0x44;
  	ret = i2c_master_send(ALS1_client, &command, 1);
	if(ret !=1)
	{	
		DBG("write ALS1 fail(ret:%d)\n",ret) ;
		return -EIO;
	}
	
	command = 0x05;
  	ret = i2c_master_send(PS2_client, &command, 1);
	
	if(ret !=1)
	{	
		DBG("write PS2 fail(ret:%d)\n",ret) ;
		return -EIO;
	}
	
	command = 0x00;
  	ret = i2c_master_send(PS1_client, &command, 1);
	
	if(ret !=1)
	{	
		DBG("write PS1 fail(ret:%d)\n",ret) ;
		return -EIO;
	}
	
	*/

	DBG("initial chip ok!\n") ;

	return 0;
}



static int cm3623_open(struct inode *inode, struct file *file)
{
		DBG("\n");
		return 0;
}

static ssize_t cm3623_read(struct file *file, char __user *buffer, size_t size, loff_t *f_ops)
{	
	char *st;
	unsigned char data1;
	int ret = -1;
	int level=1;
	u16 als_data;
	st=kmalloc(sizeof(char)*2,GFP_KERNEL);		 
	//read ALS data
	if(enableALS)
	{
		ret = i2c_master_recv(ALS1_client, &data1, 1);
		if(ret !=1)
		{	
			DBG("read LS data fail(ret:%d)\n",ret) ;			
		}else{
			//DBG("READ_LS als_msb=%d\n", (int)data1);
			als_msb = data1;			
		}
		ret = i2c_master_recv(ALS2_client, &data1, 1);
		if(ret !=1)
		{	
			DBG("read LS data fail(ret:%d)\n",ret) ;			
		}else{
			//DBG("READ_LS als_lsb=%d\n", (int)data1);
			als_lsb = data1;			
		}
		als_data = als_msb << 8;
		als_data = als_data | als_lsb;
		//DBG("READ_LS als_data=%d\n", als_data);
		fih_printk(cm3602_debug_mask, FIH_DEBUG_ZONE_G1,"READ_LS als_data=%d\n", als_data);
		//als_data = (int)als_data /2;
		if(als_data <= 255)
			st[0] = (char)als_data;	
		else	
			st[0] = 255;
	}
	else
	{
		st[0]=(char)-1;
	}
	
	if(fih_proximity_level == (char)-1) 
	{		
		gpio_direction_input(CM3623_PS_GPIO_OUT);
		level = gpio_get_value(CM3623_PS_GPIO_OUT);
		fih_proximity_level = level;
	}
	else 
	{
		level = fih_proximity_level;
	}	
	if(enablePS) {
		st[1] = level;
	}
	else {
		st[1] = 1;
	}
	fih_printk(cm3602_debug_mask, FIH_DEBUG_ZONE_G1,"cm3623_read: PS level = %d\n", st[1]);
	fih_printk(cm3602_debug_mask, FIH_DEBUG_ZONE_G1,"cm3623_read: ALS level = %d\n", st[0]);
	//DBG( KERN_INFO "cm3623_read: PS level = %d\n", st[1]);	
	//DBG( KERN_INFO "cm3623_read: ALS level = %d\n", st[0]);
	if(copy_to_user(buffer, st, sizeof(char)*2)){
		kfree(st);
		//DBG( "[line:%d] copy_to_user failed\n",  __LINE__ );
		printk(KERN_ERR "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}
	kfree(st);
	return 0; 
}
#ifdef myTest
static ssize_t LPS_test(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	unsigned LogRun,arg;	
	u8 val = -1;  	
	int ret = -1;		
	unsigned char data1;
	u8 als_msb, als_lsb;
	u16 als_data;
	int i = 0;
	//unsigned char command;
	sscanf(buf, "0x%x 0x%x\n", &LogRun, &arg);		
	dev_dbg(dev, "%s: 0x%x 0x%x\n", __func__, count, LogRun);		
	DBG("%d 0x%x 0x%x\n", count, LogRun, arg);	
	
	switch (LogRun)
	{
		case INIT_CHIP:			
			ret = cm3623_initchip();
			enableALS = 0;
			enablePS = 0;
			DBG("INIT_CHIP ret=%d\n", ret);
			break;
			//return ret;		
		case ACTIVE_PS:
			cm3623_onoff_PS(POWER_ON);
			DBG("ACTIVE_PS\n");
			break;
		case ACTIVE_LS:
			cm3623_onoff_ALS(POWER_ON);
			DBG("ACTIVE_LS\n");
			break;
		case INACTIVE_PS:		
			cm3623_onoff_PS(POWER_OFF);
			DBG("INACTIVE_PS\n");
			break;
		case INACTIVE_LS:
			cm3623_onoff_ALS(POWER_OFF);
			DBG("INACTIVE_LS\n");
			break;
		case READ_PS:			
			if(!enablePS) cm3623_onoff_PS(POWER_ON);
			val = gpio_get_value(CM3623_PS_GPIO_OUT);
			DBG("READ_PS value=%d\n", (int)val);
			break;
			//return (int)val;
		case READ_LS:				
			if(!enableALS) cm3623_onoff_ALS(POWER_ON);
			ret = i2c_master_recv(ALS1_client, &data1, 1);
  			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}else{
				DBG("READ_LS als_msb=%d\n", (int)data1);
				als_msb = data1;
			}
			ret = i2c_master_recv(ALS2_client, &data1, 1);
			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}else{
				DBG("READ_LS als_lsb=%d\n", (int)data1);
				als_lsb = data1;
			}
			als_data = als_msb << 8;
	   		als_data = als_data | als_lsb;
			DBG("READ_LS value=%d\n", als_data);				
			break;
			//return (int)data1;
		case READ_PS_DATA:			
			if(!enablePS) cm3623_onoff_PS(POWER_ON);
			ret = i2c_master_recv(PS1_client, &data1, 1);
  			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}
			DBG("READ_LS value=%d\n", (int)data1);				
			break;
			//return (int)data1;
		case INACTIVE_ALL:
			cm3623_onoff_ALS(POWER_OFF);
			cm3623_onoff_PS(POWER_OFF);	
			break;
		case PS_STATUS:		
			return 1;		
		case CM3602_FQC_Testing:
			isFQC_Testing=arg;
			DBG(" isFQC_Testing = %d\n", isFQC_Testing );
			return 0;
		case SHOW_als_reg_set:
			DBG(" SHOW_als_reg_set = %d\n", CM3623_ALSREG_INITIAL_VAL );
			break;
			//return CM3623_ALSREG_INITIAL_VAL;
		case SHOW_ps_thd:
			DBG(" SHOW_ps_thd = %d\n", CM3623_PS_THD_VAL );
			break;
			//return CM3623_PS_THD_VAL;
		case SHOW_ps_reg_set:
			DBG(" SHOW_ps_reg_set = %d\n", CM3623_PSREG_INITIAL_VAL );
			break;
			//return CM3623_PSREG_INITIAL_VAL;	
		case SHOW_init_reg_set:
			DBG(" SHOW_init_reg_set = %d\n", CM3623_INITIAL_POL_DATA );
			break;
			//return CM3623_INITIAL_POL_DATA;
		case SET_als_reg:
			ret=cm3623_write_byte(ALS1_client, arg);
			if(ret==0) DBG("SET_als_reg: 0x%x, 0x%x\n",ALS1_client->addr,arg);
			break;
			//return ret;
		case SET_ps_thd:
			ret=cm3623_write_byte(PS2_client, arg);
			if(ret==0) DBG("SET_ps_thd: 0x%x, 0x%x\n",PS2_client->addr,arg);
			break;			
			//return ret;
		case SET_ps_reg:
			ret=cm3623_write_byte(PS1_client, arg);
			if(ret==0) DBG("SET_ps_reg: 0x%x, 0x%x\n",PS1_client->addr,arg);
			break;
			//return ret;
		case SET_init_reg:
			ret=cm3623_write_byte(ALS2_client, arg);
			if(ret==0) DBG("SET_init_reg: 0x%x, %d\n",ALS2_client->addr,arg);
			break;
			//return ret;
		case RESET:
			ret=cm3623_write_byte(ALS2_client, CM3623_INITIAL_POL_DATA);
			ret=cm3623_write_byte(ALS1_client, CM3623_ALSREG_INITIAL_VAL);			
			ret=cm3623_write_byte(PS2_client, CM3623_PS_THD_VAL);			
			ret=cm3623_write_byte(PS1_client, CM3623_PSREG_INITIAL_VAL);
			break;
			//return ret;		
		case 0x0B14:				
			for(i=0;i<10;i++){
			ret = i2c_master_recv(ALS1_client, &data1, 1);
  			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}else{
				//DBG("READ_LS als_msb=%d\n", (int)data1);
				als_msb = data1;
			}
			ret = i2c_master_recv(ALS2_client, &data1, 1);
			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}else{
				//DBG("READ_LS als_lsb=%d\n", (int)data1);
				als_lsb = data1;
			}
			als_data = als_msb << 8;
	   		als_data = als_data | als_lsb;
			DBG("READ_LS value=%d\n", als_data);
			msleep(500);
			}			
			break;
		default:
			DBG( "Proximity sensor ioctl default : NO ACTION!!\n");
	}

	return count;
}

DEVICE_ATTR(LPS, 0644, NULL, LPS_test);

#endif
//static ssize_t cm3623_write(struct file *file, char __user *buffer, size_t size, loff_t *f_ops)
//{
//	DBG("\n");
//	return 0;
//}

static int cm3623_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;
	u8 val;
	unsigned char data1;
	DBG("cmd=0x%x, arg=0x%x\n", cmd, (unsigned int)arg);
	switch (cmd)
	{
		case INIT_CHIP:			
			ret = cm3623_initchip();
			enableALS = 0;
			enablePS = 0;
			DBG("INIT_CHIP ret=%d\n", ret);
			return ret;		
		case ACTIVE_PS:
			cm3623_onoff_PS(POWER_ON);
			DBG("ACTIVE_PS\n");
			break;
		case ACTIVE_LS:
			cm3623_onoff_ALS(POWER_ON);
			DBG("ACTIVE_LS\n");
			break;
		case INACTIVE_PS:		
			cm3623_onoff_PS(POWER_OFF);
			DBG("INACTIVE_PS\n");
			break;
		case INACTIVE_LS:
			cm3623_onoff_ALS(POWER_OFF);
			DBG("INACTIVE_LS\n");
			break;
		case READ_PS:			
			val = gpio_get_value(CM3623_PS_GPIO_OUT);
			DBG("READ_PS value=%d\n", (int)val);
			return (int)val;
		case READ_LS:			
			ret = i2c_master_recv(ALS1_client, &data1, 1);
			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;			
			}else{
				//DBG("READ_LS als_msb=%d\n", (int)data1);
				als_msb = data1;			
			}
			ret = i2c_master_recv(ALS2_client, &data1, 1);
			if(ret !=1)
			{	
				DBG("read LS data fail(ret:%d)\n",ret) ;			
			}else{
				//DBG("READ_LS als_lsb=%d\n", (int)data1);
				als_lsb = data1;			
			}
			data1 = als_msb << 8;
			data1 = data1 | als_lsb;
			DBG("READ_LS value=%d\n", (int)data1);				
			return (int)data1;
		case READ_PS_DATA:			
			ret = i2c_master_recv(PS1_client, &data1, 1);
  			if(ret !=1)
			{	
				DBG("read PS data fail(ret:%d)\n",ret) ;
				return -EIO;
			}
			DBG("READ_PS value=%d\n", (int)data1);				
			return (int)data1;
		case INACTIVE_ALL:
			cm3623_onoff_ALS(POWER_OFF);
			cm3623_onoff_PS(POWER_OFF);	
			break;
		case PS_STATUS:	//if device is not support proximity, return 0.(@hal_yamaha.h)	
			return 1;		
		case CM3602_FQC_Testing:
			isFQC_Testing = arg;
			DBG(" isFQC_Testing = %d\n", isFQC_Testing );
			return 0;
		case SHOW_als_reg_set:
			return CM3623_ALSREG_INITIAL_VAL;
		case SHOW_ps_thd:
			return CM3623_PS_THD_VAL;
		case SHOW_ps_reg_set:
			return CM3623_PSREG_INITIAL_VAL;	
		case SHOW_init_reg_set:
			return CM3623_INITIAL_POL_DATA;
		case SET_als_reg:
			ret=cm3623_write_byte(ALS1_client, arg);
			return ret;
		case SET_ps_thd:
			ret=cm3623_write_byte(PS2_client, arg);
			return ret;
		case SET_ps_reg:
			ret=cm3623_write_byte(PS1_client, arg);
			return ret;
		case SET_init_reg:
			ret=cm3623_write_byte(ALS2_client, arg);
			return ret;
		case RESET:
			ret=cm3623_write_byte(ALS2_client, CM3623_INITIAL_POL_DATA);
			ret=cm3623_write_byte(ALS1_client, CM3623_ALSREG_INITIAL_VAL);			
			ret=cm3623_write_byte(PS2_client, CM3623_PS_THD_VAL);			
			ret=cm3623_write_byte(PS1_client, CM3623_PSREG_INITIAL_VAL);
			return ret;		

		default:
			DBG( "Proximity sensor ioctl default : NO ACTION!!\n");
			return -EINVAL;
	}

	return 0;
}
static int cm3623_release(struct inode *inode, struct file *filp)
{
    DBG("\n");
	return 0;
}

static const struct file_operations cm3623_dev_fops = {
		.owner = THIS_MODULE,
	.open = cm3623_open,
	.read = cm3623_read,
	//.write = cm3623_write,
	.ioctl = cm3623_ioctl,
	.release = cm3623_release,
};
static struct miscdevice cm3623_dev =
{
         .minor = MISC_DYNAMIC_MINOR,
         .name = "cm3602_alsps",//reserve the file name for FTM can control
		 .fops = &cm3623_dev_fops,
};



static int ALS1_remove(struct i2c_client *client)
{
	/*
	int irq;

	if((irq = cm3623_irq)>0)
		free_irq(cm3623_irq, NULL);

	kfree(cm3623);
	i2c_set_clientdata(client, NULL);
	*/
	return 0;
	

}

static int ALS1_suspend(struct i2c_client *client, pm_message_t state)
{

	return 0;

}




static int ALS1_resume(struct i2c_client *client)
{
	
	return 0;

}
static int ALS1_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{		
	int ret = -1;
	struct vreg *vreg_rfrx2;  //Add for VREG_IR_LED power in
  	struct vreg *vreg_msmp;  //Add for VREG_MSMP power in  	
	struct vreg *vreg_wlan;  //Add for VREG_WLAN power in 	
	
  	//DBG("(addr:0x%x)\n",client->addr);
	//DBG("(name:%s)\n",client->name);	
	printk(KERN_INFO "%s: (addr:0x%x)\n", __func__,client->addr);
	printk(KERN_INFO "%s: (name:%s)\n", __func__,client->name);
	
	ALS1_client = client;	
	PID=fih_read_product_id_from_orighwid( );
	DBG( "PID : %d, Project_DQD:%d\n", PID, Project_DQD);	
			
	vreg_msmp = vreg_get(0, "msmp");
		if (!vreg_msmp) {
		    DBG( "%s: vreg MSMP get failed\n", __func__);
			return -EIO;
	    }
	 ret = vreg_enable(vreg_msmp);
	    if (ret) {
		    DBG( "%s: vreg MSMP enable failed (%d)\n",__func__, ret);		        
			return -EIO;
	    }
	if(PID >= Project_DPD && PID <= Project_FAD)	
	{
		while(!sc668_get_chip_state( )){
	    	DBG("wait sc688 enable");
	    }
	    ret = sc668_LDO_setting(1, LDO_VOLTAGE_2_8, client->name);
	    if (ret) {
		    DBG( "%s: LED CATHODE voltage setting failed (%d)\n",  __func__, ret);
			return -EIO;
	    }
	}else{
		if(PID == Project_DQE){
			vreg_wlan = vreg_get(0, "wlan");
			if (!vreg_wlan) {
				DBG( "%s: vreg wlan get failed\n", __func__);
				return -EIO;
			}
			ret = vreg_enable(vreg_wlan);
			if (ret) {
				DBG( "%s: vreg wlan enable failed (%d)\n",
					__func__, ret);
			return -EIO;
			}
		}else{		
			vreg_rfrx2 = vreg_get(0, "rfrx2");
			if (!vreg_rfrx2) {
				DBG( "%s: vreg rfrx2 get failed\n", __func__);
				return -EIO;
			}
			ret = vreg_enable(vreg_rfrx2);
			if (ret) {
				DBG( "%s: vreg rfrx2 enable failed (%d)\n",
					__func__, ret);
			return -EIO;
			}
		}
	}
	
	return 0;

}


static const struct i2c_device_id ALS1_id[] = {
	{ "ALS1", 0 },
	{ }
};

static struct i2c_driver ALS1_driver = {
	.driver = {
		.name	= "ALS1",
	},

	.probe		= ALS1_probe,
	.remove		= ALS1_remove,
    .id_table   = ALS1_id,
	.suspend		= ALS1_suspend,
	.resume		= ALS1_resume,
};









static int ALS2_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	//DBG("(addr:0x%x)\n",client->addr);
	//DBG("(name:%s)\n",client->name);	
	printk(KERN_INFO "%s: (addr:0x%x)\n", __func__,client->addr);
	printk(KERN_INFO "%s: (name:%s)\n", __func__,client->name);

	ALS2_client = client;	

	return 0;

}

static int ALS2_remove(struct i2c_client *client)
{
	//i2c_detach_client(client);
	return 0;
}

static int ALS2_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int ALS2_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ALS2_id[] = {
	{ "ALS2", 0 },
	{ }
};

static struct i2c_driver ALS2_driver = {
	.probe = ALS2_probe,
	.remove = ALS2_remove,
	.suspend	= ALS2_suspend,
	.resume		= ALS2_resume,
	.id_table = ALS2_id,
	.driver = {
		   .name = "ALS2",
		   },
};

static int PS1_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	//DBG("(addr:0x%x)\n",client->addr);
	//DBG("(name:%s)\n",client->name);	
	printk(KERN_INFO "%s: (addr:0x%x)\n", __func__,client->addr);
	printk(KERN_INFO "%s: (name:%s)\n", __func__,client->name);

	PS1_client = client;	

	return 0;

}

static int PS1_remove(struct i2c_client *client)
{
	//i2c_detach_client(client);
	return 0;
}

static int PS1_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int PS1_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id PS1_id[] = {
	{ "PS1", 0 },
	{ }
};

static struct i2c_driver PS1_driver = {
	.probe = PS1_probe,
	.remove = PS1_remove,
	.suspend	= PS1_suspend,
	.resume		= PS1_resume,
	.id_table = PS1_id,
	.driver = {
		   .name = "PS1",
		   },
};



static int PS2_remove(struct i2c_client *client)
{
	//i2c_detach_client(client);
	return 0;
}
static int PS2_suspend(struct device *dev)
{
	DBG("\n");
	enable_irq_wake(MSM_GPIO_TO_INT(CM3623_PS_GPIO_OUT));
	/*
	int ret = -1;
	struct vreg *vreg_rfrx2;  //Add for VREG_WLAN power in, 07/08
  	struct vreg *vreg_msmp;  //Add for VREG_MSMP power in, 07/08  	
  	DBG("(addr:0x%x)\n",client->addr);
	DBG("(name:%s)\n",client->name);

	
  	vreg_rfrx2 = vreg_get(0, "rfrx2");
	    if (!vreg_rfrx2) {
		    DBG( "%s: vreg rfrx2 get failed\n", __func__);
		return -EIO;
	    }
	    vreg_msmp = vreg_get(0, "msmp");
		if (!vreg_msmp) {
		    DBG( "%s: vreg MSMP get failed\n", __func__);
		return -EIO;
	    }
       
	    ret = vreg_enable(vreg_rfrx2);
	    if (ret) {
		    DBG( "%s: vreg rfrx2 enable failed (%d)\n",
		        __func__, ret);
		return -EIO;
	    }
	     ret = vreg_enable(vreg_msmp);
	    if (ret) {
		    DBG( "%s: vreg MSMP enable failed (%d)\n",
		        __func__, ret);
		return -EIO;
	    }
	    */
	return 0;
}
static int PS2_resume(struct device *dev)

{
	DBG("\n");
	disable_irq_wake(MSM_GPIO_TO_INT(CM3623_PS_GPIO_OUT));
	return 0;
}
static int PS2_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	
	int ret = -EINVAL;	
	
	//DBG("(addr:0x%x)\n",client->addr);
	//DBG("(name:%s)\n",client->name);	
	printk(KERN_INFO "%s: (addr:0x%x)\n", __func__,client->addr);
	printk(KERN_INFO "%s: (name:%s)\n", __func__,client->name);
	
	/* allocate memory */
	
	cm3623 = kzalloc(sizeof(struct cm3623_platform_data), GFP_KERNEL);
	if (!cm3623){
		printk(KERN_ERR "%s: allocate memory fail\n", __func__);
		return -ENOMEM;
	}
	memset(cm3623, 0, sizeof(struct cm3623_platform_data));
	
	PS2_client = client;
	
	dev_set_drvdata (&client->dev, cm3623);
	
#ifdef myTest

	ret = device_create_file(&client->dev, &dev_attr_LPS);		
	if (ret < 0)	
	{		
		dev_err(&client->dev, "%s: Create LPS attribute \"LPS\" failed!! <%d>", __func__, ret);	
		printk(KERN_ERR "%s: Create LPS attribute \"LPS\" failed!! <%d>\n", __func__, ret);
	}
#endif			
	
	if (0 != misc_register(&cm3623_dev))
	{
		//DBG( "cm3623_dev register failed.\n");
		printk(KERN_ERR "%s: cm3623_dev register failed\n", __func__);
		return 0;
	}
	else
	{
		DBG( "cm3623_dev register ok.\n");
	}
	ret = gpio_tlmm_config(GPIO_CFG(CM3623_PS_GPIO_OUT, 0, GPIO_INPUT, GPIO_NO_PULL/*GPIO_PULL_DOWN*/, GPIO_2MA), GPIO_ENABLE);
	if (ret) {
		//DBG( "CM3623_PS_GPIO_OUT config failed : gpio_tlmm_config=%d\n", ret);	
		printk(KERN_ERR "%s: CM3623_PS_GPIO_OUT config failed : gpio_tlmm_config=%d\n", __func__, ret);		
		return -EIO;
	}
		
	get_proximity_threshold_from_NV();
	ret = cm3623_initchip();
#ifndef FTM 
	if (ret < 0)
		return -EIO;	
	cm3623_irq = MSM_GPIO_TO_INT(CM3623_PS_GPIO_OUT);
	ret = request_irq(cm3623_irq, cm3623_isr, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, "cm3623", NULL); 
	//ret = request_irq(cm3623_irq, cm3623_isr, IRQF_TRIGGER_LOW , "cm3623", NULL); 
	if (ret) 
	{
		//DBG(  "Can't allocate irq %d\n",ret);
		printk(KERN_ERR "%s: Can't allocate irq %d\n", __func__, ret);
		free_irq(cm3623_irq, NULL);
	}
	DBG(  "[line:%d] Requst PRO_INT pin (GPIO%d)client->irq:%d\n", __LINE__ , CM3623_PS_GPIO_OUT, cm3623_irq);
	
	disable_irq(cm3623_irq);
	flag_cm3623_irq=0;
#endif
	return 0;

}
static const struct i2c_device_id PS2_id[] = {
	{ "PS2", 0 },
	{ }
};

static struct dev_pm_ops cm3623_alsps_pm_ops = {
	.suspend        = PS2_suspend,
	.resume         = PS2_resume,
};

static struct i2c_driver PS2_driver = {
	.probe = PS2_probe,
	.remove = PS2_remove,
	.id_table = PS2_id,
	.driver = {
		.name = "PS2",
#ifdef CONFIG_PM
       .pm = &cm3623_alsps_pm_ops,
#endif
	},
};

static int ARA_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	DBG("(addr:0x%x)\n",client->addr);
	DBG("(name:%s)\n",client->name);

	ARA_client = client;	

	return 0;

}

static int ARA_remove(struct i2c_client *client)
{
	struct vreg *vreg_rfrx2;  //Add for VDD_IR_LED power
  	struct vreg *vreg_msmp;  //Add for VREG_MSMP power
  	struct vreg *vreg_wlan;  //Add for VREG_WLAN power in DQE	
	int ret = -1;
  	DBG("\n"); 	
	   vreg_msmp = vreg_get(0, "msmp");
	   if (!vreg_msmp) {
			printk(KERN_ERR "%s: vreg msmp get failed\n", __func__);
			return -EIO;
	 	}
	    ret = vreg_disable(vreg_msmp);
	    if (ret) {
		    printk(KERN_ERR "%s: vreg MSMP disable failed (%d)\n",
		        __func__, ret);
		return -EIO;
	    }
	
	if(PID >= Project_DPD && PID <= Project_FAD)	
	{
		if(sc668_get_chip_state( ))
		{
			ret = sc668_LDO_setting(1, LDO_OFF, client->name);
			if (ret) {
				//DBG( "%s: LED CATHODE voltage setting failed (%d)\n",  __func__, ret);
				printk(KERN_ERR "%s: LED CATHODE voltage setting failed (%d)\n",  __func__, ret);
				return -EIO;
			} 
		}else{
	  		DBG("sc668 disable!!!");
		}
	}else{
		if(PID == Project_DQE){
			vreg_wlan = vreg_get(0, "wlan");
			if (!vreg_wlan) {
				DBG( "%s: vreg wlan get failed\n", __func__);
				return -EIO;
			}
			ret = vreg_disable(vreg_wlan);
			if (ret) {
				DBG( "%s: vreg wlan disable failed (%d)\n",
					__func__, ret);
			return -EIO;
			}
		}else{		
			vreg_rfrx2 = vreg_get(0, "rfrx2");
			if (!vreg_rfrx2) {
				DBG( "%s: vreg rfrx2 get failed\n", __func__);
				return -EIO;
			}
			ret = vreg_disable(vreg_rfrx2);
			if (ret) {
				DBG( "%s: vreg rfrx2 disable failed (%d)\n",
					__func__, ret);
			return -EIO;
			}
		}
	}
	return 0;
}

static int ARA_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int ARA_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ARA_id[] = {
	{ "ARA", 0 },
	{ }
};

static struct i2c_driver ARA_driver = {
	.probe = ARA_probe,
	.remove = ARA_remove,
	.suspend	= ARA_suspend,
	.resume		= ARA_resume,
	.id_table = ARA_id,
	.driver = {
		   .name = "ARA",
		   },
};


static int __init cm3623_init(void)
{
	int ret;	
    
	cm3602_debug_mask = *(uint32_t *)ALS_DEBUG_MASK_OFFSET;
	//fih_printk(cm3602_debug_mask, FIH_DEBUG_ZONE_G0,"cm3623_init\n");
	
	if((ret=i2c_add_driver(&ALS1_driver)))
	{
		//fih_printk(fihsensor_debug_mask, FIH_DEBUG_ZONE_G0, "ALS1 driver init fail\r\n");	
		//DBG( " ALS1 driver init fail.\n");
		printk(KERN_ERR "%s: ALS1 driver init fail\n", __func__);
	}
	else if((ret=i2c_add_driver(&ALS2_driver)))
	{
		i2c_del_driver(&ALS2_driver);		
		//DBG( " ALS2 driver init fail.\n");
		printk(KERN_ERR "%s: ALS2 driver init fail\n", __func__);
	}
	else if((ret=i2c_add_driver(&PS1_driver)))
	{
		i2c_del_driver(&ALS1_driver);
		i2c_del_driver(&ALS2_driver);
		
		//DBG( " PS1 driver init fail.\n");
		printk(KERN_ERR "%s: PS1 driver init fail\n", __func__);
	}
	else if((ret=i2c_add_driver(&ARA_driver)))
	{
		i2c_del_driver(&ALS1_driver);
		i2c_del_driver(&ALS2_driver);
		i2c_del_driver(&PS1_driver);		
		//DBG( " ARA driver init fail.\n");
		printk(KERN_ERR "%s: ARA driver init fail\n", __func__);
	}
	else if((ret=i2c_add_driver(&PS2_driver)))
	{
		
	    
		i2c_del_driver(&ALS1_driver);
		i2c_del_driver(&ALS2_driver);
		i2c_del_driver(&PS1_driver);		
		i2c_del_driver(&ARA_driver);		
		//DBG( " PS2 driver init fail.\n");
		printk(KERN_ERR "%s: PS2 driver init fail\n", __func__);
	}
	else
	{		
		//DBG(  "cm3623 add I2C driver ok\n");
		printk(KERN_INFO "%s: cm3623 add I2C driver ok\n", __func__);
	}
	
	return ret;

}

static void __exit cm3623_exit(void)
{    
    DBG("\n");
    
	i2c_del_driver(&ALS1_driver);	
	i2c_del_driver(&ALS2_driver);	
	i2c_del_driver(&PS1_driver);	
	i2c_del_driver(&PS2_driver);	
	i2c_del_driver(&ARA_driver);	
	misc_deregister(&cm3623_dev);
	
}
//subsys_initcall(cm3623_init);

late_initcall(cm3623_init);
module_exit(cm3623_exit);

MODULE_DESCRIPTION("cm3623 Sennsor driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NicoleWeng");
/*} FIH, NicoleWeng, 2010/03/09  */
