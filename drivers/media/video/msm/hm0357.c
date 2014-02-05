/*
 *     hm0357.c - Camera Sensor Driver
 *
 *     Copyright (C) 2009 Charles YS Huang <charlesyshuang@fihtdc.com>
 *     Copyright (C) 2008 FIH CO., Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include "hm0357.h"
#include <mach/pmic.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

/* FIH, Charles Huang, 2010/03/01 { */
/* [FXX_CR], Add power onoff vreg */
#ifdef CONFIG_FIH_FXX
#include <mach/vreg.h>
#endif
/* } FIH, Charles Huang, 2010/03/01 */
/* FIH, IvanCHTang, 2010/6/1 { */
/* [FXX_CR], Set CAMIF_PD_VGA by HWID for FM6/FN6 - [FM6#0003] */
#ifdef CONFIG_FIH_FXX
#include <mach/mpp.h>
//static unsigned mpp_1 = 0;
static unsigned mpp_22 = 21;
#endif
/* } FIH, IvanCHTang, 2010/6/1 */

DEFINE_MUTEX(hm0357_mut);

/* Micron HM0357 Registers and their values */
/* Sensor Core Registers */
#define  REG_HM0357_MODEL_ID_HI 0x01
#define  REG_HM0357_MODEL_ID_LO 0x02
#define  HM0357_MODEL_ID     0x0357

#define  HM0357_I2C_READ_SLAVE_ID     0x61 >> 1  
#define  HM0357_I2C_WRITE_SLAVE_ID     0x60 >> 1  

//add for maintaining easy.++
#define CAMERA_PW_DVDD "wlan"
#define CAMERA_PW_AVDD "gp6"
#define CAMERA_PW_AF "LDO4" 
#define CAMERA_PW_AVDD_Ai1 "rftx"
#define CAMERA_PW_AVDD_DMT "rftx"
char g_camera_pw_avdd[10];
//FIH, Vince, For DMP PR2 
#define HM0357_GPIO_PWD 77 //40
//FIH, Vince, For DMP 
#define CAMIF_SW_AI1 20
#define CAMIF_SW_DMT 17 //DMP PR2, DMO, DMT are using the same pin (17)
//add for maintaining easy.--
#define HM0357_GPIO_PWD_AI1 16
int g_HM0357_GPIO_PWD=0;
int g_HM0357_PRODUCT_ID = 0;
int g_HM0357_PHASE_ID = 0;

struct hm0357_work {
	struct work_struct work;
};

static struct  hm0357_work *hm0357_sensorw;
static struct  i2c_client *hm0357_client;

struct hm0357_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};

extern void brightness_onoff(int on);
static struct hm0357_ctrl *hm0357_ctrl;

/* FIH, Charles Huang, 2009/06/24 { */
/* [FXX_CR], add VFS */
#ifdef CONFIG_FIH_FXX
#define HM0357_USE_VFS
#ifdef HM0357_USE_VFS
#define HM0357_INITREG "initreg"
#define HM0357_OEMREG "oemreg"
#define HM0357_PREVIEWREG "previewreg"
#define HM0357_SNAPREG "snapreg"
#define HM0357_SNAPAEREG "snapaereg"
#define HM0357_IQREG "iqreg"
#define HM0357_LENSREG "lensreg"
#define HM0357_WRITEREG "writereg"
#define HM0357_GETREG "getreg"
#define HM0357_MCLK "mclk"
#define HM0357_MULTIPLE "multiple"
#define HM0357_FLASHTIME "flashtime"

/* MAX buf is ???? */
#define HM0357_MAX_VFS_INIT_INDEX 330
int hm0357_use_vfs_init_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_init_settings_tbl[HM0357_MAX_VFS_INIT_INDEX];
uint16_t hm0357_vfs_init_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_init_settings_tbl);

#define HM0357_MAX_VFS_OEM_INDEX 330
int hm0357_use_vfs_oem_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_oem_settings_tbl[HM0357_MAX_VFS_OEM_INDEX];
uint16_t hm0357_vfs_oem_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_oem_settings_tbl);

#define HM0357_MAX_VFS_PREVIEW_INDEX 330
int hm0357_use_vfs_preview_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_preview_settings_tbl[HM0357_MAX_VFS_PREVIEW_INDEX];
uint16_t hm0357_vfs_preview_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_preview_settings_tbl);

#define HM0357_MAX_VFS_SNAP_INDEX 330
int hm0357_use_vfs_snap_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_snap_settings_tbl[HM0357_MAX_VFS_SNAP_INDEX];
uint16_t hm0357_vfs_snap_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_snap_settings_tbl);

#define HM0357_MAX_VFS_SNAPAE_INDEX 330
int hm0357_use_vfs_snapae_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_snapae_settings_tbl[HM0357_MAX_VFS_SNAPAE_INDEX];
uint16_t hm0357_vfs_snapae_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_snapae_settings_tbl);

#define HM0357_MAX_VFS_IQ_INDEX 330
int hm0357_use_vfs_iq_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_iq_settings_tbl[HM0357_MAX_VFS_IQ_INDEX];
uint16_t hm0357_vfs_iq_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_iq_settings_tbl);

#define HM0357_MAX_VFS_LENS_INDEX 330
int hm0357_use_vfs_lens_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_lens_settings_tbl[HM0357_MAX_VFS_LENS_INDEX];
uint16_t hm0357_vfs_lens_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_lens_settings_tbl);

#define HM0357_MAX_VFS_WRITEREG_INDEX 330
int hm0357_use_vfs_writereg_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_writereg_settings_tbl[HM0357_MAX_VFS_IQ_INDEX];
uint16_t hm0357_vfs_writereg_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_writereg_settings_tbl);

#define HM0357_MAX_VFS_GETREG_INDEX 330
int hm0357_use_vfs_getreg_setting=0;
struct hm0357_i2c_reg_conf hm0357_vfs_getreg_settings_tbl[HM0357_MAX_VFS_GETREG_INDEX];
uint16_t hm0357_vfs_getreg_settings_tbl_size= ARRAY_SIZE(hm0357_vfs_getreg_settings_tbl);

int hm0357_use_vfs_mclk_setting=0;
int hm0357_use_vfs_multiple_setting=0;
int hm0357_use_vfs_flashtime_setting=0;
#endif
#endif
/* } FIH, Charles Huang, 2009/06/24 */

static DECLARE_WAIT_QUEUE_HEAD(hm0357_wait_queue);
DECLARE_MUTEX(hm0357_sem);

/*=============================================================*/
static int hm0357_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;

