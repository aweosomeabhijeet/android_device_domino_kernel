/*
 *     tps61160.c - tps61160  LEDs CONTROLLER
 *
 *     Copyright (C) 2009 Chandler Kang<chandlerkang@fihtdc.com>
 *     Copyright (C) 2009 FIH Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cmdbgapi.h>
#include <linux/kernel.h> 
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/ioctl.h>

#include <linux/tps61160.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

#include "../../../kernel/power/power.h"
#include <linux/platform_device.h>

#define MODULE_NAME "tps61160"
#define BL_CTRL  97
#define TIME_BASE 2  //2 // 2us - 180 us
#define TIME_FRAME 2   //at least 2 us

//Neo: workaround for adjusting LCD brightness
#define ISNUM(x) (((x) >= '0') && ((x) <= '9'))

/* FIH, Henry Juang, 2009/11/20 --*/
// FXX_CR, Neo Chen, 2009.06.12, Add for ensuring backlight on after panel on
extern int msm_fb_check_panel_on(int fb_no);

static spinlock_t lock_onewire= SPIN_LOCK_UNLOCKED;
/* FIH, Henry Juang, 2009/11/20 ++*/
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
static int gProximity_flag_On = 0;
static int gBrightness_level = 0;
static struct proc_dir_entry *bl_tps61160_proc_file = NULL;

static uint32_t tps61160_debug_mask = 0;
module_param_named(debug_mask, tps61160_debug_mask, uint, S_IRUGO | S_IWUSR | S_IWGRP);

#if 1
void onewire_reset(void)
{
	/*** enable 1-wire omde ***/
	
	gpio_direction_output(BL_CTRL,0);
	mdelay(3);

	gpio_direction_output(BL_CTRL,1);
	udelay(100); //detection delay
//	printk( KERN_ERR "pull %d high (%d)\n",BL_CTRL, gpio_get_value(BL_CTRL));

	gpio_direction_output(BL_CTRL,0);
	udelay(260); //detection time
//	printk( KERN_ERR "pull %d low (%d)\n",BL_CTRL, gpio_get_value(BL_CTRL));

	gpio_direction_output(BL_CTRL,1);
//	printk( KERN_ERR "pull %d high (%d)\n",BL_CTRL, gpio_get_value(BL_CTRL));	

	mdelay(1); //detection window

}
void inline onewire_send_bit(u32 bit)
{
	int low_time,high_time; 

	if(bit) {
		low_time=TIME_BASE ; high_time=TIME_BASE*3;
	} else {
		low_time=TIME_BASE*3 ; high_time=TIME_BASE;
	}
	gpio_direction_output(BL_CTRL,0);
	udelay(low_time);
	gpio_direction_output(BL_CTRL,1);
	udelay(high_time);
	
}

void inline onewire_send_addr(u8 addr)
{
	int i;
	gpio_direction_output(BL_CTRL,1);
	udelay(TIME_FRAME);
	for( i=7; i>=0; i--){
		if((addr >> i ) & 0x01) 
			onewire_send_bit(1);
		else
			onewire_send_bit(0);
	}
	gpio_direction_output(BL_CTRL,0);
	udelay(TIME_FRAME);
}

