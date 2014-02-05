/*
 *     rt9365.c - rt9365  LEDs CONTROLLER
 *
 *     Copyright (C) 2009 Chandler Kang<chandlercckang@fihtdc.com>
 *     Copyright (C) 2009 FIH Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */
/* porting_rt9365 */
#include <linux/init.h>
#include <linux/module.h>
//#include <linux/cmdbgapi.h>
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

#include <linux/i2c/rt9365.h>
#include <mach/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

#include "../../../kernel/power/power.h"
#include <linux/platform_device.h>
#include "mach/sc668.h"

#define MODULE_NAME "rt9365"
#define BL_CTRL  30
#define TRUE 1
#define FALSE 0
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
static struct proc_dir_entry *bl_rt9365_proc_file = NULL;

static uint32_t rt9365_debug_mask = 0;
module_param_named(debug_mask, rt9365_debug_mask, uint, S_IRUGO | S_IWUSR | S_IWGRP);

static int fih_bkl_is_sc668=TRUE;
static int fih_bkl_len_num=4;
static int fih_lcm_is_power_by_sc668=TRUE;

static int BKL_MAX_LEVEL=31;
static u8 current_level=0;

int set_sc668_GROUP1_fade_effect(unsigned char effectGrp1)
{
    int ret;
    unsigned char data;
    unsigned char effectGrp2;

    data = sc668_read_register(EFFECT_RATE_SETTINGS, MODULE_NAME);
    effectGrp2 = data >> 3;

    ret = sc668_set_effect_rate(effectGrp1, effectGrp2, MODULE_NAME); //set GROUP2 as BREATH_16_FADE_4
    return ret;
}

void rt9365_set_level(u8 level) //level 0-16(32)
{

	if(fih_bkl_is_sc668){
		int effect;
		
		if(level==0)
			effect=NO_EFFECT;
		else
			effect=ENABLE_FADE;
		
	  	printk( KERN_INFO "%s(): level(%d)\n", __func__, level);					
		sc668_set_backLight_currentAndEffect(BANK_1, level, level, effect, MODULE_NAME);

		/* for DMO, DMT 6 led */
		if(fih_bkl_len_num==6) 
			sc668_set_backLight_currentAndEffect(BANK_3, level, BACKLIGHT_CURRENT_0_0, NO_EFFECT, MODULE_NAME);

	}else{
	
		int i=0;
		unsigned long flags;
		
		if(current_level==level) return;
		
		if( level==0 ) {
			gpio_direction_output(BL_CTRL,0);
			mdelay(4);  // 3ms < shutdown time
			current_level=0; 
			
			return;
		}

		if( BKL_MAX_LEVEL < level ){
			printk("%s():Out of range", __func__);
			level=BKL_MAX_LEVEL;
		}
		
		spin_lock_irqsave(&lock_onewire, flags);
		
		gpio_direction_output(BL_CTRL,0);	
		mdelay(3);
			
		gpio_direction_output(BL_CTRL,1);
		udelay(40); //  30us < ready time 
		
		for( i=0; i < BKL_MAX_LEVEL-level; i++){
			gpio_direction_output(BL_CTRL,0);
			udelay(1); // 0.5us < lo time < 500us
			gpio_direction_output(BL_CTRL,1);
			udelay(1); // 0.5us < hi time 
		}
		current_level=level;
		
		spin_unlock_irqrestore(&lock_onewire, flags);
	}
	
}


/*****************LCD part***************/
int lcd_bl_set_intensity(int level)//level 0-100
{
//	static int retry_count=0;
//	int rc;
	u8 mapping_level;
//	unsigned long flags;

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
			//fih_printk(rt9365_debug_mask, FIH_DEBUG_ZONE_G1, "[%s] LCM On checkTimes: %d\n", MODULE_NAME, checkTimes);
		}
		
		//for normal white LCM, delay to prevent flash during resume
		if(fih_read_product_id_from_orighwid() == Project_DMQ && checkTimes>0) 
			msleep(50);
		
		//FXX_CR, Neo Chen, 2009.12.31 Let backlight not to open until LCM is on
		//if(checkTimes>=20) return -1;
	}
	// FXX_CR, Neo Chen, 2009.06.12 ---
	
	/***  Addition check from max8831 ***/
	
	gBrightness_level=level;

	mapping_level=(level* BKL_MAX_LEVEL )/100 ;
	
	if( level != 0 && mapping_level == 0  ){
		mapping_level=1;
	}
	printk(KERN_ERR "[%s] %s(): checkTimes(%d), level(%u), mapping_level(%u)\n", MODULE_NAME, __func__,checkTimes, level, mapping_level);
	