// add for DMP PR2+++

	if(g_HM0357_PRODUCT_ID==Project_DMP )
	{	
		if( g_HM0357_PHASE_ID >= Phase_PR2 )
		{	
			printk( "hm0357 DMP Phase >= PR2\r\n" );
			gpio_tlmm_config(GPIO_CFG(CAMIF_SW_DMT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
			rc = gpio_request(CAMIF_SW_DMT, "hm0357");
			if (!rc) {
				rc = gpio_direction_output(CAMIF_SW_DMT,1);
				printk( KERN_INFO"gpio_direction_output GPIO_17 value->%d.\n",gpio_get_value(CAMIF_SW_DMT));
			}
			gpio_free(CAMIF_SW_DMT);
			msleep( 5 );
		}
	}
	else if(g_HM0357_PRODUCT_ID==Project_DMT ||g_HM0357_PRODUCT_ID==Project_DMO)
	{	

		gpio_tlmm_config(GPIO_CFG(CAMIF_SW_DMT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_DMT, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_DMT,1);
			printk( KERN_INFO"gpio_direction_output GPIO_17 value->%d.\n",gpio_get_value(CAMIF_SW_DMT));
		}
		gpio_free(CAMIF_SW_DMT);
		msleep( 5 );

	}
	else if(Project_AI1D==g_HM0357_PRODUCT_ID || Project_AI1S==g_HM0357_PRODUCT_ID)
	{
		gpio_tlmm_config(GPIO_CFG( CAMIF_SW_AI1, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_AI1, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_AI1, 1);
		}
		gpio_free(CAMIF_SW_AI1);
		msleep(2);
		printk(KERN_INFO"[HM0357] Project Ai1 gpio_get_value(20) = %d\n", gpio_get_value(20));
   
	}
// add for DMP PR2---

     	gpio_tlmm_config(GPIO_CFG(g_HM0357_GPIO_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_8MA), GPIO_ENABLE);

	rc = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
	
	if (!rc) {
		if(gpio_get_value(g_HM0357_GPIO_PWD)==0)
		{
			gpio_direction_output(g_HM0357_GPIO_PWD, 1);
			printk( KERN_INFO"HM0357 Reset PWD pin.\n");
			msleep(3);
		}
		rc = gpio_direction_output(g_HM0357_GPIO_PWD, 0);
		printk( KERN_INFO"gpio_direction_output dev->sensor_pwd ->0.\n");
	}
	else
	{
		printk( KERN_INFO"gpio_request g_HM0357_GPIO_PWD rc=%d.\n",rc);
		rc = gpio_direction_output(g_HM0357_GPIO_PWD, 0);
		printk( KERN_INFO"force dev->sensor_pwd ->0.\n");
	}
		
	gpio_free(g_HM0357_GPIO_PWD);
	msleep(3);

	printk( KERN_INFO"Finish power up sequence.\n");
	


	
      printk(KERN_INFO"[HM0357 0.3M] gpio_get_value(%d) = %d\n",dev->sensor_reset, gpio_get_value(dev->sensor_reset));
      printk(KERN_INFO"[HM0357 0.3M] gpio_get_value(%d) = %d\n",g_HM0357_GPIO_PWD, gpio_get_value(g_HM0357_GPIO_PWD));
      
      if(gpio_get_value(g_HM0357_GPIO_PWD)==0)
		return 0;
	else
		return rc;
}

static int32_t hm0357_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr   = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(hm0357_client->adapter, msgs, 2) < 0) {
		printk(KERN_ERR "hm0357_i2c_rxdata failed, try again!\n");
		if (i2c_transfer(hm0357_client->adapter, msgs, 2) < 0) {
			printk(KERN_ERR "hm0357_i2c_rxdata failed!\n");
			CDBG("hm0357_i2c_rxdata failed!\n");
			return -EIO;
		}
	}

	return 0;
}

static int32_t hm0357_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum hm0357_width width)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	switch (width) {
	case WORD_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);   

		rc = hm0357_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;
	case BYTE_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);   

		rc = hm0357_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0];
	}
		break;

	default:
		break;
	}

	if (rc < 0)
		CDBG("hm0357_i2c_read failed!\n");

	return rc;
}

static int32_t hm0357_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(hm0357_client->adapter, msg, 1) < 0) {
		printk(KERN_ERR "hm0357_i2c_txdata failed, try again!\n");
		if (i2c_transfer(hm0357_client->adapter, msg, 1) < 0) {
			printk(KERN_ERR "hm0357_i2c_txdata failed\n");
			CDBG("hm0357_i2c_txdata failed\n");
			return -EIO;
		}
	}

	return 0;
}

static int32_t hm0357_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum hm0357_width width)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	uint16_t bytePoll = 0; 
	uint16_t bitPoll = 0;
	uint8_t count = 0;

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

		rc = hm0357_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF);
		
		rc = hm0357_i2c_txdata(saddr, buf, 3);
	}
		break;

	case BYTE_POLL: {
		do {
			count++;
			msleep(10);//msleep(40);
			rc = hm0357_i2c_read(saddr, waddr, &bytePoll, BYTE_LEN);
			//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, bytePoll);
		} while( (bytePoll != wdata) && (count <20) );
	}
		break;

	case BIT_POLL: {
		do {
			count++;
			msleep(10);//msleep(40);
			rc = hm0357_i2c_read(saddr, waddr, &bitPoll, BYTE_LEN);
			//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, bitPoll);
		} while( !(bitPoll & wdata) && (count <20) );
	}
		break;		
		

	default:
		break;
	}

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}



void hm0357_set_value_by_bitmask(uint16_t bitset, uint16_t mask, uint16_t  *new_value)
{
	uint16_t org;

	org= *new_value;
	*new_value = (org&~mask) | (bitset & mask);
}

static int32_t hm0357_i2c_write_table(
	struct hm0357_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = 0;

	for (i = 0; i < num_of_items_in_table; i++) {
		/* illegal addr */
		if (reg_conf_tbl->waddr== 0xFFFF)
			break;
		if (reg_conf_tbl->mask == 0xFFFF)
		{
			rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
				reg_conf_tbl->waddr, reg_conf_tbl->wdata,
				reg_conf_tbl->width);
		}else{
			uint16_t reg_value = 0;
			rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
				reg_conf_tbl->waddr, &reg_value, BYTE_LEN);
			hm0357_set_value_by_bitmask(reg_conf_tbl->wdata,reg_conf_tbl->mask,&reg_value);
			rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
					reg_conf_tbl->waddr, reg_value, BYTE_LEN);
		}
		
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			msleep(reg_conf_tbl->mdelay_time);
		reg_conf_tbl++;
	}

	return rc;
}

#if 0
static int32_t hm0357_set_lens_roll_off(void)
{
	int32_t rc = 0;
	rc = hm0357_i2c_write_table(&hm0357_regs.rftbl[0],
		hm0357_regs.rftbl_size);
	return rc;
}
#endif

static int hm0357_reg_init(void)
{

	int rc;


	/* Configure sensor for Preview mode and Snapshot mode */

	rc = hm0357_i2c_write_table(&hm0357_regs.init_settings_tbl[0],
		hm0357_regs.init_settings_tbl_size);

	if (rc < 0)
		return rc;
	if(Project_DMP==g_HM0357_PRODUCT_ID)
	{
		printk("[HM0357]This is DMP PR2, We need to flip the image.");
		rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
				0x0006, 0x2,
				BYTE_LEN);
	}
	else if(Project_AI1D==g_HM0357_PRODUCT_ID || Project_AI1S==g_HM0357_PRODUCT_ID)
	{
		printk("[HM0357]This is Ai1, We need to flip the image.");
		rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
				0x0006, 0x2,
				BYTE_LEN);
	}
	else if(Project_DMO==g_HM0357_PRODUCT_ID || Project_DMT==g_HM0357_PRODUCT_ID)
	{
		printk("[HM0357]This is DMO/DMT, We need to flip the image.");
		rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
				0x0006, 0x1,
				BYTE_LEN);
	}


	return 0;
}

