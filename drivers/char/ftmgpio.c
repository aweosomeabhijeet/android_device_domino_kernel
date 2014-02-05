/*
*     ftmgpio.c - GPIO access driver for FTM use
*
*     Copyright (C) 2009 Clement Hsu <clementhsu@tp.cmcs.com.tw>
*     Copyright (C) 2009 Chi Mei Communication Systems Inc.
*
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; version 2 of the License.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <asm/ioctl.h>
#include <asm/uaccess.h>
#include <linux/miscdevice.h>
#include <mach/gpio.h>
#include "../../arch/arm/mach-msm/proc_comm.h"
//FIH, MichaelKao, 2009/7/9++
/* [FXX_CR], Add for get Hardware version*/
#include <linux/io.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
//FIH, MichaelKao, 2009/7/9++
#include "ftmgpio.h"
//FIH, WillChen, 2009/7/3++
/* [FXX_CR], Merge bsp kernel and ftm kernel*/
#ifdef CONFIG_FIH_FXX
extern int ftm_mode;
#endif
//FIH, WillChen, 2009/7/3--

//FIH, Henry Juang, 2009/08/10{++
/* [FXX_CR], Add for FTM PID and testflag*/
#define PID_length 128
char bufferPID_tsflag[PID_length];
char *pbuf;
#define CAL_length 128
unsigned bufferCAL[CAL_length];

//}FIH, Henry Juang, 2009/08/10--

//FIH,NeoChen, for read&write FUSE,20100727 ++
char FUSEvalue[1];
//FIH,NeoChen, for read&write FUSE,20100727 --

struct cdev * g_ftmgpio_cdev = NULL;
char ioctl_operation=0;
int gpio_pin_num=0;

static int isReadHW=0,isJogball=0;

static const struct file_operations ftmgpio_dev_fops = {
        .open = ftmgpio_dev_open,                   
        .read = ftmgpio_dev_read,                   
        .write = ftmgpio_dev_write,                   
        .ioctl = ftmgpio_dev_ioctl,                 
};                              

static struct miscdevice ftmgpio_dev = {
        MISC_DYNAMIC_MINOR,
        "ftmgpio",
        &ftmgpio_dev_fops
};