//	spin_lock_irqsave(&lock_onewire, flags);
//	onewire_reset();
//	onewire_send_addr(0x72);	
//	rc=onewire_send_data(mapping_level,1);
	rt9365_set_level(mapping_level);
//	spin_unlock_irqrestore(&lock_onewire, flags);
#if 0	
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
#endif	
	return 0;
}
EXPORT_SYMBOL(lcd_bl_set_intensity);

/* FIH, Henry Juang, 2009/11/20 ++*/
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
int Proximity_Flag_Set(int flag)
{
	//fih_printk(rt9365_debug_mask, FIH_DEBUG_ZONE_G0, "[%s] Proximity_Flag_Set:pre-value %d\n", MODULE_NAME, gProximity_flag_On);
	if(flag){
		gProximity_flag_On = flag;
		lcd_bl_set_intensity(0);
	}
	else{
		gProximity_flag_On = flag;
		if(gBrightness_level > 0) lcd_bl_set_intensity(gBrightness_level);
	}
	
	//return gProximity_flag_On;
	return 1;
}
EXPORT_SYMBOL(Proximity_Flag_Set);


/******************Proc operation********************/
static int rt9365_seq_open(struct inode *inode, struct file *file)
{
  	return single_open(file, NULL, NULL);
}

static ssize_t rt9365_seq_write(struct file *file, const char *buff,
								size_t len, loff_t *off)
{

	char str[64];
	int param = -1;
	int param2 = -1;
	int param3 = -1;
	char cmd[32]={0};
	int i;
	len=0;
	//printk(KERN_ERR "[%s] rt9365_seq_write()\n", MODULE_NAME);
	
	if(copy_from_user(str, buff, sizeof(str)))
	{
		printk(KERN_ERR "rt9365_seq_write(): copy_from_user failed!\n");
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

static struct file_operations rt9365_seq_fops = 
{
  	.owner 		= THIS_MODULE,
	.open  		= rt9365_seq_open,
	.write 		= rt9365_seq_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int rt9365_create_proc(void)
{
  	bl_rt9365_proc_file = create_proc_entry("driver/lcm_bkl", 0666, NULL);
	
	if(!bl_rt9365_proc_file){
	  	printk(KERN_ERR "create proc file for rt9365 failed\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "rt9365_create_proc() ok\n");
	bl_rt9365_proc_file->proc_fops = &rt9365_seq_fops;
	return 0;
}

#ifdef CONFIG_FTMPROJECT
test
#endif

extern void lcdc_lil9481_trigger_lcm(void);

static int __init rt9365_probe(struct platform_device *pdev)
{
	int rc=0;
	
//	unsigned long flags;
	
	printk( KERN_ERR "%s()\n", __func__);	
	spin_lock_init(&lock_onewire);	
	
	if(fih_bkl_is_sc668){
		
		if(fih_lcm_is_power_by_sc668){
			//open ldo
			sc668_LDO_setting( 1, LDO_VOLTAGE_2_8, MODULE_NAME);	
		}		
		
		if(fih_bkl_len_num==5 || fih_bkl_len_num==6){			
			//bank assigment
			sc668_set_light_assignments(
				BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1, 
				GROUP1_BK1_GROUP2_BK2_BK3_BK4, MODULE_NAME);
		}else if(fih_bkl_len_num==4){
			//bank assigment
			sc668_set_light_assignments(
				BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1, 
				GROUP1_BK1_GROUP2_BK2_BK3_BK4, MODULE_NAME);
		}
		
		mdelay(10);
		
		//enable bank1 for lcd backlight
		sc668_enable_Banks(BANK_1_ENABLE, MODULE_NAME);
		mdelay(10);

		//enable bank1 for lcd backlight
		if(fih_bkl_len_num==6){
			sc668_enable_Banks(BANK_3_ENABLE, MODULE_NAME);
		}

		//set backlight
		sc668_set_backLight_currentAndEffect(BANK_1, 
			BACKLIGHT_CURRENT_10_0, 
			BACKLIGHT_CURRENT_0_0, ENABLE_FADE, MODULE_NAME);
		
		if(fih_bkl_len_num==6)
		sc668_set_backLight_currentAndEffect(BANK_3, 
			BACKLIGHT_CURRENT_10_0, 
			BACKLIGHT_CURRENT_0_0, NO_EFFECT, MODULE_NAME);
		
		set_sc668_GROUP1_fade_effect(0x07);

		if(fih_lcm_is_power_by_sc668){
			lcdc_lil9481_trigger_lcm();
		}
	}else{
		gpio_direction_output(BL_CTRL,1);
		udelay(50); //  30us < ready time
	  	printk( KERN_INFO "%s(): gpio(%d) read = %d\n",__func__,BL_CTRL, gpio_get_value(BL_CTRL));
	}

	
/*	
	rc=gpio_tlmm_config(GPIO_CFG(BL_CTRL, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	if(rc){
		printk( KERN_ERR "%s():gpio BL_CTRL(%d) config failed.(%d), just ignore\n", __func__, BL_CTRL, rc);
		//return -1;
	}
	*/
	/*
	rc=gpio_request(BL_CTRL, "BL_CTRL");
	if(rc){
		printk( KERN_ERR "%s(): BL_CTRL(%d) request failed.(%d)\n", __func__, BL_CTRL, rc);
		return -1;
	}
	*/
	//spin_lock_irqsave(&lock_onewire, flags);
	
	//spin_unlock_irqrestore(&lock_onewire, flags);		
	/*
	gpio_free(BL_CTRL);	
	*/
	return rc;
}

static int rt9365_remove(struct platform_device *pdev)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );

	//gpio_free(BL_CTRL);

	return 0;
}

static struct platform_driver rt9365_driver = {
	.probe = rt9365_probe,
	.remove = rt9365_remove,
	.suspend  = NULL,
	.resume   = NULL,
	.driver = {
		.name = "rt9365",
	},
};

int fih_bkl_is_rt9365(void)
{
#if 1
	if(Project_DQE <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_FAD){
		return true;
	}else if(Project_DMO <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_DMT){
		return true;
	}else{
		return false;
	}
#else
	return false;
#endif
}

static void get_hw_setting()
{
	//DMP-related
	fih_bkl_is_sc668=TRUE;
	fih_lcm_is_power_by_sc668=TRUE;
	fih_bkl_len_num=5;
	BKL_MAX_LEVEL=31;

	/* check bkl type */
	if(Project_DQE <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_DQD){
		//DMQ-related

		fih_bkl_len_num=4;
		fih_lcm_is_power_by_sc668=FALSE;
		if(fih_read_phase_id_from_orighwid() <= Phase_PR1)
		{
			fih_bkl_is_sc668=FALSE;
			BKL_MAX_LEVEL=16;		
		}
	/*	DMO,DMT-related  */
	}else if(Project_DMO<=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_DMT){
		fih_lcm_is_power_by_sc668=FALSE;
		fih_bkl_len_num=6;
	}

}

static int __init rt9365_init(void)
{
	int ret = 0;

	//printk( KERN_INFO "Driver init: %s\n", __func__ );
	printk( KERN_INFO "%s(): hwid(%d), phase(PR%d)\n", __func__, fih_read_product_id_from_orighwid(), fih_read_phase_id_from_orighwid());	

	if(!fih_bkl_is_rt9365()){
		goto exit;
	}

	/* check hw setting */
	get_hw_setting();
	
	printk( KERN_INFO "%s(): fih_bkl_is_sc668(%d), fih_lcm_is_power_by_sc668(%d), fih_bkl_len_num(%d)\n", 
		__func__, fih_bkl_is_sc668, fih_lcm_is_power_by_sc668,fih_bkl_len_num);	
	
	ret = platform_driver_register(&rt9365_driver);
	if(ret)	{
		printk(KERN_ERR "rt9365_init failed\r\n");
	}

	// create proc
	ret = rt9365_create_proc();
	if(ret) {
	  	printk(KERN_ERR "%s: create proc file failed\n", __func__);
		goto exit;
	}

//	rt9365_debug_mask = *(uint32_t *)BKL_DEBUG_MASK_OFFSET;
	//rt9365_debug_mask = 0xff;
	
	//all successfully.
	return ret;

exit:
	return -1;
}

static void __exit rt9365_exit(void)
{
#if 1
	if(bl_rt9365_proc_file)
		remove_proc_entry("driver/lcm_bkl",NULL);
#endif
	platform_driver_unregister(&rt9365_driver);
}

module_init(rt9365_init);
module_exit(rt9365_exit);

MODULE_AUTHOR( "Chandelr Kang<chandlercckang@fihtdc.com>" );
MODULE_DESCRIPTION( "rt9365 driver" );
MODULE_LICENSE( "GPL" );