static long hm0357_set_sensor_mode(int mode)
{
	long rc = 0;

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Configure sensor for Preview mode */
/* FIH, Charles Huang, 2009/06/24 { */
/* [FXX_CR], add VFS */
#ifdef CONFIG_FIH_FXX
#ifdef HM0357_USE_VFS
		if (hm0357_use_vfs_preview_setting)
			rc = hm0357_i2c_write_table(&hm0357_vfs_preview_settings_tbl[0],
				hm0357_vfs_preview_settings_tbl_size);
		else
#endif
#endif
/* } FIH, Charles Huang, 2009/06/24 */
			rc = hm0357_i2c_write_table(&hm0357_regs.preview_settings_tbl[0],
				hm0357_regs.preview_settings_tbl_size);

		if (rc < 0)
			return rc;

	/* Configure sensor for customer settings */
/* FIH, Charles Huang, 2009/06/24 { */
/* [FXX_CR], add VFS */
#ifdef CONFIG_FIH_FXX
#ifdef HM0357_USE_VFS
		if (hm0357_use_vfs_oem_setting)
			rc = hm0357_i2c_write_table(&hm0357_vfs_oem_settings_tbl[0],
				hm0357_vfs_oem_settings_tbl_size);
		else
#endif
#endif
/* } FIH, Charles Huang, 2009/06/24 */
			rc = hm0357_i2c_write_table(&hm0357_regs.oem_settings_tbl[0],
				hm0357_regs.oem_settings_tbl_size);

		if (rc < 0)
			return rc;

		msleep(5);
		break;

	case SENSOR_SNAPSHOT_MODE:
		break;
	default:
		return -EFAULT;
	}

	return 0;
}