static int ftmgpio_dev_open( struct inode * inode, struct file * file )
{
	printk( KERN_INFO "FTM GPIO driver open\n" );

	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		printk( KERN_INFO "FTM GPIO driver device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}

static ssize_t ftmgpio_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos )
{
	char *st,level,pk_status[5];
	int HWID=0;
	printk( KERN_INFO "FTM GPIO driver read, length=%d\n",size);
	level=0;

	if(ioctl_operation==IOCTL_GPIO_GET)	
	{

		gpio_direction_input(gpio_pin_num);
		level = gpio_get_value(gpio_pin_num);
		printk( KERN_INFO "level:%d\n",level );
		st=kmalloc(sizeof(char),GFP_KERNEL);
		sprintf(st,"%d",level);
		if(copy_to_user(buffer,st,sizeof(char)))
		{
			kfree(st);
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		kfree(st);
		return(size);
	}	
	//FIH, MichaelKao, 2009/7/9++
	/* [FXX_CR], Add for get Hardware version*/
#define GPIO_JOGBALL_LEFT  88
#define GPIO_JOGBALL_DOWN  90
#define GPIO_JOGBALL_RIGHT 91
#define GPIO_JOGBALL_UP    93
	else if(ioctl_operation==IOCTL_HV_GET)
	{
		if(!isReadHW)
		{
			printk( KERN_INFO "gpio_get_value(%d):%x, gpio_get_value(%d):%x\n",GPIO_JOGBALL_LEFT,gpio_get_value(GPIO_JOGBALL_LEFT),GPIO_JOGBALL_DOWN,gpio_get_value(GPIO_JOGBALL_DOWN) );
			printk( KERN_INFO "gpio_get_value(%d):%x, gpio_get_value(%d):%x\n",GPIO_JOGBALL_RIGHT,gpio_get_value(GPIO_JOGBALL_RIGHT),GPIO_JOGBALL_UP,gpio_get_value(GPIO_JOGBALL_UP) );

			if(!(gpio_get_value(GPIO_JOGBALL_LEFT)) && !(gpio_get_value(GPIO_JOGBALL_DOWN))
			&& !(gpio_get_value(GPIO_JOGBALL_RIGHT)) && !(gpio_get_value(GPIO_JOGBALL_UP)))
			{
				isJogball=1;		
			}
			isReadHW=1;
		}
		HWID = FIH_READ_ORIG_HWID_FROM_SMEM();
		if(isJogball)
		{
			HWID+=0x400;		
		}
		st=kmalloc(sizeof(char)*4,GFP_KERNEL);
		sprintf(st,"%04x",HWID);
		printk( KERN_INFO "HWID:%x%x%x%x\n",st[0],st[1],st[2],st[3] );
		if(copy_to_user(buffer,st,sizeof(char)*4))
		{
			kfree(st);
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		kfree(st);
		return(size);
	}	
	//FIH, MichaelKao, 2009/7/9--
	
	//FIH, Henry Juang, 2009/08/10++
	/* [FXX_CR], Add for FTM PID and testflag*/
	else if(ioctl_operation==IOCTL_PID_GET)
	{
		pbuf=(char*)bufferPID_tsflag;
#if 0
		printk("**********IOCTL_PID_GET pbuffer=0x%x\n",(unsigned int)pbuf);		
		for(HWID=0;HWID<128;HWID++){
			printk("*********IOCTL_PID_GET[%d]=%d\n",HWID,pbuf[HWID]);
		}

		for(HWID=0;HWID<128;HWID++){
			printk("*********IOCTL_PID_GET[%d]=%d\n",HWID,bufferPID_tsflag[HWID]);
		}
#endif

		if(copy_to_user(buffer,bufferPID_tsflag,sizeof(char)*40))
		{
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		return 1;
	}
	else if(ioctl_operation==IOCTL_TESTFLAG_GET)
	{
		pbuf=(char*)bufferPID_tsflag;
#if 0
		printk("**********IOCTL_TESTFLAG_GET pbuffer=0x%x\n",(unsigned int)pbuf);
		for(HWID=0;HWID<128;HWID++){
			printk("*********IOCTL_TESTFLAG_GET[%d]=%d\n",HWID,pbuf[HWID]);
		}
		for(HWID=0;HWID<128;HWID++){
			printk("*********IOCTL_TESTFLAG_GET[%d]=%d\n",HWID,bufferPID_tsflag[HWID]);
		}
#endif
		
		if(copy_to_user(buffer,bufferPID_tsflag,sizeof(char)*64))
		{
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		return 1;
	}
	//FIH, Henry Juang, 2009/08/10--

	/* FIH, Michael Kao, 2010/09/10{ */
	/* [FXX_CR], Add for FQC Calibration data test*/
	else if((ioctl_operation==IOCTL_CAL_STATUS1)||(ioctl_operation==IOCTL_CAL_STATUS2))
	{
		pbuf=(char*)bufferCAL;
		printk( KERN_INFO "[FTMGPIO_DEV_READ]IOCTL_CAL_STATUS\n" );
		if(copy_to_user(buffer,bufferCAL,sizeof(char)*8))
		{
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		return 1;
	}
	/* FIH, Michael Kao, 2010/09/10{ */
	
	//FIH,NeoChen, for read&write FUSE,20100727 ++
	else if(ioctl_operation==IOCTL_READ_FUSE)
	{
		if(copy_to_user(buffer,FUSEvalue,sizeof(char)))
		{
			printk( KERN_INFO "copy_to_user() fail!\n" );
			return -EFAULT;
		}
		return 1;
	}
	//FIH,NeoChen, for read&write FUSE,20100727 --
	//FIH,Henry Juang, for GRE power key detection,20100816 ++
	else if(ioctl_operation==IOCTL_READ_PWR_KEY_DETECTION)
	{
		pk_status[0]=proc_comm_read_poc();
		if(copy_to_user(buffer,pk_status,sizeof(char)))
		{
			printk( KERN_INFO "IOCTL_READ_PWR_KEY_DETECTION copy_to_user() fail!\n" );
			return -EFAULT;
		}
		printk( KERN_INFO "pk_status[0]=%d\n",pk_status[0]);
		return 1;
	}
	//FIH,Henry Juang, for GRE power key detection,20100816 ++
	
	else
		printk( KERN_INFO "Undefined FTM GPIO driver IOCTL command\n" );
	
	return 0;
}

static int ftmgpio_dev_write( struct file * filp, const char __user * buffer, size_t length, loff_t * offset )
{
	int rc;
	int micbais_en;
	printk( KERN_INFO "FTM GPIO driver write, length=%d, data=%d\n",length , buffer[0] );
	if(ioctl_operation==IOCTL_GPIO_INIT)
	{
		if(buffer[0]==GPIO_IN)
			rc = gpio_tlmm_config(GPIO_CFG(gpio_pin_num, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		else
			rc = gpio_tlmm_config(GPIO_CFG(gpio_pin_num, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
			
		if (rc) 
		{
			printk(KERN_ERR
					"%s--%d: gpio_tlmm_config=%d\n",
					__func__,__LINE__, rc);
			return -EIO;
		}
	
	}
	else if(ioctl_operation==IOCTL_GPIO_SET)	
	{
		gpio_direction_output(gpio_pin_num, buffer[0]);
	}
	else if(ioctl_operation==IOCTL_POWER_OFF)
	{
		msm_proc_comm(PCOM_POWER_DOWN, 0, 0);
		for (;;)
		;
	}
//FIH, Henry Juang, 2009/08/10++
/* [FXX_CR], Add for FTM PID and testflag*/
	else if(ioctl_operation==IOCTL_PID_SET)
	{
		if(copy_from_user(bufferPID_tsflag,buffer,sizeof(char)*40))
		{
			printk( KERN_INFO "copy_from_user() fail!\n" );
			return -EFAULT;
		}
		proc_comm_write_PID_testFlag( SMEM_PROC_COMM_OEM_PRODUCT_ID_WRITE,(unsigned*)bufferPID_tsflag);
		return 1;
	
	}
	else if(ioctl_operation==IOCTL_TESTFLAG_SET)
	{
		if(copy_from_user(bufferPID_tsflag,buffer,sizeof(char)*64))
		{
			printk( KERN_INFO "copy_from_user() fail!\n" );
			return -EFAULT;
		}
		proc_comm_write_PID_testFlag( SMEM_PROC_COMM_OEM_TEST_FLAG_WRITE,(unsigned*)bufferPID_tsflag);
		return 1;
	}
//FIH, Henry Juang, 2009/08/10--
	/* FIH, Michael Kao, 2009/08/28{ */
	/* [FXX_CR], Add to turn on Mic Bias */
	else if(ioctl_operation==IOCTL_MICBIAS_SET)
	{
		micbais_en=buffer[0];
		proc_comm_micbias_set(&micbais_en);
	}
	/* FIH, Michael Kao, 2009/08/28{ */
	else if(ioctl_operation==IOCTL_WRITE_NV_8029)
	{
		if(write_NV_single(gpio_pin_num,buffer[0])!=0)
		{
			printk( KERN_INFO "IOCTL_WRITE_NV_8029 fail! nv=%d,val=%d\n",gpio_pin_num,buffer[0]);
			return -EFAULT;
		}
	}

	else
		printk( KERN_INFO "Undefined FTM GPIO driver IOCTL command\n" );
				
	return length;
}	

#define CLK_UART_NS_REG (MSM_CLK_CTL_BASE + 0x000000E0)

static int ftmgpio_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg )
{
	//[+++]FIH, ChiaYuan, 2007/07/10
	// [FXX_CR] Put the modem into LPM mode for FTM test
	unsigned smem_response;
	uint32_t oem_cmd;
	unsigned oem_parameter=0;
	int Cal_data;
	//[---]FIH, ChiaYuan, 2007/07/10
//FIH, Henry Juang, 20091116 ++
/*Add for modifing UART3 source clock*/
	unsigned long value_UART_NS_REG=0x0;
//FIH, Henry Juang, 20091116 --
	printk( KERN_INFO "FTM GPIO driver ioctl, cmd = %d, arg = %ld\n", cmd , arg);
	
	switch(cmd)
	{
		case IOCTL_GPIO_INIT:		
		case IOCTL_GPIO_SET:		
		case IOCTL_GPIO_GET:	
		//FIH, MichaelKao, 2009/7/9++
		/* [FXX_CR], Add for get Hardware version*/
		case IOCTL_HV_GET:
		//FIH, MichaelKao, 2009/7/9++
		case IOCTL_POWER_OFF:
		/* FIH, Michael Kao, 2009/08/28{ */
		/* [FXX_CR], Add to turn on Mic Bias */
		case IOCTL_MICBIAS_SET:
		/* FIH, Michael Kao, 2009/08/28{ */
			//printk("[Ftmgoio.c][ftmgpio_dev_ioctl]power_off_test++++");
			ioctl_operation = cmd;
			gpio_pin_num = arg;			
			break;

//FIH, Henry Juang, 2009/08/10{++
/* [FXX_CR], Add for FTM PID and testflag*/
		case IOCTL_PID_GET:
			memset(bufferPID_tsflag,0,40*sizeof(char));
			proc_comm_read_PID_testFlag( SMEM_PROC_COMM_OEM_PRODUCT_ID_READ,(unsigned*)bufferPID_tsflag);
			ioctl_operation = cmd;
			gpio_pin_num = arg;	
			break;

		case IOCTL_PID_SET:
			ioctl_operation = cmd;
			gpio_pin_num = arg;			
			break;
		case IOCTL_TESTFLAG_GET:
			memset(bufferPID_tsflag,0,64*sizeof(char));
			proc_comm_read_PID_testFlag( SMEM_PROC_COMM_OEM_TEST_FLAG_READ,(unsigned*)bufferPID_tsflag);
			ioctl_operation = cmd;
			gpio_pin_num = arg;	
			break;
		case IOCTL_TESTFLAG_SET:
			ioctl_operation = cmd;
			gpio_pin_num = arg;			
			break;
//}FIH, Henry Juang, 2009/08/10--
		
		//[+++]FIH, ChiaYuan, 2007/07/10
		// [FXX_CR] Put the modem into LPM mode for FTM test	
		case IOCTL_MODEM_LPM:
			oem_cmd = SMEM_PROC_COMM_OEM_EBOOT_SLEEP_REQ;
			msm_proc_comm_oem(PCOM_CUSTOMER_CMD1, &oem_cmd, &smem_response, &oem_parameter );
			break;
		//[---]FIH, ChiaYuan, 2007/07/10							
//FIH, Henry Juang, 20091116 ++
/*Add for modifing UART3 source clock*/
		case IOCTL_UART_BAUDRATE_460800:
				//add for testing.	
				value_UART_NS_REG=0x7ff8fff & readl(CLK_UART_NS_REG);//set UART source clock to TCXO.
				printk("1******value_UART_NS_REG=0x%x%x,addr=0x%x\n",(unsigned int)(value_UART_NS_REG>>8)&0xff,(unsigned int)(value_UART_NS_REG & 0xff),(unsigned int)CLK_UART_NS_REG);	
				if(arg!=1){
					value_UART_NS_REG|=0x0001000; //roll back UART source clock to TCXO/4.
				}
				writel(value_UART_NS_REG,CLK_UART_NS_REG);	
				value_UART_NS_REG=readl(CLK_UART_NS_REG);	
				printk("2******value_UART_NS_REG=0x%x%x,addr=0x%x\n",(unsigned int)(value_UART_NS_REG>>8)&0xff,(unsigned int)(value_UART_NS_REG & 0xff),(unsigned int)CLK_UART_NS_REG);	
			break;
//FIH, Henry Juang, 20091116 --
//FIH, Henry Juang, 20100426 ++
/*Add for write NV 8029.*/
		case IOCTL_WRITE_NV_8029:
			printk("select  nv_%d to write. \n",(int)arg);
			ioctl_operation = cmd;
			gpio_pin_num = arg;	
			
			break;
//FIH, Henry Juang, 20100426 --
		/* FIH, Michael Kao, 2010/09/10{ */
		/* [FXX_CR], Add for FQC Calibration data test*/
		case IOCTL_CAL_STATUS1:
			bufferCAL[0]=2499;
			if(proc_comm_read_nv((unsigned*)bufferCAL)==0)
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS1=%x, %x\n",bufferCAL[0],bufferCAL[1]);
			else
			{
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS1 read fail\n");
				return 0;
			}
			Cal_data=(bufferCAL[0]&0xff)*1000000+((bufferCAL[0]&0xff00)>>8)*10000+
				((bufferCAL[0]&0xff0000)>>16)*100+((bufferCAL[0]&0xff000000)>>24);
			printk("[FTMGPIO.c]Cal_data1=%d\n",Cal_data);
			ioctl_operation = cmd;
			gpio_pin_num = arg;
			return Cal_data;
			break;
		case IOCTL_CAL_STATUS2:
			bufferCAL[0]=2499;
			if(proc_comm_read_nv((unsigned*)bufferCAL)==0)
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS1=%d, %d\n",bufferCAL[0],bufferCAL[1]);
			else
			{
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS1 read fail\n");
				return 0;
			}
			Cal_data=(bufferCAL[1]&0xff)*1000000+((bufferCAL[1]&0xff00)>>8)*10000+
				((bufferCAL[1]&0xff0000)>>16)*100+((bufferCAL[1]&0xff000000)>>24);
			printk("[FTMGPIO.c]Cal_data2=%d\n",Cal_data);
			ioctl_operation = cmd;
			gpio_pin_num = arg;	
			return Cal_data;
			break;
		case IOCTL_CAL_STATUS3:
			bufferCAL[0]=2500;
			if(proc_comm_read_nv((unsigned*)bufferCAL)==0)
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS2=0x%x\n",bufferCAL[0]);
			else
			{
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS2 read fail\n");
				return 0;
			}
			Cal_data=(bufferCAL[0]&0xff)*1000000+((bufferCAL[0]&0xff00)>>8)*10000+
				((bufferCAL[0]&0xff0000)>>16)*100+((bufferCAL[0]&0xff000000)>>24);
			printk("[FTMGPIO.c]Cal_data3=%d\n",Cal_data);
			ioctl_operation = cmd;
			gpio_pin_num = arg;	
			return Cal_data;
			break;
		case IOCTL_CAL_STATUS4:
			bufferCAL[0]=2500;
			if(proc_comm_read_nv((unsigned*)bufferCAL)==0)
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS2=0x%x\n",bufferCAL[0]);
			else
			{
				printk("[FTMGPIO.c]IOCTL_CAL_STATUS2 read fail\n");
				return 0;
			}
			Cal_data=(bufferCAL[1]&0xff)*1000000+((bufferCAL[1]&0xff00)>>8)*10000+
				((bufferCAL[1]&0xff0000)>>16)*100+((bufferCAL[1]&0xff000000)>>24);
			printk("[FTMGPIO.c]Cal_data4=%d\n",Cal_data);
			ioctl_operation = cmd;
			gpio_pin_num = arg;
			return Cal_data;
			break;
		/* FIH, Michael Kao, 2010/09/10{ */
//FIH,NeoChen, for read&write FUSE,20100727 ++
		case IOCTL_WRITE_FUSE:
			proc_comm_write_FUSE( SMEM_PROC_COMM_OEM_WRITE_FUSE);
			
			break;
			
		case IOCTL_READ_FUSE:
			memset(FUSEvalue,0,sizeof(char));
			ioctl_operation = cmd;
			proc_comm_read_FUSE( SMEM_PROC_COMM_OEM_READ_FUSE,(unsigned*)FUSEvalue);
			
			break;
//FIH,NeoChen, for read&write FUSE,20100727 --
//FIH,NeoChen, for GRE read&write FUSE,20100727 --

//FIH,Henry Juang, for GRE power key detection,20100816 ++
		case IOCTL_READ_PWR_KEY_DETECTION:
			ioctl_operation = cmd;
			break;
//FIH,Henry Juang, for GRE power key detection,20100816 --

/* 20101207, SquareCHFang, NVBackup { */
		case IOCTL_BACKUP_UNIQUE_NV:
			printk(KERN_INFO "===========================> ftmgpio.c IOCTL_BACKUP_UNIQUE_NV 2\n");
			if( proc_comm_ftm_backup_unique_nv() != 0 ){
				return -1;
			}
			break;
		case IOCTL_HW_RESET:
			printk(KERN_INFO "===========================> ftmgpio.c IOCTL_HW_RESET 2\n");
			//proc_comm_ftm_hw_reset();
			break;
		case IOCTL_BACKUP_NV_FLAG:
			printk(KERN_INFO "===========================> ftmgpio.c IOCTL_BACKUP_NV_FLAG 2\n");
			if( proc_comm_ftm_nv_flag() != 0){
				return -1;
			}
			break;
/* } 20101207, SquareCHFang, NVBackup */

		default:
			printk( KERN_INFO "Undefined FTM GPIO driver IOCTL command\n" );
			return -1;
	}


	return 0;
}

 static int __init ftmgpio_init(void)
{
        int ret;

        printk( KERN_INFO "FTM GPIO Driver init\n" );
//FIH, WillChen, 2009/7/3++
/* [FXX_CR], Merge bsp kernel and ftm kernel*/
/* FIH, Michael Kao, 2010/09/10{ */
/* [FXX_CR], Add for FQC Calibration data test*/
#if 0
#ifdef CONFIG_FIH_FXX
	if (ftm_mode == 0)
	{
		printk(KERN_INFO "This is NOT FTM mode. return without init!!!\n");
		return -1;
	}
#endif
#endif
/* FIH, Michael Kao, 2010/09/10{ */

//FIH, WillChen, 2009/7/3--
	ret = misc_register(&ftmgpio_dev);
	if (ret){
		printk(KERN_WARNING "FTM GPIO Unable to register misc device.\n");
		return ret;
	}

        return ret;
        
}

static void __exit ftmgpio_exit(void)                         
{                                                                              
        printk( KERN_INFO "FTM GPIO Driver exit\n" );          
        misc_deregister(&ftmgpio_dev);
}                                                        
                                                         
module_init(ftmgpio_init);                            
module_exit(ftmgpio_exit);                                
                                                         
MODULE_AUTHOR( "Clement Hsu <clementhsu@tp.cmcs.com.tw>" );
MODULE_DESCRIPTION( "FTM GPIO driver" );                
MODULE_LICENSE( "GPL" );                                 



