int inline onewire_send_data(u8 data, int ack)
{
	int i,rc,ret=0;
	if(ack){
		data = data | (0x01 << 7);
	}
	gpio_direction_output(BL_CTRL,1);
	udelay(TIME_FRAME);
	
	for( i=7; i>=0; i--){
		if((data >> i ) & 0x01) 
			onewire_send_bit(1);
		else
			onewire_send_bit(0);
	}	
	gpio_direction_output(BL_CTRL,0);
	udelay(TIME_FRAME);
	
	/*** handle ACK ***/
	if(!ack){	
		gpio_direction_output(BL_CTRL,1);
	}else{	
		gpio_direction_output(BL_CTRL,1);
		
		/*  switch to input */		
		gpio_tlmm_config(GPIO_CFG(BL_CTRL, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
		
		rc=gpio_request(BL_CTRL, "BL_CTRL");
		if(rc){
			printk( KERN_ERR "gpio_request BL_CTRL input failed (%d)\n",rc);
			ret= -1;
		}
		udelay(100);// 2us + 512 us
		if(gpio_get_value(BL_CTRL)) {
			printk(KERN_ERR "[%s]%s(): No ack !!!\n",MODULE_NAME, __func__);
			ret = -1;
		}else{
			// pull low mean ack
			printk(KERN_ERR "[%s]%s(): Got ack.\n",MODULE_NAME,__func__);
		} 
		gpio_free(BL_CTRL);		
		
		
		/*  switch back to output */		
		gpio_tlmm_config(GPIO_CFG(BL_CTRL, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);	
#if 0		
		rc=gpio_request(BL_CTRL, "BL_CTRL");
		if(rc){
			printk( KERN_ERR "gpio_request BL_CTRL output failed (%d)\n",rc);
			ret = -1;
		}
		if(!ret)
			gpio_direction_output(BL_CTRL,1);
		gpio_free(BL_CTRL);
#endif			
	}
	
	return ret;
}

#endif


/*****************LCD part***************/
static int lcd_bl_set_intensity(int level)
{
	static int retry_count=0;
	int rc;
	u8 mapping_level;
	unsigned long flags;

	/***  Addition check from max8831 ***/
	int checkPanelON = 0; // FXX_CR, Neo Chen, 2009.06.12, Add for ensuring backlight on after panel on
	int checkTimes = 0; //FXX_CR, Neo Chen, 2009.08.11, Prevent to block in while loop when check panel on
#if 0	
	suspend_state_t SuspendState = PM_SUSPEND_ON;//0
	// FXX_CR, Neo Chen, 2009.08.13, if in suspend state, not to open backlight
	SuspendState = get_suspend_state();
	if(SuspendState == PM_SUSPEND_MEM && level != 0) //3
		return -1;
#endif
	// FXX_CR, Neo Chen, 2009.06.12, Add for ensuring backlight on after panel on +++
	while(!checkPanelON)
	{
		checkPanelON = msm_fb_check_panel_on(0);
		if(!checkPanelON)
		{
			msleep(10);
			checkTimes++;//FXX_CR, Neo Chen, 2009.08.11, Prevent to block in while loop when check panel on
			//fih_printk(tps61160_debug_mask, FIH_DEBUG_ZONE_G1, "[%s] LCM On checkTimes: %d\n", MODULE_NAME, checkTimes);
		}
		//FXX_CR, Neo Chen, 2009.12.31 Let backlight not to open until LCM is on
		//if(checkTimes>=20) return -1;
	}
	// FXX_CR, Neo Chen, 2009.06.12 ---
	
	/***  Addition check from max8831 ***/
	
	gBrightness_level=level;

	mapping_level=(level*31 )/100 ;
	
	if( level != 0 && mapping_level == 0  ){
		mapping_level=1;
	}
	printk(KERN_ERR "[%s] %s(): level(%u), mapping_level(%u)\n", MODULE_NAME, __func__, level, mapping_level);
	
retry:
	spin_lock_irqsave(&lock_onewire, flags);
//	onewire_reset();
	onewire_send_addr(0x72);	
	rc=onewire_send_data(mapping_level,1);
	
	spin_unlock_irqrestore(&lock_onewire, flags);
	
	if(rc){
		retry_count++;
		printk(KERN_ERR "[%s] %s(): retry count(%d)\n", MODULE_NAME, __func__, retry_count);
		if(retry_count<3){
			spin_lock_irqsave(&lock_onewire, flags);
			onewire_reset();
			spin_unlock_irqrestore(&lock_onewire, flags);
			goto retry;
		}

		retry_count=0;
	}
	return 0;
}


/******************Proc operation********************/
static int tps61160_seq_open(struct inode *inode, struct file *file)
{
  	return single_open(file, NULL, NULL);
}

static ssize_t tps61160_seq_write(struct file *file, const char *buff,
								size_t len, loff_t *off)
{

	char str[64];
	int param = -1;
	int param2 = -1;
	int param3 = -1;
	char cmd[32]={0};
	int i;
	len=0;
	
	if(copy_from_user(str, buff, sizeof(str)))
	{
		printk(KERN_ERR "tps61160_seq_write(): copy_from_user failed!\n");
		return -EFAULT;	
	}

	str[63]=0;
#if 0	
	printk("[tps6610] start\n");
	for(i=0 ; i<64; i++){
	  	printk("%c",str[i]);			
	}
	printk("\n[tps6610]end\n"); 
#endif

  	if(sscanf(str, "%s %d %d %d", cmd, &param, &param2, &param3) == -1)
	{
	  	printk(KERN_ERR"parameter format: <type> <value>\n");
 		return -EINVAL;
	}
	
	if(ISNUM(cmd[0])){ 
		
		param=0;
	
		for(i=0;i<3;i++){
			if (ISNUM(cmd[i])) {
				param*=10;
				param+=cmd[i]-48;
				len++;
			}else {
				break;
			}

		}
	}
	
	if(param != -1){
	  	lcd_bl_set_intensity(param);
	}

	return len;
	
	
}

static struct file_operations tps61160_seq_fops = 
{
  	.owner 		= THIS_MODULE,
	.open  		= tps61160_seq_open,
	.write 		= tps61160_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};



static int tps61160_create_proc(void)
{
  	bl_tps61160_proc_file = create_proc_entry("driver/lcm_bkl", 0666, NULL);
	
	if(!bl_tps61160_proc_file){
	  	printk(KERN_ERR "create proc file for tps61160 failed\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "tps61160_create_proc() ok\n");
	bl_tps61160_proc_file->proc_fops = &tps61160_seq_fops;
	return 0;
}

#if 0
static int tps61160_resume(struct platform_device *pdev)
{
	return 0;
}


static int tps61160_suspend(struct platform_device *pdev)
{
	return 0;
}

#endif

static int __init tps61160_probe(struct platform_device *pdev)
{
	int rc;
	int i;
	unsigned long flags;
	
	printk( KERN_ERR "tps61160_probe()\n");	
	
	spin_lock_init(&lock_onewire);	
	
	rc=gpio_tlmm_config(GPIO_CFG(BL_CTRL, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	if(rc){
		printk( KERN_ERR "gpio touch_rst config failed.(%d)\n",rc);
		return -1;
	}
	
	rc=gpio_request(BL_CTRL, "BL_CTRL");
	if(rc){
		printk( KERN_ERR "gpio touch_rst request failed.(%d)\n",rc);
		return -1;
	}
	
	spin_lock_irqsave(&lock_onewire, flags);
	onewire_reset();
	for(i=1;i<32;i++){
		onewire_send_addr(0x72);
		onewire_send_data(i,0);
		mdelay(30);
	}
	spin_unlock_irqrestore(&lock_onewire, flags);		
	
	gpio_free(BL_CTRL);	

	return rc;
}

static int tps61160_remove(struct platform_device *pdev)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );

	gpio_free(BL_CTRL);

	return 0;
}

static struct platform_driver tps61160_driver = {
	.probe = tps61160_probe,
	.remove = tps61160_remove,
	.suspend  = NULL,
	.resume   = NULL,
	.driver = {
		.name = "tps61160",
	},
};


int fih_bkl_is_tps61160(void)
{
	if(Project_AI1S <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_AI1D){
		return true;
	}else{
		return false;
	}
}


static int __init tps61160_init(void)
{
	int ret = 0;

	printk( KERN_INFO "Driver init: %s\n", __func__ );

	if(!fih_bkl_is_tps61160()){
		goto exit;
	}
	
	ret = platform_driver_register(&tps61160_driver);
	if(ret)	{
		printk(KERN_ERR "tps61160_init fail\r\n");
	}

	// create proc
	ret = tps61160_create_proc();
	if(ret) {
	  	printk(KERN_ERR "%s: create proc file failed\n", __func__);
		goto exit;
	}

	tps61160_debug_mask = *(uint32_t *)BKL_DEBUG_MASK_OFFSET;
	//onewire_reset();//test
	
	//all successfully.
	return ret;

exit:
	return -1;
}

static void __exit tps61160_exit(void)
{
#if 1
	if(bl_tps61160_proc_file)
		remove_proc_entry("driver/lcm_bkl",NULL);
#endif
	platform_driver_unregister(&tps61160_driver);
}

module_init(tps61160_init);
module_exit(tps61160_exit);

MODULE_AUTHOR( "Chandelr Kang<chandlercckang@fihtdc.com>" );
MODULE_DESCRIPTION( "tps61160 driver" );
MODULE_LICENSE( "GPL" );