static long hm0357_set_effect(int mode, int effect)
{
	long rc = 0;

	CDBG("hm0357_set_effect, mode = %d, effect = %d\n",
		mode, effect);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Context A Special Effects */
		break;

	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		break;

	default:
		break;
	}

	switch (effect) {
	case CAMERA_EFFECT_OFF: {//Normal
		struct hm0357_i2c_reg_conf const hm0357_effect_off_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_off_tbl[0],
				ARRAY_SIZE(hm0357_effect_off_tbl));

		if (rc < 0)
			return rc;
	}
			break;

	case CAMERA_EFFECT_MONO: {//B&W
		struct hm0357_i2c_reg_conf const hm0357_effect_mono_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_mono_tbl[0],
				ARRAY_SIZE(hm0357_effect_mono_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_NEGATIVE: {//Negative
		struct hm0357_i2c_reg_conf const hm0357_effect_negative_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_negative_tbl[0],
				ARRAY_SIZE(hm0357_effect_negative_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_SEPIA: {
		struct hm0357_i2c_reg_conf const hm0357_effect_sepia_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_sepia_tbl[0],
				ARRAY_SIZE(hm0357_effect_sepia_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_BLUISH: {//Bluish
		struct hm0357_i2c_reg_conf const hm0357_effect_bluish_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_bluish_tbl[0],
				ARRAY_SIZE(hm0357_effect_bluish_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_REDDISH: {//Reddish
		struct hm0357_i2c_reg_conf const hm0357_effect_reddish_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_reddish_tbl[0],
				ARRAY_SIZE(hm0357_effect_reddish_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_EFFECT_GREENISH: {//Greenish
		struct hm0357_i2c_reg_conf const hm0357_effect_greenish_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_effect_greenish_tbl[0],
				ARRAY_SIZE(hm0357_effect_greenish_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	default: {
		if (rc < 0)
			return rc;

		/*return -EFAULT;*/
		/* -EFAULT makes app fail */
		return 0;
	}
	}

	return rc;
}

static long hm0357_set_wb(int mode, int wb)
{
	long rc = 0;

	CDBG("hm0357_set_wb, mode = %d, wb = %d\n",
		mode, wb);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Context A Special Effects */
		break;

	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		break;

	default:
		break;
	}

	switch (wb) {
	case CAMERA_WB_AUTO: {
		struct hm0357_i2c_reg_conf const hm0357_wb_auto_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_wb_auto_tbl[0],
				ARRAY_SIZE(hm0357_wb_auto_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_WB_INCANDESCENT: {//TUNGSTEN
		struct hm0357_i2c_reg_conf const hm0357_wb_incandescent_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_wb_incandescent_tbl[0],
				ARRAY_SIZE(hm0357_wb_incandescent_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_WB_FLUORESCENT: {//Office
		struct hm0357_i2c_reg_conf const hm0357_wb_fluorescent_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_wb_fluorescent_tbl[0],
				ARRAY_SIZE(hm0357_wb_fluorescent_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_WB_DAYLIGHT: {//Sunny
		struct hm0357_i2c_reg_conf const hm0357_wb_daylight_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_wb_daylight_tbl[0],
				ARRAY_SIZE(hm0357_wb_daylight_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_WB_CLOUDY_DAYLIGHT: {//Cloudy
		struct hm0357_i2c_reg_conf const hm0357_wb_cloudydaylight_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_wb_cloudydaylight_tbl[0],
				ARRAY_SIZE(hm0357_wb_cloudydaylight_tbl));

		if (rc < 0)
			return rc;

		rc = hm0357_i2c_write(HM0357_I2C_WRITE_SLAVE_ID,
				0x3405, 0x45, BYTE_LEN);

		if (rc < 0)
			return rc;
	}
		break;

	default: {
		if (rc < 0)
			return rc;

		/*return -EFAULT;*/
		/* -EFAULT makes app fail */
		return 0;
	}
	}

	return rc;
}


static long hm0357_set_brightness(int mode, int brightness)
{
	long rc = 0;

	CDBG("hm0357_set_brightness, mode = %d, brightness = %d\n",
		mode, brightness);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Context A Special Effects */
		break;

	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		break;

	default:
		break;
	}

	switch (brightness) {
	case CAMERA_BRIGHTNESS_0: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_0_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_0_tbl[0],
				ARRAY_SIZE(hm0357_brightness_0_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_BRIGHTNESS_1: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_1_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_1_tbl[0],
				ARRAY_SIZE(hm0357_brightness_1_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_BRIGHTNESS_2: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_2_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_2_tbl[0],
				ARRAY_SIZE(hm0357_brightness_2_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_BRIGHTNESS_3: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_3_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_3_tbl[0],
				ARRAY_SIZE(hm0357_brightness_3_tbl));

		if (rc < 0)
			return rc;
	}
		break;

	case CAMERA_BRIGHTNESS_4: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_4_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_4_tbl[0],
				ARRAY_SIZE(hm0357_brightness_4_tbl));

		if (rc < 0)
			return rc;
	} 
		break;

	case CAMERA_BRIGHTNESS_5: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_5_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_5_tbl[0],
				ARRAY_SIZE(hm0357_brightness_5_tbl));

		if (rc < 0)
			return rc;
	} 
		break;

	case CAMERA_BRIGHTNESS_6: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_6_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_6_tbl[0],
				ARRAY_SIZE(hm0357_brightness_6_tbl));

		if (rc < 0)
			return rc;
	} 
		break;

	case CAMERA_BRIGHTNESS_7:
	case CAMERA_BRIGHTNESS_8:
	case CAMERA_BRIGHTNESS_9:
	case CAMERA_BRIGHTNESS_10: {
		struct hm0357_i2c_reg_conf const hm0357_brightness_10_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_brightness_10_tbl[0],
				ARRAY_SIZE(hm0357_brightness_10_tbl));

		if (rc < 0)
			return rc;
	} 
		break;

	default: {
		if (rc < 0)
			return rc;

		/*return -EFAULT;*/
		/* -EFAULT makes app fail */
		return 0;
	}
	}

	return rc;
}

static long hm0357_set_antibanding(int mode, int antibanding)
{
	long rc = 0;

	CDBG("hm0357_set_antibanding, mode = %d, antibanding = %d\n",
		mode, antibanding);

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		/* Context A Special Effects */
		break;

	case SENSOR_SNAPSHOT_MODE:
		/* Context B Special Effects */
		break;

	default:
		break;
	}

	switch (antibanding) {
	case CAMERA_ANTIBANDING_OFF: {
		struct hm0357_i2c_reg_conf const hm0357_antibanding_off_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_antibanding_off_tbl[0],
				ARRAY_SIZE(hm0357_antibanding_off_tbl));

		if (rc < 0)
			return rc;

	}
			break;

	case CAMERA_ANTIBANDING_60HZ: {
		struct hm0357_i2c_reg_conf const hm0357_antibanding_60hz_tbl[] = {
			{0x120, 0x1, BYTE_LEN, 0, 0xFFFF},
		};

		rc = hm0357_i2c_write_table(&hm0357_antibanding_60hz_tbl[0],
				ARRAY_SIZE(hm0357_antibanding_60hz_tbl));

		if (rc < 0)
			return rc;

	}
		break;

	case CAMERA_ANTIBANDING_50HZ: {
		struct hm0357_i2c_reg_conf const hm0357_antibanding_60hz_tbl[] = {
			{0x120, 0x0, BYTE_LEN, 0, 0xFFFF},
		};

		rc = hm0357_i2c_write_table(&hm0357_antibanding_60hz_tbl[0],
				ARRAY_SIZE(hm0357_antibanding_60hz_tbl));

		if (rc < 0)
			return rc;

	}
		break;

	case CAMERA_ANTIBANDING_AUTO: {
		struct hm0357_i2c_reg_conf const hm0357_antibanding_60hz_tbl[] = {
		};

		rc = hm0357_i2c_write_table(&hm0357_antibanding_60hz_tbl[0],
				ARRAY_SIZE(hm0357_antibanding_60hz_tbl));

		if (rc < 0)
			return rc;

	}
		break;

	default: {
		if (rc < 0)
			return rc;

		return -EINVAL;
	}
	}

	return rc;
}

int hm0357_power_on(void)
{
	struct vreg *vreg_camera;
	int rc2;
	
	rc2 = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
	if (!rc2) {
		rc2 = gpio_direction_output(g_HM0357_GPIO_PWD, 0);
	}
	gpio_free(g_HM0357_GPIO_PWD);
	
	vreg_camera = vreg_get(NULL, g_camera_pw_avdd);
	printk(KERN_INFO "[CAMERA_PS] power pin g_camera_pw_avdd=%s- VREG_CAM_UXGA_ACORE_V2P8 ON.\n",g_camera_pw_avdd);
	if (IS_ERR(vreg_camera)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
		return PTR_ERR(vreg_camera);
	}
	printk(KERN_ERR "%s: vreg g_camera_pw_avdd got!\n", __func__);

	
	rc2 = vreg_set_level(vreg_camera, 2800);
	if (rc2) {
		printk(KERN_ERR "%s: vreg set level to 2.8V failed (%d)\n", __func__, rc2);
		return -EIO;
	}	
	rc2 = vreg_enable(vreg_camera);
	if (rc2) {
		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc2);
		return -EIO;
	}

	msleep(2);
	return rc2;
}

int hm0357_power_off(void)
{
	struct vreg *vreg_camera;
	int rc2,rc;

//	printk(KERN_INFO "[CAMERA_PS] power off but do nothing..\n");
//	return 0;

	rc = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
	if (!rc) {
		rc = gpio_direction_output(g_HM0357_GPIO_PWD, 1);
	}
	gpio_free(g_HM0357_GPIO_PWD);



	(void)pmic_vreg_pull_down_switch(ON_CMD, PM_VREG_PDOWN_WLAN_ID);
	(void)pmic_vreg_pull_down_switch(ON_CMD, PM_VREG_PDOWN_GP6_ID);	

//vince test 0210+++
//DMP PR2, and AI1, use this pin with main camera.
//We can not disable this pin since the main camera may be in standby mode, which  also need this power.
//But DMO/DMT does not have standby mode, so it should be disable.
	if(g_HM0357_PRODUCT_ID==Project_DMT ||g_HM0357_PRODUCT_ID==Project_DMO)
	{
		vreg_camera = vreg_get(NULL, g_camera_pw_avdd);
		printk(KERN_INFO "[CAMERA_PS] power pin - VREG_CAM_UXGA_ACORE_V2P8 OFF.\n");
		rc2 = vreg_disable(vreg_camera);
		if (rc2) {
			printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc2);
			return -EIO;
		}
	}

//vince test 0210---
       printk(KERN_INFO"[HM0357 0.3M] gpio_get_value(%d) = %d\n",g_HM0357_GPIO_PWD, gpio_get_value(g_HM0357_GPIO_PWD));
	   
	return rc2;
}


static int hm0357_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
/* FIH, Henry Juang, 2010/11/01 { */
/* [FXX_CR], Add retry to prevent VGA blocked.*/
	int rc = 0,i=0,total=0;
/* FIH, Henry Juang, 2010/11/01 } */

	/* OV suggested Power up block End */
	/* Read the Model ID of the sensor */
	/* Read REG_HM0357_MODEL_ID_HI & REG_HM0357_MODEL_ID_LO */
	rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
		REG_HM0357_MODEL_ID_HI, &model_id, BYTE_LEN);
/* FIH, Henry Juang, 2010/11/01 { */
/* [FXX_CR], Add retry to prevent VGA blocked.*/	
	while(rc<0 && i<10) 
	{	
		gpio_tlmm_config(GPIO_CFG(g_HM0357_GPIO_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		rc = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(g_HM0357_GPIO_PWD, 1);
		}
		msleep(10);
		rc = gpio_direction_output(g_HM0357_GPIO_PWD, 0); 
		gpio_free(g_HM0357_GPIO_PWD);		
		total+=10+i*25;
		printk(KERN_WARNING"hm0357_sensor_init_probe failed and retry after sleep %d ms. %d times!,total=%d ms.\n ",10+i*25,i+1,total);
		msleep(10+i*25);
		rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,	REG_HM0357_MODEL_ID_HI, &model_id, BYTE_LEN);
		i++;
	}
/* FIH, Henry Juang, 2010/11/01 } */	
	
	if (rc < 0)
		goto init_probe_fail;

	printk("hm0357 model_id = 0x%x\n", model_id);

	rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
		REG_HM0357_MODEL_ID_LO, &model_id, BYTE_LEN);
	if (rc < 0)
		goto init_probe_fail;

	printk("hm0357 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	//if (model_id != HM0357_MODEL_ID) {
	//	rc = -EFAULT;
	//	goto init_probe_fail;
	//}

	/* Get version */
	//rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
	//0x302A, &model_id, BYTE_LEN);
	//CDBG("hm0357 version reg 0x302A = 0x%x\n", model_id);

	rc = hm0357_reg_init();
	if (rc < 0)
		goto init_probe_fail;

	return rc;

init_probe_fail:
	return rc;
}

int hm0357_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	struct vreg *vreg_camera;
/*
	rc=hm0357_power_on();
	if (rc < 0) {
		CDBG("reset failed!\n");
		printk("hm0357_power_on failed in hm0357_sensor_init!\n");
		goto init_fail;
	}
*/
	vreg_camera = vreg_get(NULL, g_camera_pw_avdd);
	printk(KERN_INFO "[CAMERA_PS] power pin g_camera_pw_avdd=%s- VREG_CAM_UXGA_ACORE_V2P8 ON.\n",g_camera_pw_avdd);
	if (IS_ERR(vreg_camera)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
		goto init_fail;
	}
	
	rc = vreg_set_level(vreg_camera, 2800);
	if (rc) {
		printk(KERN_ERR "%s: vreg set level to 2.8V failed (%d)\n", __func__, rc);
		goto init_fail;
	}	
	rc = vreg_enable(vreg_camera);
	if (rc) {
		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc);
		goto init_fail;
	}
	printk(KERN_ERR "%s: vreg g_camera_pw_avdd got and set 2800!\n", __func__);


	rc = hm0357_reset(data);
	if (rc < 0) {
		CDBG("reset failed!\n");
		goto init_fail;
	}

	/* EVB CAMIF cannont handle in 24MHz */
	/* EVB use 12.8MHz */
	/* R4215 couldn't use too high clk */
	printk(KERN_ERR "%s: hm0357_reset done!\n", __func__);
	gpio_tlmm_config(GPIO_CFG(15, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	msm_camio_clk_rate_set(24000000);
	msleep(25);

	msm_camio_camif_pad_reg_reset();

	rc = hm0357_sensor_init_probe(data);
	if (rc < 0) {
		CDBG("hm0357_sensor_init failed!\n");
		goto init_fail;
	}

//init_done:
	return rc;

init_fail:
	kfree(hm0357_ctrl);
	return rc;
}

static int hm0357_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&hm0357_wait_queue);
	return 0;
}

int hm0357_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(
				&cfg_data,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&hm0357_sem); */

	CDBG("hm0357_sensor_config, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = hm0357_set_sensor_mode(
					cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		rc = hm0357_set_effect(
					cfg_data.mode,
					cfg_data.cfg.effect);
		break;

	case CFG_START:
		rc = -EFAULT;
		break;
		
	case CFG_PWR_UP:
		rc = -EFAULT;
		break;

	case CFG_PWR_DOWN:
		rc = -EFAULT;
		break;

	case CFG_WRITE_EXPOSURE_GAIN:
		rc = -EFAULT;
		break;

	case CFG_MOVE_FOCUS:
		rc = -EFAULT;
		break;

	case CFG_REGISTER_TO_REAL_GAIN:
		rc = -EFAULT;
		break;

	case CFG_REAL_TO_REGISTER_GAIN:
		rc = -EFAULT;
		break;

	case CFG_SET_FPS:
		rc = -EFAULT;
		break;

	case CFG_SET_PICT_FPS:
		rc = -EFAULT;
		break;

	case CFG_SET_BRIGHTNESS:
		rc = hm0357_set_brightness(
					cfg_data.mode,
					cfg_data.cfg.brightness);
		break;

	case CFG_SET_CONTRAST:
		rc = -EFAULT;
		break;

	case CFG_SET_EXPOSURE_MODE:
		rc = -EFAULT;
		break;

	case CFG_SET_WB:
		rc = hm0357_set_wb(
					cfg_data.mode,
					cfg_data.cfg.wb);
		break;

	case CFG_SET_ANTIBANDING:
		rc = hm0357_set_antibanding(
					cfg_data.mode,
					cfg_data.cfg.antibanding);
		break;

	case CFG_SET_EXP_GAIN:
		rc = -EFAULT;
		break;

	case CFG_SET_PICT_EXP_GAIN:
		rc = -EFAULT;
		break;

	case CFG_SET_LENS_SHADING:
		rc = -EFAULT;
		break;

	case CFG_GET_PICT_FPS:
		rc = -EFAULT;
		break;

	case CFG_GET_PREV_L_PF:
		rc = -EFAULT;
		break;

	case CFG_GET_PREV_P_PL:
		rc = -EFAULT;
		break;

	case CFG_GET_PICT_L_PF:
		rc = -EFAULT;
		break;

	case CFG_GET_PICT_P_PL:
		rc = -EFAULT;
		break;

	case CFG_GET_AF_MAX_STEPS:
		rc = -EFAULT;
		break;

	case CFG_GET_PICT_MAX_EXP_LC:
		rc = -EFAULT;
		break;

	case CFG_SET_LEDMOD:
		rc = -EFAULT;
		break;

	default:
		rc = -EINVAL;
		break;
	}

	/* up(&hm0357_sem); */

	return rc;
}

int hm0357_sensor_release(void)
{
	int rc = 0;
	struct vreg *vreg_camera;
	/* down(&hm0357_sem); */
	printk("hm0357_sensor_release()+++\n");
	
	rc = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
	gpio_direction_output(g_HM0357_GPIO_PWD, 1);
	printk(KERN_INFO "HM0357_GPIO_PWD GPIO_%d= %d.\n",g_HM0357_GPIO_PWD,gpio_get_value(g_HM0357_GPIO_PWD));
	gpio_free(g_HM0357_GPIO_PWD);
	msleep(3);	

//vince test 0210+++
//DMP PR2, and AI1, use this pin with main camera.
//We can not disable this pin since the main camera may be in standby mode, which  also need this power.
//But DMO/DMT does not have standby mode, so it should be disable.
	if(g_HM0357_PRODUCT_ID==Project_DMT ||g_HM0357_PRODUCT_ID==Project_DMO)
	{
		vreg_camera = vreg_get(NULL, g_camera_pw_avdd);
		printk(KERN_INFO "[CAMERA_PS] power pin - VREG_CAM_UXGA_ACORE_V2P8 OFF.\n");
		rc = vreg_disable(vreg_camera);
		if (rc) {
			printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc);
			return -EIO;
		}
	}

//vince test 0210---
//	msm_camio_clk_disable(CAMIO_VFE_CLK);

	mutex_lock(&hm0357_mut);
	kfree(hm0357_ctrl);
	hm0357_ctrl = NULL;
	/* up(&hm0357_sem); */
	mutex_unlock(&hm0357_mut);

	printk("hm0357_sensor_release()---\n");
	return rc;
}

/* FIH, Charles Huang, 2009/06/24 { */
/* [FXX_CR], add VFS */
#ifdef CONFIG_FIH_FXX
#ifdef HM0357_USE_VFS
void hm0357_get_param(const char *buf, size_t count, struct hm0357_i2c_reg_conf *tbl, 
	unsigned short tbl_size, int *use_setting)
{
	unsigned short waddr;
	unsigned short wdata;
	enum hm0357_width width;
	unsigned short mdelay_time;
	unsigned short mask;
	char param1[10],param2[10],param3[10];
	int read_count;
	const char *pstr;
	int vfs_index=0;
	pstr=buf;

	CDBG("count=%d\n",count);
	do
	{
		read_count=sscanf(pstr,"%4s,%2s,%2s",param1,param2,param3);

      		//CDBG("pstr=%s\n",pstr);
      		//CDBG("read_count=%d,count=%d\n",read_count,count);
		if (read_count ==3)
		{
			waddr=simple_strtoul(param1,NULL,16);
			wdata=simple_strtoul(param2,NULL,16);
			width=1;
			mdelay_time=0;
			mask=simple_strtoul(param3,NULL,16);
				
			tbl[vfs_index].waddr= waddr;
			tbl[vfs_index].wdata= wdata;
			tbl[vfs_index].width= width;
			tbl[vfs_index].mdelay_time= mdelay_time;
			tbl[vfs_index].mask= mask;
			vfs_index++;

			if (vfs_index == tbl_size)
			{
				CDBG("Just match MAX_VFS_INDEX\n");
				*use_setting=1;
			}else if (vfs_index > tbl_size)
			{
				CDBG("Out of range MAX_VFS_INDEX\n");
				*use_setting=0;
				break;
			}
			
       		//CDBG("param1=%s,param2=%s,param3=%s\n",param1,param2,param3);
       		//CDBG("waddr=0x%04X,wdata=0x%04X,width=%d,mdelay_time=%d,mask=0x%04X\n",waddr,wdata,width,mdelay_time,mask);
		}else{
			tbl[vfs_index].waddr= 0xFFFF;
			tbl[vfs_index].wdata= 0xFFFF;
			tbl[vfs_index].width= 1;
			tbl[vfs_index].mdelay_time= 0xFFFF;
			tbl[vfs_index].mask= 0xFFFF;
			*use_setting=1;
			break;
		}
		/* get next line */
		pstr=strchr(pstr, '\n');
		if (pstr==NULL)
			break;
		pstr++;
	}while(read_count!=0);


}

static ssize_t hm0357_write_initreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_init_settings_tbl[0], hm0357_vfs_init_settings_tbl_size, &hm0357_use_vfs_init_setting);
	return count;
}

static ssize_t hm0357_write_oemtreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_oem_settings_tbl[0], hm0357_vfs_oem_settings_tbl_size, &hm0357_use_vfs_oem_setting);
	return count;
}

static ssize_t hm0357_write_previewreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_preview_settings_tbl[0], hm0357_vfs_preview_settings_tbl_size, &hm0357_use_vfs_preview_setting);
	return count;
}

static ssize_t hm0357_write_snapreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_snap_settings_tbl[0], hm0357_vfs_snap_settings_tbl_size, &hm0357_use_vfs_snap_setting);
	return count;
}

static ssize_t hm0357_write_snapaereg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_snapae_settings_tbl[0], hm0357_vfs_snapae_settings_tbl_size, &hm0357_use_vfs_snapae_setting);
	return count;
}

static ssize_t hm0357_write_iqreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_iq_settings_tbl[0], hm0357_vfs_iq_settings_tbl_size, &hm0357_use_vfs_iq_setting);
	return count;
}

static ssize_t hm0357_write_lensreg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_lens_settings_tbl[0], hm0357_vfs_lens_settings_tbl_size, &hm0357_use_vfs_lens_setting);
	return count;
}

static ssize_t hm0357_write_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	long rc = 0;

	hm0357_get_param(buf, count, &hm0357_vfs_writereg_settings_tbl[0], hm0357_vfs_writereg_settings_tbl_size, &hm0357_use_vfs_writereg_setting);
	if (hm0357_use_vfs_writereg_setting)
	{
		rc = hm0357_i2c_write_table(&hm0357_vfs_writereg_settings_tbl[0],
			hm0357_vfs_writereg_settings_tbl_size);
		hm0357_use_vfs_writereg_setting =0;
	}
	return count;
}

static ssize_t hm0357_setrange(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	hm0357_get_param(buf, count, &hm0357_vfs_getreg_settings_tbl[0], hm0357_vfs_getreg_settings_tbl_size, &hm0357_use_vfs_getreg_setting);
	return count;
}

static ssize_t hm0357_getrange(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i,rc;
	char *str = buf;

	if (hm0357_use_vfs_getreg_setting)
	{
		for (i=0;i<=hm0357_vfs_getreg_settings_tbl_size;i++)
		{
			if (hm0357_vfs_getreg_settings_tbl[i].waddr==0xFFFF)
				break;

			rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
				hm0357_vfs_getreg_settings_tbl[i].waddr, &(hm0357_vfs_getreg_settings_tbl[i].wdata), BYTE_LEN);
			CDBG("hm0357 reg 0x%4X = 0x%2X\n", hm0357_vfs_getreg_settings_tbl[i].waddr, hm0357_vfs_getreg_settings_tbl[i].wdata);

			str += sprintf(str, "%04X,%2X,%2X\n", hm0357_vfs_getreg_settings_tbl[i].waddr, 
				hm0357_vfs_getreg_settings_tbl[i].wdata, 
				hm0357_vfs_getreg_settings_tbl[i].mask);

			if (rc <0)
				break;
		}
	}
	return (str - buf);
}

static ssize_t hm0357_setmclk(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf,"%d",&hm0357_use_vfs_mclk_setting);
	return count;
}

static ssize_t hm0357_getmclk(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (sprintf(buf,"%d\n",hm0357_use_vfs_mclk_setting));
}

static ssize_t hm0357_setmultiple(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf,"%d",&hm0357_use_vfs_multiple_setting);
	return count;
}

static ssize_t hm0357_getmultiple(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (sprintf(buf,"%d\n",hm0357_use_vfs_multiple_setting));
}

static ssize_t hm0357_setflashtime(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf,"%d",&hm0357_use_vfs_flashtime_setting);
	return count;
}

static ssize_t hm0357_getflashtime(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (sprintf(buf,"%d\n",hm0357_use_vfs_flashtime_setting));
}

DEVICE_ATTR(initreg_hm0357, 0666, NULL, hm0357_write_initreg);
DEVICE_ATTR(oemreg_hm0357, 0666, NULL, hm0357_write_oemtreg);
DEVICE_ATTR(previewreg_hm0357, 0666, NULL, hm0357_write_previewreg);
DEVICE_ATTR(snapreg_hm0357, 0666, NULL, hm0357_write_snapreg);
DEVICE_ATTR(snapregae_hm0357, 0666, NULL, hm0357_write_snapaereg);
DEVICE_ATTR(iqreg_hm0357, 0666, NULL, hm0357_write_iqreg);
DEVICE_ATTR(lensreg_hm0357, 0666, NULL, hm0357_write_lensreg);
DEVICE_ATTR(writereg_hm0357, 0666, NULL, hm0357_write_reg);
DEVICE_ATTR(getreg_hm0357, 0666, hm0357_getrange, hm0357_setrange);
DEVICE_ATTR(mclk_hm0357, 0666, hm0357_getmclk, hm0357_setmclk);
DEVICE_ATTR(multiple_hm0357, 0666, hm0357_getmultiple, hm0357_setmultiple);
DEVICE_ATTR(flashtime_hm0357, 0666, hm0357_getflashtime, hm0357_setflashtime);
#if 0
static int create_attributes(struct i2c_client *client)
{
	int rc;

	dev_attr_initreg_hm0357.attr.name = HM0357_INITREG;
	dev_attr_oemreg_hm0357.attr.name = HM0357_OEMREG;
	dev_attr_previewreg_hm0357.attr.name = HM0357_PREVIEWREG;
	dev_attr_snapreg_hm0357.attr.name = HM0357_SNAPREG;
	dev_attr_snapregae_hm0357.attr.name = HM0357_SNAPAEREG;
	dev_attr_iqreg_hm0357.attr.name = HM0357_IQREG;
	dev_attr_lensreg_hm0357.attr.name = HM0357_LENSREG;
	dev_attr_writereg_hm0357.attr.name = HM0357_WRITEREG;
	dev_attr_getreg_hm0357.attr.name = HM0357_GETREG;
	dev_attr_mclk_hm0357.attr.name = HM0357_MCLK;
	dev_attr_multiple_hm0357.attr.name = HM0357_MULTIPLE;
	dev_attr_flashtime_hm0357.attr.name = HM0357_FLASHTIME;

	rc = device_create_file(&client->dev, &dev_attr_initreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"initreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}

	rc = device_create_file(&client->dev, &dev_attr_oemreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"oemreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}

	rc = device_create_file(&client->dev, &dev_attr_previewreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"previewreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}

	rc = device_create_file(&client->dev, &dev_attr_snapreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"snapreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_snapregae_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"snapregae\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_iqreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"iqreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_lensreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"lensreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_writereg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"writereg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_getreg_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"getreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_mclk_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"mclk\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_multiple_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"multiple\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_flashtime_hm0357);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create hm0357 attribute \"flashtime\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	return rc;
}
#endif

static int remove_attributes(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_initreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_oemreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_previewreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_snapreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_snapregae_hm0357);
	device_remove_file(&client->dev, &dev_attr_iqreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_lensreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_writereg_hm0357);
	device_remove_file(&client->dev, &dev_attr_getreg_hm0357);
	device_remove_file(&client->dev, &dev_attr_mclk_hm0357);
	device_remove_file(&client->dev, &dev_attr_multiple_hm0357);
	device_remove_file(&client->dev, &dev_attr_flashtime_hm0357);

	return 0;
}
#endif /* HM0357_USE_VFS */
#endif
/* } FIH, Charles Huang, 2009/06/24 */

static int hm0357_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	hm0357_sensorw =
		kzalloc(sizeof(struct hm0357_work), GFP_KERNEL);

	if (!hm0357_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, hm0357_sensorw);
	hm0357_init_client(client);
	hm0357_client = client;
	
	CDBG("hm0357_probe successed!\n");

	return 0;

probe_failure:
	kfree(hm0357_sensorw);
	hm0357_sensorw = NULL;
	CDBG("hm0357_probe failed!\n");
	return rc;
}

static int __exit hm0357_i2c_remove(struct i2c_client *client)
{
	struct hm0357_work *sensorw = i2c_get_clientdata(client);

/* FIH, Charles Huang, 2009/06/24 { */
/* [FXX_CR], add VFS */
#ifdef CONFIG_FIH_FXX
#ifdef HM0357_USE_VFS
	remove_attributes(client);
#endif /* HM0357_USE_VFS */
#endif
/* } FIH, Charles Huang, 2009/06/24 */

	free_irq(client->irq, sensorw);
	hm0357_client = NULL;
	hm0357_sensorw = NULL;
	kfree(sensorw);
	return 0;
}

#ifdef CONFIG_PM
static int hm0357_suspend(struct i2c_client *client, pm_message_t mesg)
{
/* FIH, Charles Huang, 2009/06/25 { */
/* [FXX_CR], suspend/resume for pm */
#ifdef CONFIG_FIH_FXX
	/* FIH, IvanCHTang, 2010/6/1 { */
	/* [FXX_CR], Set CAMIF_PD_VGA by HWID for FM6/FN6 - [FM6#0003] */
	//struct mpp *mpp_22;
	int HWID=FIH_READ_HWID_FROM_SMEM();
	if((HWID>=CMCS_FN6_PR1)&&(HWID<=CMCS_FN6_MP1))
	{
		/* FIH, IvanCHTang, 2010/7/7 { */
		/* [FXX_CR], [camera] DVDD (msme2 -> mpp1) & VGA_PD (mpp22 -> gpio<108>) for FN6 PR2 - [FM6#0010] */
		if(HWID>=CMCS_FN6_PR2)
		{
			gpio_tlmm_config(GPIO_CFG(108, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
			gpio_direction_output(108, 1);
		}
		else
			mpp_config_digital_out(mpp_22, MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_HIGH));
		/* } FIH, IvanCHTang, 2010/7/7 */
	}
	else if((HWID>=CMCS_128_FM6_PR1)&&(HWID<=CMCS_125_FM6_MP))
	{
		/* sensor_pwd pin gpio122 */
		gpio_direction_output(122,1);
	}
	/* } FIH, IvanCHTang, 2010/6/1 */
#endif
/* } FIH, Charles Huang, 2009/06/25 */
	return 0;
}

static int hm0357_resume(struct i2c_client *client)
{
/* FIH, Charles Huang, 2009/06/25 { */
/* [FXX_CR], suspend/resume for pm */
#ifdef CONFIG_FIH_FXX
	/* FIH, IvanCHTang, 2010/6/1 { */
	/* [FXX_CR], Set CAMIF_PD_VGA by HWID for FM6/FN6 - [FM6#0003] */
	//struct mpp *mpp_22;
	int HWID=FIH_READ_HWID_FROM_SMEM();
	if((HWID>=CMCS_FN6_PR1)&&(HWID<=CMCS_FN6_MP1))
	{
		/* FIH, IvanCHTang, 2010/7/7 { */
		/* [FXX_CR], [camera] DVDD (msme2 -> mpp1) & VGA_PD (mpp22 -> gpio<108>) for FN6 PR2 - [FM6#0010] */
		if(HWID>=CMCS_FN6_PR2)
		{
			gpio_tlmm_config(GPIO_CFG(108, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
			gpio_direction_output(108, 1);
		}
		else
			mpp_config_digital_out(mpp_22, MPP_CFG(MPP_DLOGIC_LVL_MSMP, MPP_DLOGIC_OUT_CTRL_HIGH));
		/* } FIH, IvanCHTang, 2010/7/7 */
	}
	else if((HWID>=CMCS_128_FM6_PR1)&&(HWID<=CMCS_125_FM6_MP))
	{
		/* sensor_pwd pin gpio122 */
		/* Handle by sensor initialization */
		/* workable setting for waste power while resuming */
		gpio_direction_output(122,1);
	}
	/* } FIH, IvanCHTang, 2010/6/1 */
#endif
/* } FIH, Charles Huang, 2009/06/25 */
	return 0;
}
#else
# define hm0357_suspend NULL
# define hm0357_resume  NULL
#endif

static const struct i2c_device_id hm0357_i2c_id[] = {
	{ "hm0357", 0},
	{ },
};

static struct i2c_driver hm0357_i2c_driver = {
	.id_table = hm0357_i2c_id,
	.probe  = hm0357_i2c_probe,
	.remove = __exit_p(hm0357_i2c_remove),
	.suspend  	= hm0357_suspend,
	.resume   	= hm0357_resume,
	.driver = {
		.name = "hm0357",
	},
};


static int hm0357_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&hm0357_i2c_driver),i,total;
	uint16_t model_id = 0;
	struct vreg *vreg_camera;
	printk(KERN_INFO "hm0357_sensor_probe is called!\n");
	if (rc < 0 || hm0357_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	if(fih_read_new_hwid_mech_from_orighwid())
   	{
   		g_HM0357_PRODUCT_ID = fih_read_product_id_from_orighwid() ;      
   		g_HM0357_PHASE_ID = fih_read_phase_id_from_orighwid();
   	}
	printk( "hm0357 g_PRODUCT_ID:%d, g_PHASE_ID:%d\r\n", g_HM0357_PRODUCT_ID, g_HM0357_PHASE_ID );
	
	if(Project_AI1D==g_HM0357_PRODUCT_ID || Project_AI1S==g_HM0357_PRODUCT_ID)
	{
		g_HM0357_GPIO_PWD=HM0357_GPIO_PWD_AI1;
		strcpy (g_camera_pw_avdd,CAMERA_PW_AVDD_Ai1);
		//g_camera_pw_avdd='rftx';//CAMERA_PW_AVDD_Ai1;
		printk( "Project Ai1: hm0357 hwGpioPin_cam_if_sw:GPIO-20\r\n" );
		gpio_tlmm_config(GPIO_CFG( CAMIF_SW_AI1, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_AI1, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_AI1, 1);
		}
		gpio_free(CAMIF_SW_AI1);
		msleep(2);
		printk(KERN_INFO"[HM0357] gpio_get_value(20) = %d\n", gpio_get_value(CAMIF_SW_AI1));
   
	}
	else if(Project_DMT==g_HM0357_PRODUCT_ID ||Project_DMO==g_HM0357_PRODUCT_ID)
	{
		g_HM0357_GPIO_PWD=HM0357_GPIO_PWD;
		strcpy (g_camera_pw_avdd,CAMERA_PW_AVDD_DMT);
		//g_camera_pw_avdd='rftx';//CAMERA_PW_AVDD_Ai1;
		printk( "Project DMT: hm0357 hwGpioPin_cam_if_sw:GPIO-17\r\n" );
		gpio_tlmm_config(GPIO_CFG( CAMIF_SW_DMT, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
		rc = gpio_request(CAMIF_SW_DMT, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(CAMIF_SW_DMT, 1);
		}
		gpio_free(CAMIF_SW_DMT);
		msleep(2);
		printk(KERN_INFO"[HM0357] gpio_get_value(17) = %d\n", gpio_get_value(CAMIF_SW_DMT));
   
	}
	else
	{
		g_HM0357_GPIO_PWD=HM0357_GPIO_PWD;
		strcpy (g_camera_pw_avdd,CAMERA_PW_AVDD);
		//g_camera_pw_avdd='wlan';//CAMERA_PW_AVDD;
	}
	printk(KERN_INFO "hm0357_sensor_probe HM0357_GPIO_PWD_AI1=0x%x,HM0357_GPIO_PWD=0x%x,g_HM0357_GPIO_PWD =0x%x,\n",HM0357_GPIO_PWD_AI1,HM0357_GPIO_PWD,g_HM0357_GPIO_PWD);	
	printk(KERN_INFO "[CAMERA_PS] power pin g_camera_pw_avdd=%s- VREG_CAM_UXGA_ACORE_V2P8 ON.\n",g_camera_pw_avdd);	
	
	vreg_camera = vreg_get(NULL, g_camera_pw_avdd);	
	if (IS_ERR(vreg_camera)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
		goto probe_done;
	}
	printk(KERN_ERR "%s: vreg g_camera_pw_avdd got!\n", __func__);

	
	rc = vreg_set_level(vreg_camera, 2800);
	if (rc) {
		printk(KERN_ERR "%s: vreg set level to 2.8V failed (%d)\n", __func__, rc);
		goto probe_done;
	}	
	rc = vreg_enable(vreg_camera);
	if (rc) {
		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc);
		goto probe_done;
	}



	rc = hm0357_reset(info);
	if (rc < 0) {
		printk("reset failed!\n");
		goto probe_done;
	}
	printk(KERN_ERR "%s: hm0357_reset done!\n", __func__);
	/* EVB CAMIF cannont handle in 24MHz */
	/* EVB use 12.8MHz */
	gpio_tlmm_config(GPIO_CFG(15, 1, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	msm_camio_clk_rate_set(24000000);
	msleep(25); 

	/* OV suggested Power up block End */
	/* Read the Model ID of the sensor */
	/* Read REG_HM0357_MODEL_ID_HI & REG_HM0357_MODEL_ID_LO */
	rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
		REG_HM0357_MODEL_ID_HI, &model_id, BYTE_LEN);
		
/* FIH, Henry Juang, 2010/11/01 { */
/* [FXX_CR], Add retry to prevent VGA blocked.*/
	i=0;	
	total=0;
	while(rc<0 && i<10) 
	{	
		gpio_tlmm_config(GPIO_CFG(g_HM0357_GPIO_PWD, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		rc = gpio_request(g_HM0357_GPIO_PWD, "hm0357");
		if (!rc) {
			rc = gpio_direction_output(g_HM0357_GPIO_PWD, 1);
		}
		msleep(10);
		rc = gpio_direction_output(g_HM0357_GPIO_PWD, 0); 
		gpio_free(g_HM0357_GPIO_PWD);		
		total+=10+i*25;
		printk(KERN_WARNING"hm0357_sensor_init_probe failed and retry after sleep %d ms. %d times!,total=%d ms.\n ",10+i*25,i+1,total);
		msleep(10+i*25);
		rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,	REG_HM0357_MODEL_ID_HI, &model_id, BYTE_LEN);
		i++;
	}
/* FIH, Henry Juang, 2010/11/01 } */

	if (rc < 0)
		goto probe_done;

	printk("hm0357 model_id = 0x%x\n", model_id);

	rc = hm0357_i2c_read(HM0357_I2C_READ_SLAVE_ID,
		REG_HM0357_MODEL_ID_LO, &model_id, BYTE_LEN);
	if (rc < 0)
		goto probe_done;

	printk("hm0357 model_id = 0x%x\n", model_id);


	s->s_init = hm0357_sensor_init;
	s->s_release = hm0357_sensor_release;
	s->s_config  = hm0357_sensor_config;
	printk("[HM0357 VGA] hm0357_sensor_probe successfully.\n");
probe_done:
	hm0357_power_off();
	printk("[HM0357 VGA]  gpio_get_value(40) = %d\n", gpio_get_value(40));
	dev_info(&hm0357_client->dev, "probe_done %s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;

}

static int __hm0357_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, hm0357_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __hm0357_probe,
	.driver = {
		.name = "msm_camera_hm0357",
		.owner = THIS_MODULE,
	},
};

static int __init hm0357_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(hm0357_init);
