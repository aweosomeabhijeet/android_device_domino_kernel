/*
 *     mt9p111.c - Camera Sensor Config
 *
 *     Copyright (C) 2010 Kent Kwan <kentkwan@fihspec.com>
 *     Copyright (C) 2008 FIH CO., Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <linux/gpio.h>
#include "mt9p111.h"

#include <mach/pmic.h>
//Div2-SW6-MM-SC-NewSensor-01+{
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
//Div2-SW6-MM-SC-NewSensor-01+}
/* [FXX_CR], flashlight function  */
#include <linux/completion.h>
#include <linux/hrtimer.h>

#include <linux/slab.h>
//Add this for AF power and Flash LED power.++
#include <mach/mpp.h>
#include <mach/sc668.h>
extern void brightness_onoff(int on);
extern void flash_settime(int time);
static int mt9p111_reset(const struct msm_camera_sensor_info *dev);
//Add this for AF power and Flash LED power.--

//Henry added for supporting standby mode.++
int mtp9111_sensor_standby(int onoff);
static int mt9p111_first_init=1;
static int is_regThreadEnd=0;
//Henry added for supporting standby mode.--

/*
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
*/


int mt9p111_m_ledmod=0;
//Div6-PT2-MM-CH-CameraTime-00+[
static int mt9p111_exposure_mode=-1;
static int mt9p111_antibanding_mode=-1;
static int mt9p111_brightness_mode=-1;
static int mt9p111_contrast_mode=-1;
static int mt9p111_effect_mode=-1;
static int mt9p111_saturation_mode=-1;
static int mt9p111_sharpness_mode=-1;
static int mt9p111_whitebalance_mode=-1;
static int mt9p111_snapshoted=0;
//static int mt9p111_iso_mode = -1;	//Div2-SW6-MM-HL-Camera-ISO-00+
static uint16_t R0x3012 = 0; //Div6-PT2-MM-CH-Camera_Flash-00+
static int wb_tmp = 0;
static int brightness_tmp = 0;
static int ab_tmp = 0;
static int contrast_tmp = 0;
static int saturation_tmp = 0;
static int sharpness_tmp = 0;
static int mt9p111_AFsetting = 0;

static int mt9p111_flashled_off_thread(void *arg);
static int mt9p111_reg_init_thread(void *arg);
//int mt9p111_reg_init_PATCH();
int ISmt9p111_init=0;

// Camera Interface Switch
int hwGpioPin_cam_if_sw = 255;
int device_PRODUCT_ID = 0;
int device_PHASE_ID = 0;

//Div6-PT2-MM-CH-CameraTime-00+]
static pid_t mt9p111_thread_id;
static pid_t mt9p111_reg_init_thread_id;
struct hrtimer mt9p111_flashled_timer;
#define mt9p111_FLASHLED_DELAY 200 //Div6-PT2-MM-CH-CameraSetting-01*
DECLARE_COMPLETION(mt9p111_flashled_comp);
DECLARE_COMPLETION(mt9p111_reginit_comp);

/* Micron mt9p111 Registers and their values */
/* Sensor Core Registers */
#define  REG_mt9p111_MODEL_ID 0x0000 //this should be 0x0000, Henry.
//#define  REG_mt9p111_MODEL_ID 0x3000
#define  mt9p111_MODEL_ID     0x2880

/*  SOC Registers Page 1  */
#define  REG_mt9p111_SENSOR_RESET     0x001A
#define  REG_mt9p111_STANDBY_CONTROL  0x0018
#define  REG_mt9p111_MCU_BOOT         0x001C

struct mt9p111_work {
	struct work_struct work;
};

static struct  mt9p111_work *mt9p111_sensorw;
static struct  i2c_client *mt9p111_client;

struct mt9p111_ctrl {
	const struct msm_camera_sensor_info *sensordata;
};


static struct mt9p111_ctrl *mt9p111_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(mt9p111_wait_queue);
DECLARE_MUTEX(mt9p111_sem);
DEFINE_MUTEX(mt9p111_mut);
//static int16_t mt9p111_effect = CAMERA_EFFECT_OFF;
//Div6-PT2-MM-CH-Camera_VFS-00+[
//#define MT9P111_USE_VFS
#ifdef MT9P111_USE_VFS
#define MT9P111_PREV_SNAP_REG "prev_snap_reg"
//Div6-PT2-MM-CH-Camera_VFS-01+[
#define MT9P111_PREV_SNAP2_REG "prev_snap2_reg"
#define MT9P111_PREV_SNAP3_REG "prev_snap3_reg"
#define MT9P111_PREV_SNAP4_REG "prev_snap4_reg"
#define MT9P111_PREV_SNAP5_REG "prev_snap5_reg"
#define MT9P111_PATCH2_REG "patch2_reg"
#define MT9P111_PATCH3_REG "patch3_reg"
//Div6-PT2-MM-CH-Camera_VFS-01+]
#define MT9P111_PATCH_REG "patch_reg"
#define MT9P111_PLL_REG "pll_reg"
#define MT9P111_AF_REG "af_reg" 
#define MT9P111_WRITEREG "writereg"
#define MT9P111_GETREG "getreg" 
#define MT9P111_FLASHTIME "flashtime"

#define MT9P111_VFS_PARAM_NUM 4 

/* MAX buf is ???? */
#define MT9P111_MAX_VFS_PREV_SNAP_INDEX 330
int mt9p111_use_vfs_prev_snap_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_prev_snap_settings_tbl[MT9P111_MAX_VFS_PREV_SNAP_INDEX];
uint16_t mt9p111_vfs_prev_snap_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_prev_snap_settings_tbl);

//Div6-PT2-MM-CH-Camera_VFS-01+[
#define MT9P111_MAX_VFS_PREV_SNAP2_INDEX 330
int mt9p111_use_vfs_prev_snap2_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_prev_snap2_settings_tbl[MT9P111_MAX_VFS_PREV_SNAP2_INDEX];
uint16_t mt9p111_vfs_prev_snap2_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_prev_snap2_settings_tbl);

#define MT9P111_MAX_VFS_PREV_SNAP3_INDEX 330
int mt9p111_use_vfs_prev_snap3_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_prev_snap3_settings_tbl[MT9P111_MAX_VFS_PREV_SNAP3_INDEX];
uint16_t mt9p111_vfs_prev_snap3_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_prev_snap3_settings_tbl);

#define MT9P111_MAX_VFS_PREV_SNAP4_INDEX 330
int mt9p111_use_vfs_prev_snap4_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_prev_snap4_settings_tbl[MT9P111_MAX_VFS_PREV_SNAP4_INDEX];
uint16_t mt9p111_vfs_prev_snap4_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_prev_snap4_settings_tbl);

#define MT9P111_MAX_VFS_PREV_SNAP5_INDEX 330
int mt9p111_use_vfs_prev_snap5_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_prev_snap5_settings_tbl[MT9P111_MAX_VFS_PREV_SNAP5_INDEX];
uint16_t mt9p111_vfs_prev_snap5_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_prev_snap5_settings_tbl);

#define MT9P111_MAX_VFS_PATCH2_INDEX 330
int mt9p111_use_vfs_patch2_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_patch2_settings_tbl[MT9P111_MAX_VFS_PATCH2_INDEX];
uint16_t mt9p111_vfs_patch2_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_patch2_settings_tbl);

#define MT9P111_MAX_VFS_PATCH3_INDEX 330
int mt9p111_use_vfs_patch3_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_patch3_settings_tbl[MT9P111_MAX_VFS_PATCH3_INDEX];
uint16_t mt9p111_vfs_patch3_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_patch3_settings_tbl);

//Div6-PT2-MM-CH-Camera_VFS-01+]

#define MT9P111_MAX_VFS_PATCH_INDEX 330
int mt9p111_use_vfs_patch_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_patch_settings_tbl[MT9P111_MAX_VFS_PATCH_INDEX];
uint16_t mt9p111_vfs_patch_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_patch_settings_tbl);

#define MT9P111_MAX_VFS_PLL_INDEX 330
int mt9p111_use_vfs_pll_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_pll_settings_tbl[MT9P111_MAX_VFS_PLL_INDEX];
uint16_t mt9p111_vfs_pll_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_pll_settings_tbl);

#define MT9P111_MAX_VFS_AF_INDEX 330
int mt9p111_use_vfs_af_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_af_settings_tbl[MT9P111_MAX_VFS_AF_INDEX];
uint16_t mt9p111_vfs_af_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_af_settings_tbl);
  
#define MT9P111_MAX_VFS_WRITEREG_INDEX 330
int mt9p111_use_vfs_writereg_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_writereg_settings_tbl[MT9P111_MAX_VFS_WRITEREG_INDEX];
uint16_t mt9p111_vfs_writereg_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_writereg_settings_tbl);

#define MT9P111_MAX_VFS_GETREG_INDEX 330
int mt9p111_use_vfs_getreg_setting=0;
struct mt9p111_i2c_reg_conf mt9p111_vfs_getreg_settings_tbl[MT9P111_MAX_VFS_GETREG_INDEX];
uint16_t mt9p111_vfs_getreg_settings_tbl_size= ARRAY_SIZE(mt9p111_vfs_getreg_settings_tbl);

int mt9p111_use_vfs_flashtime_setting=0;
 
#endif
//Div6-PT2-MM-CH-Camera_VFS-00+]

//add for maintaining easy.++
#define CAMERA_PW_DVDD "wlan"
#define CAMERA_PW_AVDD "gp6"
#define CAMERA_PW_AF "LDO4"
#define MT9P111_GPIO_PWD 31
#define MT9P111_GPIO_STANDBY 91
#define MT9P111_GPIO_STANDBY_AI1 31
#define MT9P111_GPIO_RESET 0 //can be left floating if not used.

int g_mt9p111_gpio_pwd=-1;
int g_mt9p111_gpio_standby=-1;
int g_mt9p111_gpio_reset=-1;

//add for maintaining easy.--

/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/
extern struct mt9p111_reg mt9p111_regs;
//static int32_t mt9p111_i2c_read(unsigned short   saddr, unsigned short raddr, unsigned short *rdata, enum mt9p111_width width);

/*=============================================================*/

static int32_t mt9p111_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	int i;//test
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};

	if (i2c_transfer(mt9p111_client->adapter, msg, 1) < 0) {
		printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_txdata failed, try again!\n");
		msleep(10);
		for(i=0;i<length;i++)
			printk(KERN_ERR "tx data[%d]=0x%02x\n",i,msg[0].buf[i]);
		printk(KERN_ERR "mt9p111_msg: delay 1s to retry i2c.\n");
		if (i2c_transfer(mt9p111_client->adapter, msg, 1) < 0) {
			printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_txdata failed twice, try again.\n");
			for(i=0;i<length;i++)
				printk(KERN_ERR "tx data[%d]=0x%02x\n",i,msg[0].buf[i]);
			msleep(20);
			printk(KERN_ERR "mt9p111_msg: delay 2s to retry i2c.\n");
			if (i2c_transfer(mt9p111_client->adapter, msg, 1) < 0) {
				printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_txdata failed.\n");
				for(i=0;i<length;i++)
					printk(KERN_ERR "tx data[%d]=0x%02x\n",i,msg[0].buf[i]);
				return -EIO;
			}
		}
	}

	return 0;
}

static int mt9p111_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	int i;
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

	if (i2c_transfer(mt9p111_client->adapter, msgs, 2) < 0) {
		printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_rxdata failed, try again!\n");
		msleep(10);
		for(i=0;i<msgs[0].len;i++)
			printk(KERN_ERR "register data[%d]=0x%02x\n",i,msgs[0].buf[i]);
		printk(KERN_ERR "mt9p111_msg: delay 1s to retry i2c.\n");
		if (i2c_transfer(mt9p111_client->adapter, msgs, 2) < 0) {
			printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_rxdata twice failed! try again.\n");
			for(i=0;i<msgs[0].len;i++)
				printk(KERN_ERR "register data[%d]=0x%02x\n",i,msgs[0].buf[i]);
			msleep(20);
			printk(KERN_ERR "mt9p111_msg: delay 2s to retry i2c.\n");
			if (i2c_transfer(mt9p111_client->adapter, msgs, 2) < 0) {
				for(i=0;i<msgs[0].len;i++)
					printk(KERN_ERR "register data[%d]=0x%02x\n",i,msgs[0].buf[i]);
				printk(KERN_ERR "mt9p111_msg: mt9p111_i2c_rxdata failed!\n");
				return -EIO;
			}
		}
	}

	return 0;
}

static int32_t mt9p111_i2c_read(unsigned short   saddr,
	unsigned short raddr, unsigned short *rdata, enum mt9p111_width width)
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

		rc = mt9p111_i2c_rxdata(saddr, buf, 2);
		if (rc < 0)
			return rc;

		*rdata = buf[0] << 8 | buf[1];
	}
		break;
		
	case BYTE_LEN: {
		buf[0] = (raddr & 0xFF00)>>8;
		buf[1] = (raddr & 0x00FF);

		//FIHLX-D6P2-MM-KK-FixI2CError-0++
		rc = mt9p111_i2c_rxdata(saddr, buf, 2);
		//rc = ov5642_i2c_rxdata(saddr, buf, 1);
		//FIHLX-D6P2-MM-KK-FixI2CError-0--
		if (rc < 0)
			return rc;

		*rdata = buf[0];
	}
		break;
		
	default:
		break;
	}

	if (rc < 0)
		CDBG("mt9p111_i2c_read failed!\n");

	return rc;
}


static int32_t mt9p111_i2c_write(unsigned short saddr,
	unsigned short waddr, unsigned short wdata, enum mt9p111_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[4];
	uint16_t bytePoll = 0; 
	uint16_t bitPoll = 0;
	uint8_t count = 0;

	//uint16_t readBack = 0;
	//printk("[Debug Info] mt9p111_i2c_write(), waddr = 0x%x, wdata = 0x%x.\n", waddr, wdata);

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF00)>>8;
		buf[3] = (wdata & 0x00FF);

		rc = mt9p111_i2c_txdata(saddr, buf, 4);
	}
		break;

	case BYTE_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata & 0xFF);

		rc = mt9p111_i2c_txdata(saddr, buf, 3);
	}
		break;

	case BYTE_POLL: {
		do {
			count++;
			msleep(10);//msleep(40);
			rc = mt9p111_i2c_read(saddr, waddr, &bytePoll, BYTE_LEN);
			//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, bytePoll);
		} while( (bytePoll != wdata) && (count <20) );
	}
		break;

	case BIT_POLL: {
		do {
			count++;
			msleep(10);//msleep(40);
			rc = mt9p111_i2c_read(saddr, waddr, &bitPoll, BYTE_LEN);
			//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, bitPoll);
		} while( !(bitPoll & wdata) && (count <20) );
	}
		break;
		
	/*
	case BYTE_LEN: {
		buf[0] = waddr;
		buf[1] = wdata;
		rc = mt9p111_i2c_txdata(saddr, buf, 2);
	}
		break;
	*/
	default:
		break;
	}

	//mdelay(1);
	//rc = mt9p111_i2c_read(saddr, waddr, &readBack, (enum mt9p111_width)width);
	//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, readBack);
	//mdelay(1);

	if (rc < 0)
		CDBG(
		"i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t mt9p111_i2c_write_table(
	struct mt9p111_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9p111_i2c_write(mt9p111_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata,
			reg_conf_tbl->width);
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			msleep(reg_conf_tbl->mdelay_time);

		reg_conf_tbl++;
	}

	return rc;
}

static int32_t mt9p111_i2c_write_burst_mode(unsigned short saddr,
	unsigned short waddr, unsigned short wdata1, unsigned short wdata2, unsigned short wdata3, unsigned short wdata4, 
	unsigned short wdata5, unsigned short wdata6, unsigned short wdata7, unsigned short wdata8, enum mt9p111_width width)
{
	int32_t rc = -EIO;
	unsigned char buf[18];

	//uint16_t readBack = 0;
	//printk("[Debug Info] mt9p111_i2c_write(), waddr = 0x%x, wdata = 0x%x.\n", waddr, wdata);

	memset(buf, 0, sizeof(buf));
	switch (width) {
	case WORD_LEN: {
		buf[0] = (waddr & 0xFF00)>>8;
		buf[1] = (waddr & 0x00FF);
		buf[2] = (wdata1 & 0xFF00)>>8;
		buf[3] = (wdata1 & 0x00FF);
		buf[4] = (wdata2 & 0xFF00)>>8;
		buf[5] = (wdata2 & 0x00FF);
		buf[6] = (wdata3 & 0xFF00)>>8;
		buf[7] = (wdata3 & 0x00FF);
		buf[8] = (wdata4 & 0xFF00)>>8;
		buf[9] = (wdata4 & 0x00FF);
		buf[10] = (wdata5 & 0xFF00)>>8;
		buf[11] = (wdata5 & 0x00FF);
		buf[12] = (wdata6 & 0xFF00)>>8;
		buf[13] = (wdata6 & 0x00FF);
		buf[14] = (wdata7 & 0xFF00)>>8;
		buf[15] = (wdata7 & 0x00FF);
		buf[16] = (wdata8 & 0xFF00)>>8;
		buf[17] = (wdata8 & 0x00FF);

		rc = mt9p111_i2c_txdata(saddr, buf, 18);
	}
		break;
		
	default:
		break;
	}

	//mdelay(1);
	//rc = mt9p111_i2c_read(saddr, waddr, &readBack, (enum mt9p111_width)width);
	//printk("[Debug Info] Read Back!!, raddr = 0x%x, rdata = 0x%x.\n", waddr, readBack);
	//mdelay(1);

	if (rc < 0)
		printk("Burst mode: i2c_write failed, addr = 0x%x!\n", waddr);

	return rc;
}

static int32_t mt9p111_i2c_write_table_burst_mode(
	struct mt9p111_i2c_reg_burst_mode_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EIO;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9p111_i2c_write_burst_mode(mt9p111_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata1, reg_conf_tbl->wdata2, reg_conf_tbl->wdata3, reg_conf_tbl->wdata4,
			reg_conf_tbl->wdata5, reg_conf_tbl->wdata6, reg_conf_tbl->wdata7, reg_conf_tbl->wdata8, reg_conf_tbl->width);
		if (rc < 0)
			break;
		if (reg_conf_tbl->mdelay_time != 0)
			msleep(reg_conf_tbl->mdelay_time);

		reg_conf_tbl++;
	}

	return rc;
}

#if 0
static long mt9p111_reg_init(void)
{
	//int32_t array_length;
	//int32_t i;
	long rc;

	/* Configure sensor for Preview mode and Snapshot mode Start */
	//Div6-PT2-MM-CH-Camera_VFS-01*[	
	//Div6-PT2-MM-CH-Camera_VFS-00+[
	#ifdef MT9P111_USE_VFS
	    if (mt9p111_use_vfs_prev_snap_setting)  
	    {
	        rc = mt9p111_i2c_write_table(&mt9p111_vfs_prev_snap_settings_tbl[0],	mt9p111_vfs_prev_snap_settings_tbl_size); 
			if (mt9p111_use_vfs_prev_snap2_setting)  
    	    {
	            rc = mt9p111_i2c_write_table(&mt9p111_vfs_prev_snap2_settings_tbl[0],	mt9p111_vfs_prev_snap2_settings_tbl_size); 
				if (mt9p111_use_vfs_prev_snap3_setting)  
        	    {
	                rc = mt9p111_i2c_write_table(&mt9p111_vfs_prev_snap3_settings_tbl[0],	mt9p111_vfs_prev_snap3_settings_tbl_size); 
					if (mt9p111_use_vfs_prev_snap4_setting)  
        	        {
	                    rc = mt9p111_i2c_write_table(&mt9p111_vfs_prev_snap4_settings_tbl[0],	mt9p111_vfs_prev_snap4_settings_tbl_size); 
						if (mt9p111_use_vfs_prev_snap5_setting)  
        	            {
	                        rc = mt9p111_i2c_write_table(&mt9p111_vfs_prev_snap5_settings_tbl[0],	mt9p111_vfs_prev_snap5_settings_tbl_size); 
        	            }
        	        }
        	    }
	        }
	    }
    	else
	#endif	
	//Div6-PT2-MM-CH-Camera_VFS-00+]
	//Div6-PT2-MM-CH-Camera_VFS-01*]	
	rc = mt9p111_i2c_write_table(&mt9p111_regs.prev_snap_reg_settings[0], mt9p111_regs.prev_snap_reg_settings_size);
	if (rc < 0)
		return rc;
	else
		printk("Finish Initial Setting.\n");
	/* Configure sensor for Preview mode and Snapshot mode End   */
	//msleep(5);

	complete(&mt9p111_reginit_comp);
#if 0
	
	/* Configure for Noise Reduction, Saturation and Aperture Correction Start */
	rc = mt9p111_i2c_write_table(&mt9p111_regs.noise_reduction_reg_settings[0], mt9p111_regs.noise_reduction_reg_settings_size);
	if (rc < 0)
		return rc;
	/* Configure for Noise Reduction, Saturation and Aperture Correction End   */
    msleep(5); //wait for the patch to complete initialization*/


	/* AF Setting */
	rc = mt9p111_i2c_write_table(&mt9p111_regs.aftbl[0], mt9p111_regs.aftbl_size);
	if (rc < 0)
		return rc;
	/* AF Setting  */
    msleep(5);
#endif

	return 0;
}
#endif


static ssize_t mt9p111_I2C_setting_attr(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned reg,value;
	char operation;
	unsigned short readdata=0;
	int rc=0;
	sscanf(buf, "%x %x %c\n", &reg, &value, &operation);

	printk(KERN_INFO "reg=0x%x, value=0x%x, operation=%c.\n", reg, value, operation);
	if(mt9p111_client==NULL)
	{
		printk("mt9p111_client is null.\n");
		return count;
	}
	
	if (operation=='w'){
		rc = mt9p111_i2c_write(mt9p111_client->addr, reg, value, WORD_LEN);

	}
	else if(operation=='r')
	{
		rc = mt9p111_i2c_read(mt9p111_client->addr,reg, &readdata, WORD_LEN);
		if (rc >=0)
		printk("read reg[0x%x]=0x%x.\n",reg,readdata);
	}
	else
	{
		printk("Operation failed.\n");
	}
	return count;
}
DEVICE_ATTR(mt9p111regset, 0666, NULL, mt9p111_I2C_setting_attr);


//Div6-PT2-MM-CH-CameraSetting-00*[
static long mt9p111_set_effect(int mode, int effect)
{
	long rc = 0;	
	printk("[cam_setting] effect = %d. mt9p111_effect_mode = %d.\n", effect, mt9p111_effect_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[
    if(mt9p111_effect_mode==effect)
        return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]

	mt9p111_effect_mode=effect; //Div6-PT2-MM-CH-CameraTime-00+

	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}

	switch (effect) 
	{
		case CAMERA_EFFECT_OFF: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.effect_none_tbl[0], mt9p111_regs.effect_none_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_EFFECT_MONO: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.effect_mono_tbl[0], mt9p111_regs.effect_mono_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_EFFECT_SEPIA: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.effect_sepia_tbl[0], mt9p111_regs.effect_sepia_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_EFFECT_NEGATIVE: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.effect_negative_tbl[0], mt9p111_regs.effect_negative_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_EFFECT_SOLARIZE: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.effect_solarize_tbl[0], mt9p111_regs.effect_solarize_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_brightness(int mode, int brightness)
{
	long rc = 0;	
	printk("[cam_setting] brightness = %d. mt9p111_brightness_mode = %d.\n", brightness, mt9p111_brightness_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[	
    if(mt9p111_brightness_mode==brightness)
   	 return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]	

	mt9p111_brightness_mode=brightness; //Div6-PT2-MM-CH-CameraTime-00+
	
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}
		
    switch (brightness) 
	{
		case CAMERA_BRIGHTNESS_0: 	//[EV -3]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br0_tbl[0], mt9p111_regs.br0_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;
		
		case CAMERA_BRIGHTNESS_1: 	//[EV -2]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br1_tbl[0], mt9p111_regs.br1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_BRIGHTNESS_2: 	//[EV -1]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br2_tbl[0], mt9p111_regs.br2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_BRIGHTNESS_3: 	//[EV 0 (default)]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br3_tbl[0], mt9p111_regs.br3_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_BRIGHTNESS_4: 	//[EV +1]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br4_tbl[0], mt9p111_regs.br4_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		case CAMERA_BRIGHTNESS_5: 	//[EV +2]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br5_tbl[0], mt9p111_regs.br5_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		case CAMERA_BRIGHTNESS_6: 	//[EV +3]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.br6_tbl[0], mt9p111_regs.br6_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_contrast(int mode, int contrast)
{
	long rc = 0;	
	printk("[cam_setting] contrast = %d. mt9p111_contrast_mode = %d.\n", contrast, mt9p111_contrast_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[	
    if(mt9p111_contrast_mode==contrast)
        return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]

	mt9p111_contrast_mode=contrast; //Div6-PT2-MM-CH-CameraTime-00+
	
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}
		
    switch (contrast) 
	{
		case CAMERA_CONTRAST_MINUS_2: 	//[Const -2]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.const_m2_tbl[0], mt9p111_regs.const_m2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_CONTRAST_MINUS_1: 	//[Const -1]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.const_m1_tbl[0], mt9p111_regs.const_m1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_CONTRAST_ZERO: 	//[Const 0 (default)]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.const_zero_tbl[0], mt9p111_regs.const_zero_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_CONTRAST_POSITIVE_1: 	//[Const +1]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.const_p1_tbl[0], mt9p111_regs.const_p1_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		case CAMERA_CONTRAST_POSITIVE_2: 	//[Const +2]
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.const_p2_tbl[0], mt9p111_regs.const_p2_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		default: 
			return -EINVAL;
	}
		
	return rc;
}

static long mt9p111_set_exposure_mode(int mode, int exposuremode)
{
	long rc = 0;
	printk("[cam_setting] exposuremode = %d. mt9p111_exposure_mode = %d.\n", exposuremode, mt9p111_exposure_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[
    if(mt9p111_exposure_mode==exposuremode)
        return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]

	mt9p111_exposure_mode=exposuremode; //Div6-PT2-MM-CH-CameraTime-00+ 
	
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}
	
    switch (exposuremode) 
	{
		case CAMERA_AEC_FRAME_AVERAGE:	//EXPOSURE_AVERAGE
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.exp_average_tbl[0], mt9p111_regs.exp_average_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;
	
		case CAMERA_AEC_CENTER_WEIGHTED:	//EXPOSURE_NORMAL
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.exp_normal_tbl[0], mt9p111_regs.exp_normal_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_AEC_SPOT_METERING:	//EXPOSURE_SPOT
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.exp_spot_tbl[0], mt9p111_regs.exp_spot_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_saturation(int mode, int saturation)
{
	long rc = 0;		
	printk("[cam_setting] saturation = %d. mt9p111_saturation_mode = %d.\n", saturation, mt9p111_saturation_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[
    if(mt9p111_saturation_mode==saturation)
        return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]

	mt9p111_saturation_mode=saturation; //Div6-PT2-MM-CH-CameraTime-00+
	
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}
	
    switch (saturation) 
	{
		case CAMERA_SATURATION_MINUS_2:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.satu_m2_tbl[0], mt9p111_regs.satu_m2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SATURATION_MINUS_1:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.satu_m1_tbl[0], mt9p111_regs.satu_m1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SATURATION_ZERO:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.satu_zero_tbl[0], mt9p111_regs.satu_zero_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SATURATION_POSITIVE_1:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.satu_p1_tbl[0], mt9p111_regs.satu_p1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SATURATION_POSITIVE_2:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.satu_p2_tbl[0], mt9p111_regs.satu_p2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;		

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_wb(int mode, int wb)
{
	long rc = 0;
	printk("[cam_setting] wb = %d. mt9p111_whitebalance_mode = %d.\n", wb, mt9p111_whitebalance_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[	
    if(mt9p111_whitebalance_mode==wb)
        return 0;
	//Div6-PT2-MM-CH-CameraTime-00+]	

	mt9p111_whitebalance_mode=wb; //Div6-PT2-MM-CH-CameraTime-00+

	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}

    switch (wb) 
	{
		case CAMERA_WB_AUTO: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.wb_auto_tbl[0], mt9p111_regs.wb_auto_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_WB_DAYLIGHT: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.wb_daylight_tbl[0], mt9p111_regs.wb_daylight_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_WB_CLOUDY_DAYLIGHT: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.wb_cloudy_daylight_tbl[0], mt9p111_regs.wb_cloudy_daylight_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_WB_INCANDESCENT: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.wb_incandescent_tbl[0], mt9p111_regs.wb_incandescent_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_WB_FLUORESCENT: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.wb_fluorescent_tbl[0], mt9p111_regs.wb_fluorescent_tbl_size);
			if (rc < 0)
				return rc;
		} 
		break;

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_antibanding(int mode, int antibanding)
{
	long rc = 0;
	printk("[cam_setting] antibanding = %d. mt9p111_antibanding_mode = %d.\n", antibanding, mt9p111_antibanding_mode);
	
	//Div6-PT2-MM-CH-CameraTime-00+[
    if(mt9p111_antibanding_mode==antibanding)
        return 0;
    //Div6-PT2-MM-CH-CameraTime-00+]

	mt9p111_antibanding_mode=antibanding; //Div6-PT2-MM-CH-CameraTime-00+

	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}

	switch (antibanding) 
	{
		case CAMERA_ANTIBANDING_OFF: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.ab_off_tbl[0], mt9p111_regs.ab_off_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_ANTIBANDING_60HZ: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.ab_60mhz_tbl[0], mt9p111_regs.ab_60mhz_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_ANTIBANDING_50HZ: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.ab_50mhz_tbl[0], mt9p111_regs.ab_50mhz_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		case CAMERA_ANTIBANDING_AUTO: 
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.ab_auto_tbl[0], mt9p111_regs.ab_auto_tbl_size);
			if (rc < 0)
				return rc;		
		}
		break;

		default: 
			return -EINVAL;
	}
	
	return rc;
}

static long mt9p111_set_sharpness(int mode, int sharpness)
{
	long rc = 0;
	printk("[cam_setting] sharpness = %d. mt9p111_sharpness_mode = %d.\n", sharpness, mt9p111_sharpness_mode);

    if(mt9p111_sharpness_mode==sharpness)
        return 0;

	mt9p111_sharpness_mode=sharpness;
	
	switch (mode) 
	{
		case SENSOR_PREVIEW_MODE:
			/* Context A Special Effects */
			break;

		case SENSOR_SNAPSHOT_MODE:
			/* Context B Special Effects */
			break;

		default:
			break;
	}
	
    switch (sharpness) 
	{
		/*
		case CAMERA_SHARPNESS_MINUS2:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.sharp_m2_tbl[0], mt9p111_regs.sharp_m2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SHARPNESS_MINUS1:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.sharp_m1_tbl[0], mt9p111_regs.sharp_m1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;
        */
		case CAMERA_SHARPNESS_ZERO:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.sharp_zero_tbl[0], mt9p111_regs.sharp_zero_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SHARPNESS_POSITIVE_1:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.sharp_p1_tbl[0], mt9p111_regs.sharp_p1_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;

		case CAMERA_SHARPNESS_POSITIVE_2:
		{
			rc = mt9p111_i2c_write_table(&mt9p111_regs.sharp_p2_tbl[0], mt9p111_regs.sharp_p2_tbl_size);
			if (rc < 0)
				return rc;
		}
		break;		

		default: 
			return -EINVAL;
	}
	
	return rc;
}

//Div6-PT2-MM-CH-CameraSetting-00*]
static long mt9p111_set_ledmod(int mode, int ledmod)
{
	long rc = 0;	
	
	CDBG("mt9p111_set_ledmod, mode = %d, ledmod = %d\n",
		mode, ledmod);
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

	switch (ledmod) {
	case LED_MODE_OFF: {
		mt9p111_m_ledmod=0;
	}
			break;

	case LED_MODE_AUTO: {
		mt9p111_m_ledmod=1;
	}
		break;

	case LED_MODE_ON: {
		mt9p111_m_ledmod=2;
	}
		break;

	default: {
		if (rc < 0)
			return rc;

		mt9p111_m_ledmod=0;
		return -EFAULT;
	}
	}

	return rc;
}
//Div6-PT2-MM-CH-AF-00+[
static long mt9p111_set_autofocus(int mode, int autofocus)
{
	long rc = 0;
	uint16_t f_state = 0; //Div6-PT2-MM-CH-AF-01+
	uint16_t f_state2 = 0; //Div6-PT2-MM-CH-AF-02+
	uint16_t R0x3012_AF = 0;
	uint8_t count = 0;
	
	printk("[cam_setting] mt9p111_set_autofocus...\n");
	
	if (mt9p111_AFsetting == 0)
	{
	    	/* AF Setting */
	        //Div6-PT2-MM-CH-Camera_VFS-00+[
		#ifdef MT9P111_USE_VFS
	        if (mt9p111_use_vfs_af_setting)	
			rc = mt9p111_i2c_write_table(&mt9p111_vfs_af_settings_tbl[0],	mt9p111_vfs_af_settings_tbl_size);	
	        else
		#endif	
	        //Div6-PT2-MM-CH-Camera_VFS-00+]		
		        rc = mt9p111_i2c_write_table(&mt9p111_regs.aftbl[0], mt9p111_regs.aftbl_size);
	    	if (rc < 0)
		    	return rc;
		else
			printk("Finish AF Setting.\n");

		mt9p111_AFsetting = 1;
	    	/* AF Setting  */	
	}
	
	rc = mt9p111_i2c_read(mt9p111_client->addr, 0x3012, &R0x3012_AF, WORD_LEN);
	if (rc < 0)
		return rc;

	if (R0x3012_AF > 0x0A00) {
		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x3002, WORD_LEN);
		if (rc < 0)
			return rc;	  
		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0xB002, 0x0301, WORD_LEN);
		if (rc < 0)
			return rc;

	//AF_postition_settings
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB018, 0x00, BYTE_LEN);	// AF_FS_POS_0
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB019, 0x20, BYTE_LEN);	// AF_FS_POS_1
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01A, 0x38, BYTE_LEN);	// AF_FS_POS_2
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01B, 0x50, BYTE_LEN);	// AF_FS_POS_3
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01C, 0x68, BYTE_LEN);	// AF_FS_POS_4
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01D, 0x80, BYTE_LEN);	// AF_FS_POS_5
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01E, 0x98, BYTE_LEN);	// AF_FS_POS_6
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01F, 0xB0, BYTE_LEN);	// AF_FS_POS_7
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB020, 0xC0, BYTE_LEN);	// AF_FS_POS_8
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB012, 0x09, BYTE_LEN);	// AF_FS_NUM_STEPS
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB013, 0x55, BYTE_LEN);	// AF_FS_NUM_STEPS2
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB014, 0x06, BYTE_LEN);	// AF_FS_STEP_SIZE
		
	} else {
		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x3002, WORD_LEN);
		if (rc < 0)
			return rc;	  
		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0xB002, 0x0305, WORD_LEN);//0x030D
		if (rc < 0)
			return rc;

	//AF_postition_settings
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB018, 0x00, BYTE_LEN);	// AF_FS_POS_0
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB019, 0x14, BYTE_LEN);	// AF_FS_POS_1
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01A, 0x20, BYTE_LEN);	// AF_FS_POS_2
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01B, 0x2C, BYTE_LEN);	// AF_FS_POS_3
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01C, 0x38, BYTE_LEN);	// AF_FS_POS_4
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01D, 0x44, BYTE_LEN);	// AF_FS_POS_5
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01E, 0x50, BYTE_LEN);	// AF_FS_POS_6
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB01F, 0x5C, BYTE_LEN);	// AF_FS_POS_7
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB020, 0x68, BYTE_LEN);	// AF_FS_POS_8
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB021, 0x74, BYTE_LEN);	// AF_FS_POS_9
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB022, 0x80, BYTE_LEN);	// AF_FS_POS_10
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB023, 0x8C, BYTE_LEN);	// AF_FS_POS_11
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB024, 0x98, BYTE_LEN);	// AF_FS_POS_12
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB025, 0xA4, BYTE_LEN);	// AF_FS_POS_13
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB026, 0xB0, BYTE_LEN);	// AF_FS_POS_14
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB027, 0xBC, BYTE_LEN);	// AF_FS_POS_15
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB012, 0x0F, BYTE_LEN);	// AF_FS_NUM_STEPS
		(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB014, 0x0B, BYTE_LEN);	// AF_FS_STEP_SIZE
		
	}
        //Div6-PT2-MM-CH-AF-02*[   
	rc =	mt9p111_i2c_write(mt9p111_client->addr, 0xB006, 0x01, BYTE_LEN);
	if (rc < 0)
		return rc;
	
	//Div6-PT2-MM-CH-AF-01+[
	do 
	{
	        msleep(50);
	    	rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x3000, WORD_LEN);
		if (rc < 0)
			return rc;            
	            
	        rc = mt9p111_i2c_read(mt9p111_client->addr, 0xB000, &f_state, WORD_LEN);
	        if (rc < 0)
	        	return rc;

		if (f_state & 0x0400)  // AF algorithm cannot get sharpness info
			count++;
	    
	        CDBG("mt9p111_set_autofocus_state = 0x%x\n", f_state);
	} while(((f_state & 0x0010)==0) && (count <=30));
	//Div6-PT2-MM-CH-AF-01+]

	rc = mt9p111_i2c_read(mt9p111_client->addr, 0xB000, &f_state2, WORD_LEN);
	if (rc < 0)
		 return rc;	
//Henry added this for returning AF result.++	
	 printk("out of loop mt9p111_set_autofocus_state = 0x%x\n", f_state);
#if 1 // We will turn on this feature when APP needs this failed information!
	if((f_state2&0x8000) || (f_state2 == 0x0400)) {	
		printk("AF Error!!! mt9p111_set_autofocus_state2 = 0x%x\n", f_state2);
		rc = -1;
	}
#endif	
//Henry added this for returning AF result.--
        //Div6-PT2-MM-CH-AF-02*]

	(void)mt9p111_i2c_write(mt9p111_client->addr, 0xB000, 0x0000, WORD_LEN);  // Clear state
		
	return rc;
}
//Div6-PT2-MM-CH-AF-00+]
static long mt9p111_set_sensor_mode(int mode)
{
	//uint16_t clock;
	long rc = 0;
	int count = 0;
	uint16_t seq_state = 0;
	int retryloop=0;

	retryloop=0;
	while(is_regThreadEnd==0 && retryloop++<50)
	{
		printk(KERN_ERR "**is_regThreadEnd = %d.\n", is_regThreadEnd);
		msleep(25);
	}
		
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		printk(KERN_ERR "mt9p111_msg: case SENSOR_PREVIEW_MODE. mt9p111_snapshoted = %d.\n", mt9p111_snapshoted);
	        //Div6-PT2-MM-CH-CameraTime-00+[
	        if(mt9p111_snapshoted==0)
	            return 0;		
		mt9p111_snapshoted = 0;
	        //Div6-PT2-MM-CH-CameraTime-00+]

		if ((mt9p111_m_ledmod==2) || ((mt9p111_m_ledmod == 1) && (R0x3012 >= 0x9C4)))  //Div6-PT2-MM-CH-Camera_Flash-00*]
		{
			if (R0x3012 >= 0x9C4) {
				printk("[kent] rollback, R0x3012 = 0x%x.\n", R0x3012);
				//Change AWB & Effect setting back if LED is lit
				mt9p111_set_wb(0, wb_tmp);
				mt9p111_set_brightness(0, brightness_tmp);  // For brightness setting
			}
		}
		
		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x098E, 0x843C, WORD_LEN);
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x843C, 0x01, BYTE_LEN);
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x8404, 0x01, BYTE_LEN);
		if (rc < 0)
			return rc;

		do {
			msleep(50);   //msleep(100);
				
			rc = mt9p111_i2c_read(mt9p111_client->addr,	0x8405, &seq_state, BYTE_LEN);
			if (rc < 0)
				return rc;

			printk("mt9p111 Preview seq_state = 0x%x\n", seq_state);
		} while(seq_state != 0x03);

		printk(KERN_ERR "mt9p111_msg: case SENSOR_PREVIEW_MODE end~~~~~~.\n");
		
		break;

	case SENSOR_SNAPSHOT_MODE:
        	mt9p111_snapshoted=1; //Div6-PT2-MM-CH-CameraTime-00+
		printk(KERN_ERR "mt9p111_msg: case SENSOR_SNAPSHOT_MODE.\n");
		
		//Div6-PT2-MM-CH-Camera_Flash-00*[
		rc = mt9p111_i2c_read(mt9p111_client->addr,	0x3012, &R0x3012, WORD_LEN);
		if (rc < 0)
			return rc;			
		printk("mt9p111_msg: snapshot mt9p111 mt9p111_m_ledmod = %d, R0x3012 = 0x%x.\n", mt9p111_m_ledmod, R0x3012);		
		if ((mt9p111_m_ledmod==2) || ((mt9p111_m_ledmod == 1) && (R0x3012 >= 0x9C4)))  //Div6-PT2-MM-CH-Camera_Flash-00*]
		{
			if (R0x3012 >= 0x9C4)
			{
				//Change AWB & Effect setting first when turn on LED
				wb_tmp = mt9p111_whitebalance_mode;
				mt9p111_whitebalance_mode = -1;
				mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0xACB0, WORD_LEN);
				mt9p111_i2c_write(mt9p111_client->addr, 0xACB0, 0x38, BYTE_LEN);	// AWB_RG_MIN
				mt9p111_i2c_write(mt9p111_client->addr, 0xACB1, 0x4C, BYTE_LEN);	// AWB_RG_MAX
				mt9p111_i2c_write(mt9p111_client->addr, 0xACB4, 0x38, BYTE_LEN);	// AWB_BG_MIN
				mt9p111_i2c_write(mt9p111_client->addr, 0xACB5, 0x4C, BYTE_LEN);	// AWB_BG_MAX
				mt9p111_i2c_write(mt9p111_client->addr, 0xAC04, 0x3E, BYTE_LEN); // AWB_PRE_AWB_R2G_RATIO
				mt9p111_i2c_write(mt9p111_client->addr, 0xAC05, 0x48, BYTE_LEN); // AWB_PRE_AWB_B2G_RATIO
				brightness_tmp =mt9p111_brightness_mode;
				mt9p111_brightness_mode = -1;
				mt9p111_i2c_write(mt9p111_client->addr, 0xA409, 0x14, BYTE_LEN); // For brightness setting
			}

			//flash_settime(16);//pmic_flash_led_set_current(50); 
			//brightness_onoff(1);
			brightness_onoff(1);
			msleep(200);
		}	

		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x098E, 0x843C, WORD_LEN);
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x843C, 0x07, BYTE_LEN); // 0xFF
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x8404, 0x02, BYTE_LEN);
		if (rc < 0)
			return rc;

		do {
			msleep(50);  //msleep(100);
			count++;
			rc = mt9p111_i2c_read(mt9p111_client->addr,	0x8405, &seq_state, BYTE_LEN);
			if (rc < 0)
				return rc;

			printk("mt9p111 Snapshot seq_state = 0x%x\n", seq_state);

			//[DMP.B-24] jones+++
			if( count % 2 == 0 )
			{
				printk("mt9p111 issue snapshot command again\n");	
				rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x098E, 0x843C, WORD_LEN);
				if (rc < 0)
					return rc;
		
				rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x843C, 0x07, BYTE_LEN); // 0xFF
				if (rc < 0)
					return rc;
		
				rc =	mt9p111_i2c_write(mt9p111_client->addr,	0x8404, 0x02, BYTE_LEN);
				if (rc < 0)
					return rc;
			}
			if( count > 50 )
			{
				count = 0;
				break;
			}
                        //[DMP.B-24] jones---
		} while(seq_state != 0x07);		

		if ((mt9p111_m_ledmod==2) || ((mt9p111_m_ledmod == 1) && (R0x3012 >= 0x9C4)))  //Div6-PT2-MM-CH-Camera_Flash-00*]
		{	
			hrtimer_cancel(&mt9p111_flashled_timer);
			//Div6-PT2-MM-CH-Camera_VFS-00+[
			#ifdef MT9P111_USE_VFS
				if (mt9p111_use_vfs_flashtime_setting!=0)
					hrtimer_start(&mt9p111_flashled_timer,
						ktime_set(mt9p111_use_vfs_flashtime_setting / 1000, (mt9p111_use_vfs_flashtime_setting % 1000) * 1000000),
						HRTIMER_MODE_REL);
				else
			#endif
			//Div6-PT2-MM-CH-Camera_VFS-00+]
				//flash_settime(16);
				hrtimer_start(&mt9p111_flashled_timer,
					ktime_set(mt9p111_FLASHLED_DELAY / 1000, (mt9p111_FLASHLED_DELAY % 1000) * 1000000),
					HRTIMER_MODE_REL);
				//brightness_onoff(0);
				if (hrtimer_active(&mt9p111_flashled_timer))
					printk(KERN_INFO "%s: TIMER running\n", __func__);
		}
		printk(KERN_ERR "mt9p111_msg: case SENSOR_SNAPSHOT_MODE end~~~~~~~~~~.\n");
		
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		mt9p111_snapshoted=1; //Div6-PT2-MM-CH-CameraTime-00+
		printk(KERN_ERR "mt9p111_msg: case SENSOR_RAW_SNAPSHOT_MODE.\n");

		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x843C, WORD_LEN);
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x843C, 0xFF, BYTE_LEN);
		if (rc < 0)
			return rc;

		rc =	mt9p111_i2c_write(mt9p111_client->addr, 0x8404, 0x02, BYTE_LEN);
		if (rc < 0)
			return rc;

		do {
			msleep(50);  //msleep(100);
				
			rc = mt9p111_i2c_read(mt9p111_client->addr, 0x8405, &seq_state, BYTE_LEN);
			if (rc < 0)
				return rc;
		
			printk("mt9p111 Raw Snapshot seq_state = 0x%x\n", seq_state);
		} while(seq_state != 0x07);

		printk(KERN_ERR "mt9p111_msg: case SENSOR_RAW_SNAPSHOT_MODE end~~~~~~~~~~.\n");

		break;

	default:
		return -EINVAL;
	}

	return 0;
}




int mt9p111_power_on(void)
{
	struct vreg *vreg_camera;
	int rc2 = 0xF;
	int rc,retry_sc668;

    printk( "mt9p111_power_on\r\n" );
    if( device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S) // For Ai1 ////////////////////
    {			
        printk( "mt9p111 Project AI1D\r\n" );                
        
        // i should add error handling, but not now.
        //DCORE/DVDD 1.8V, for Ai1 is gp2
        printk( "mt9p111 Enable DCORE:gp2:1.8v\r\n" );
        vreg_camera = vreg_get(NULL, "gp2");		
		vreg_set_level(vreg_camera, 1800);		
		vreg_enable(vreg_camera);        
        msleep( 2 );
               
        //VCM AF power 
        printk( "mt9p111 Enable VCM via GPIO93/CAM_LDO_EN_3V\r\n" );
        gpio_tlmm_config(GPIO_CFG( 93, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
        rc = gpio_request(93, "mt9p111");
        if (!rc) {
        	rc = gpio_direction_output(93, 1);
        }
        gpio_free(93);
        msleep( 2 );
        
        //ACORE/AVDD 2.8V
        printk( "mt9p111 Enable ACORE:rftx:2.8v\r\n" );
        vreg_camera = vreg_get(NULL, "rftx");		
		vreg_set_level(vreg_camera, 2800);		
		vreg_enable(vreg_camera);        
        msleep( 2 );
        // Thanks god, the VDDIO is from msmp, the msmp is always on.                
    }
    else        
    {// For DMP/DPD //////////////////////////////////////////////////

    	vreg_camera = vreg_get(NULL, CAMERA_PW_DVDD);
    	printk(KERN_INFO "[CAMERA_PS] power pin - VREG_MSME_V1P8 ON.\n");
    	if (IS_ERR(vreg_camera)) {
    		printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
    		return PTR_ERR(vreg_camera);
    	}
    	rc2 = vreg_set_level(vreg_camera, 1800);
    	if (rc2) {
    		printk(KERN_ERR "%s: vreg set level to 1.8V failed (%d)\n", __func__, rc2);
    		return -EIO;
    	}
    	rc2 = vreg_enable(vreg_camera);
    	if (rc2) {
    		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc2);
    		return -EIO;
    	}
    
    	retry_sc668=0;
    	while(!sc668_get_chip_state( ) && retry_sc668++<10){
    		printk(KERN_ERR"wait sc688 enable");
    		msleep(2);
    	}
    	
    	if(retry_sc668==10)
    		printk(KERN_ERR"sc688 sucks, please contact with sc668 driver owner!!!");
    		
    		
    	rc = sc668_LDO_setting(4, LDO_VOLTAGE_2_8, "mt9p111");
    	if (rc) {
    		printk(KERN_ERR"%s: LED CATHODE voltage(4) setting failed (%d)\n",  __func__, rc);
    		return -EIO;
    	}
	
//test DVDD++
//	msleep(2);
//	rc2 = vreg_disable(vreg_camera);
//	if (rc2) {
//		printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc2);
//		return -EIO;
//	}
//test DVDD--
#if 0 //The VREG_CAM_VDD_IO_V2P6 is always power on.
	//mdelay(10); 

	vreg_camera = vreg_get(NULL, "rftx");
 	printk(KERN_INFO "[CAMERA_PS] power pin - VREG_CAM_VDD_IO_V2P6 ON.\n");
 	if (IS_ERR(vreg_camera)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
		return PTR_ERR(vreg_camera);
	}
	rc2 = vreg_set_level(vreg_camera, 2600);
	if (rc2) {
		printk(KERN_ERR "%s: vreg set level to 2.6V failed (%d)\n", __func__, rc2);
		return -EIO;
	}
	rc2 = vreg_enable(vreg_camera);
	if (rc2) {
		printk(KERN_ERR "%s: vreg enable failed (%d)\n", __func__, rc2);
    		return -EIO;
	}
	
	//mdelay(10);
#endif	
	
	    vreg_camera = vreg_get(NULL, CAMERA_PW_AVDD);
	    printk(KERN_INFO "[CAMERA_PS] power pin CAMERA_PW_AVDD=%s- VREG_CAM_UXGA_ACORE_V2P8 ON.\n",CAMERA_PW_AVDD);
	    if (IS_ERR(vreg_camera)) {
	    	printk(KERN_ERR "%s: vreg get failed (%ld)\n", __func__, PTR_ERR(vreg_camera));
	    	return PTR_ERR(vreg_camera);
	    }
	    printk(KERN_ERR "%s: vreg CAMERA_PW_AVDD got!\n", __func__);
        
	    
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
	}//End of For DMP/DPD ////////////////////////////////////////////
	
	return rc2;
}

int mt9p111_power_off(void)
{
	struct vreg *vreg_camera;
	int rc2,rc,retry_sc668;

//	printk(KERN_INFO "[CAMERA_PS] power off but do nothing..\n");
//	return 0;
    printk( "mt9p111_power_off\r\n" );
// Add this to avoid Ai1 crash.Henry++
	if( !(device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S))
	{
		rc = gpio_request(g_mt9p111_gpio_pwd, "mt9p111");
		if (!rc) {
			rc = gpio_direction_output(g_mt9p111_gpio_pwd, 1);
		}
		gpio_free(g_mt9p111_gpio_pwd);
	}
	else
	{
		printk( "mt9p111: This is Ai1D/Ai1S PR1. There is no PWD pin.\n" );                        
	}
// Add this to avoid Ai1 crash.Henry--

	rc = gpio_request(g_mt9p111_gpio_reset, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 0);
	}
	gpio_free(g_mt9p111_gpio_reset);


	rc = gpio_request(g_mt9p111_gpio_standby, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_standby, 0);
	}
	gpio_free(g_mt9p111_gpio_standby);


    if( device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S) // For Ai1 ////////////////////
    {			
        printk( "mt9p111 Project AI1D\r\n" );                        
        // i should add error handling, but not now.
        //DCORE/DVDD 1.8V, for Ai1 is gp2
        printk( "mt9p111 disable DCORE:gp2:1.8v\r\n" );
        vreg_camera = vreg_get(NULL, "gp2");
		vreg_disable(vreg_camera);
        msleep( 2 );
               
        //VCM AF power 
        printk( "mt9p111 disable VCM via GPIO93/CAM_LDO_EN_3V\r\n" );
        gpio_tlmm_config(GPIO_CFG( 93, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
        rc = gpio_request(93, "mt9p111");
        if (!rc) {
        	rc = gpio_direction_output(93, 0);
        }
        gpio_free(93);
        msleep( 2 );
        
        //ACORE/AVDD 2.8V
        printk( "mt9p111 disable ACORE:rftx:2.8v\r\n" );
        vreg_camera = vreg_get(NULL, "rftx");				
		vreg_disable(vreg_camera);        
        msleep( 2 );
        // Thanks god, the VDDIO is from msmp, the msmp is always on.                
    }
    else // For DMP/DPD //////////////////////////////////////////////
    {

    	(void)pmic_vreg_pull_down_switch(ON_CMD, PM_VREG_PDOWN_WLAN_ID);
    	(void)pmic_vreg_pull_down_switch(ON_CMD, PM_VREG_PDOWN_GP6_ID);	
    
    	vreg_camera = vreg_get(NULL, CAMERA_PW_DVDD);
    	printk(KERN_INFO "[CAMERA_PS] power pin - VREG_MSME_V1P8 OFF.\n");
    	rc2 = vreg_disable(vreg_camera);
    	if (rc2) {
    		printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc2);
    		return -EIO;
    	}
    	retry_sc668=0;
    	while(!sc668_get_chip_state( ) && retry_sc668++<10){
    		printk(KERN_ERR"wait sc688 enable");
    		msleep(2);
    	}
    	
    	if(retry_sc668==10)
    		printk(KERN_ERR"sc688 sucks, please contact with sc668 driver owner!!!");
    		
    	rc = sc668_LDO_setting(4, LDO_OFF, "mt9p111");
    	if (rc) {
    		printk(KERN_ERR"%s: LED CATHODE voltage(4) setting failed (%d)\n",  __func__, rc);
    		return -EIO;
    	}
    	//mdelay(5); // t2
#if 0 //The VREG_CAM_VDD_IO_V2P6 is always power on.
	vreg_camera = vreg_get(NULL, "rftx");
	printk(KERN_INFO "[CAMERA_PS] power pin - VREG_CAM_VDD_IO_V2P6 OFF.\n");
	rc2 = vreg_disable(vreg_camera);
	if (rc2) {
		printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc2);
		return -EIO;
	}	
#endif
	    //mdelay(5); 
        
	    vreg_camera = vreg_get(NULL, CAMERA_PW_AVDD);
	    printk(KERN_INFO "[CAMERA_PS] power pin - VREG_CAM_UXGA_ACORE_V2P8 OFF.\n");
	    rc2 = vreg_disable(vreg_camera);
	    if (rc2) {
	    	printk(KERN_ERR "%s: vreg disable failed (%d)\n", __func__, rc2);
	    	return -EIO;
	    }    
    }// End of For DMP/DPD //////////////////////////////////////////////     

    printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_reset, gpio_get_value(g_mt9p111_gpio_reset));
    printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_standby, gpio_get_value(g_mt9p111_gpio_standby));
    if( !(device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S))
    	printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_pwd, gpio_get_value(g_mt9p111_gpio_pwd));
	   
	return rc2;
}

static int mt9p111_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	uint16_t model_id = 0;
	//uint16_t check = 0;
	int rc = 0;

	printk("mt9p111_sensor_init_probe entry.\n");

	/* PLL Setup Start */
    //Div6-PT2-MM-CH-Camera_VFS-00+[	
    #ifdef MT9P111_USE_VFS
    	if (mt9p111_use_vfs_pll_setting)	
	   rc = mt9p111_i2c_write_table(&mt9p111_vfs_pll_settings_tbl[0],	mt9p111_vfs_pll_settings_tbl_size);	
    	else
    #endif	
    //Div6-PT2-MM-CH-Camera_VFS-00+]	
	    rc = mt9p111_i2c_write_table(&mt9p111_regs.plltbl[0], mt9p111_regs.plltbl_size);
	if (rc < 0){
		printk("Finish PLL failed.\n");
		goto init_probe_fail;
	}
	else
		printk("Finish PLL Setting.\n");
	/* PLL Setup End   */

//	rc = mt9p111_reg_init();
//	if (rc < 0)
//		goto init_probe_fail;

	/* Read the Model ID of the sensor */
	rc = mt9p111_i2c_read(mt9p111_client->addr,
		REG_mt9p111_MODEL_ID, &model_id, WORD_LEN);
	if (rc < 0)
		goto init_probe_fail;

	printk("mt9p111 model_id = 0x%x\n", model_id);

	/* Check if it matches it with the value in Datasheet */
	if (model_id != mt9p111_MODEL_ID) {
		rc = -EINVAL;
		goto init_probe_fail;
	}
	if(ISmt9p111_init!=1)
	{
		complete(&mt9p111_reginit_comp);
	}
	//Div6-PT2-MM-CH-CameraTime-00+[
	mt9p111_exposure_mode=-1;
	//mt9p111_antibanding_mode=-1;
	//mt9p111_brightness_mode=-1;
	//mt9p111_contrast_mode=-1;
	mt9p111_effect_mode=-1;
	//mt9p111_saturation_mode=-1;
	//mt9p111_sharpness_mode=-1;
	//mt9p111_whitebalance_mode=-1;
	//Div6-PT2-MM-CH-CameraTime-00+]
	return rc;

init_probe_fail:
	printk("mt9p111_sensor_init_probe FAIL.\n");
	return rc;
}

int mt9p111_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	uint16_t model_id = 0,standby_i2c_state=0;

	mt9p111_ctrl = kzalloc(sizeof(struct mt9p111_ctrl), GFP_KERNEL);
	if (!mt9p111_ctrl) {
		printk(KERN_ERR"mt9p111_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		mt9p111_ctrl->sensordata = data;
	if(mt9p111_first_init==1){
		mt9p111_power_off();
		msleep(50);
	
	rc=mt9p111_power_on();
	if (rc < 0)
	{
		printk(KERN_ERR "mt9p111_msg: in mt9p111_sensor_init() to mt9p111_power_on failed.rc=%d.\n",rc);
		goto init_fail;
	}
	
	rc=mt9p111_reset(data);
	if (rc < 0)
	{
		printk(KERN_ERR "mt9p111_msg: in mt9p111_sensor_init() to mt9p111_reset  failed.rc=%d.\n",rc);
		goto init_fail;
	}


	/* Input MCLK = 24MHz */

	msm_camio_clk_rate_set(24000000);
	mdelay(1);
	rc = gpio_request(g_mt9p111_gpio_reset, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 1);
	}
	gpio_free(g_mt9p111_gpio_reset);
	mdelay(5);

//	msm_camio_camif_pad_reg_reset();
		is_regThreadEnd=0;
	mt9p111_reg_init_thread_id = kernel_thread(mt9p111_reg_init_thread, NULL, CLONE_FS | CLONE_FILES);	

	rc = mt9p111_sensor_init_probe(data);
	if (rc < 0) {
		printk(KERN_ERR"mt9p111_sensor_init failed!\n");
		goto init_fail;
	}
		mt9p111_first_init=0;
	}else{

		if( hwGpioPin_cam_if_sw != 255 )
		{
			gpio_tlmm_config(GPIO_CFG( hwGpioPin_cam_if_sw, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
			rc = gpio_request(hwGpioPin_cam_if_sw, "mt9p111");
			if (!rc) {
				rc = gpio_direction_output(hwGpioPin_cam_if_sw, 0);
			}
			gpio_free(hwGpioPin_cam_if_sw);
			msleep( 5 );
			printk( "mt9p111 hwGpioPin_cam_if_sw:GPIO-%d value=%d\r\n", hwGpioPin_cam_if_sw,gpio_get_value(hwGpioPin_cam_if_sw) );
		}
		msm_camio_clk_rate_set(24000000);
		gpio_tlmm_config(GPIO_CFG(15, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
//		msm_camio_camif_pad_reg_reset();
		printk(KERN_INFO "%s: Input MCLK = 24MHz.\n", __func__);
		msleep(20);
		
	        rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0018, &standby_i2c_state, WORD_LEN);
       	 if (rc < 0)
	            goto init_fail;
		if(standby_i2c_state!=0x6009)
		{
			printk(KERN_ERR"mt9p111_sensor_init previous state is not in standby mode!standby_i2c_state=0x%x\n",standby_i2c_state);
			rc = -EINVAL;
			goto init_fail;
		}
		 	


		mtp9111_sensor_standby(1);
		msleep(20);
		
		rc = mt9p111_i2c_read(mt9p111_client->addr,
			REG_mt9p111_MODEL_ID, &model_id, WORD_LEN);
		if (rc < 0)
			goto init_fail;

		printk("mt9p111 model_id = 0x%x\n", model_id);

		/* Check if it matches it with the value in Datasheet */
		if (model_id != mt9p111_MODEL_ID) {
			rc = -EINVAL;
			goto init_fail;
		}


		rc = mt9p111_i2c_read(mt9p111_client->addr,
			0x3000, &model_id, WORD_LEN);
		if (rc < 0)
			goto init_fail;

		printk("From 0x3000 mt9p111 model_id = 0x%x\n", model_id);

 
	}
/*
	rc=mt9p111_reg_init_PATCH();
	if (rc < 0) {
		printk(KERN_ERR"mt9p111_reg_init_PATCH failed!\n");
		goto init_fail;
	}
*/	
	mt9p111_thread_id = kernel_thread(mt9p111_flashled_off_thread, NULL, CLONE_FS | CLONE_FILES);//Only add flash led  in initial.

init_done:
	return rc;

init_fail:
	mt9p111_first_init=1;//Added for standby mode,Henry.
	kfree(mt9p111_ctrl);
	return rc;
}

static int mt9p111_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9p111_wait_queue);
	return 0;
}

int mt9p111_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	/* down(&mt9p111_sem); */

	CDBG("mt9p111_ioctl, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = mt9p111_set_sensor_mode(
					 cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		rc = mt9p111_set_effect(cfg_data.mode,
					 cfg_data.cfg.effect);
		break;

	case CFG_GET_AF_MAX_STEPS:

		break;

    	case CFG_SET_BRIGHTNESS:
	    	rc = mt9p111_set_brightness(
			                 cfg_data.mode,
					 cfg_data.cfg.brightness);
    		break;

    	case CFG_SET_CONTRAST:
	    	rc = mt9p111_set_contrast(
				    	 cfg_data.mode,
			    		 cfg_data.cfg.contrast);
		break;

	case CFG_SET_EXPOSURE_MODE:
    		rc = mt9p111_set_exposure_mode(
    					 cfg_data.mode,
	    				 cfg_data.cfg.exposuremod);
    		break;
			
    	case CFG_SET_SATURATION:
		rc = mt9p111_set_saturation(
					cfg_data.mode,
					cfg_data.cfg.saturation);	
    		break;		
			
    	case CFG_SET_WB:
	    	rc = mt9p111_set_wb(
		    			cfg_data.mode,
			    		cfg_data.cfg.wb);
    		break;

    	case CFG_SET_ANTIBANDING:
	    	rc = mt9p111_set_antibanding(
		    			 cfg_data.mode,
			    		 cfg_data.cfg.antibanding);
    		break;			
	//Div6-PT2-MM-CH-CameraSetting-00+[
	case CFG_SET_SHARPNESS:
		rc = mt9p111_set_sharpness(cfg_data.mode,
					cfg_data.cfg.sharpness);
	
		break;
	//Div6-PT2-MM-CH-CameraSetting-00+]
    	case CFG_SET_LEDMOD:		
    		rc = mt9p111_set_ledmod(
					cfg_data.mode,
					cfg_data.cfg.ledmod);
    		break;
	//Div6-PT2-MM-CH-AF-00+[
	case CFG_SET_AUTOFOCUS:
		rc = mt9p111_set_autofocus(
					cfg_data.mode,
					cfg_data.cfg.autofocus);
		if (rc == -1) {
			cfg_data.cfg.pictl_pf = 1;  // means af fail
			/* copy back to user space */
			if (copy_to_user((void *)argp,
					&cfg_data,
					sizeof(struct sensor_cfg_data))) {
				printk("mt9p111_sensor_config(): ERR_COPY_TO_USER FAIL.\n");
				rc = -EFAULT;
			}
			rc = 0;
		}
//Henry added this for returning AF result.++		
		else
		{
			cfg_data.cfg.pictl_pf = 0;  // means af success.
			/* copy back to user space */
			if (copy_to_user((void *)argp,
					&cfg_data,
					sizeof(struct sensor_cfg_data))) {
				printk("mt9p111_sensor_config(): ERR_COPY_TO_USER FAIL.\n");
			}
			rc = 0;
		}
		CDBG("mt9p111_sensor_config(): cfg_data.cfg.pictl_pf = %d.\n",cfg_data.cfg.pictl_pf);
//Henry added this for returning AF result.--		
		break;			
	//Div6-PT2-MM-CH-AF-00+]			
	//For FTM test item - Camera Ping++
	default:
		break;
	//For FTM test item - Camera Ping--
	}

	/* up(&mt9p111_sem); */

	return rc;
}

int mt9p111_sensor_release(void)
{
	int rc = 0;

	/* down(&mt9p111_sem); */
	printk("mt9p111_sensor_release()+++\n");
//	mt9p111_power_off();
	//Henry added for supporting standby mode.++
	mtp9111_sensor_standby(0);
	gpio_tlmm_config(GPIO_CFG(15, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_DISABLE);
	printk("%s: mt9p111 disable mclk\n", __func__);
	//Henry added for supporting standby mode.--
	
	mutex_lock(&mt9p111_mut);
	kfree(mt9p111_ctrl);
	mt9p111_ctrl = NULL;
	/* up(&mt9p111_sem); */
	mutex_unlock(&mt9p111_mut);
	complete(&mt9p111_flashled_comp);//Henry ++ to avoid thread pending.
	msleep(5);
	printk("mt9p111_sensor_release()---\n");
	mt9p111_snapshoted = 0; //Div6-PT2-MM-CH-CameraTime-00+
	mt9p111_AFsetting = 0;
	return rc;
}
static enum hrtimer_restart mt9p111_flashled_timer_func(struct hrtimer *timer)
{
	printk(KERN_INFO "mt9p111_msg: mt9p111_flashled_timer_func mt9p111_m_ledmod = %d\n", mt9p111_m_ledmod);


	if (!	mpp_config_digital_out(18,MPP_CFG(MPP_DLOGIC_LVL_MSMP,	MPP_DLOGIC_OUT_CTRL_HIGH)))   //80;350
		printk(KERN_INFO "mt9p111_msg: open flash led success.\n");
	else
		printk(KERN_ERR "mt9p111_msg: open flash led fail.\n");
	
	complete(&mt9p111_flashled_comp);
	
	return HRTIMER_NORESTART;
}

static int mt9p111_flashled_off_thread(void *arg)
{
	printk(KERN_INFO "mt9p111_msg: mt9p111_flashled_off_thread running\n");

	while (1) {
		wait_for_completion(&mt9p111_flashled_comp);
		printk(KERN_INFO "%s: Got flashled complete signal\n", __func__);
		if(mt9p111_ctrl==NULL)
		{
			printk(KERN_INFO "%s: End mt9p111_flashled_off_thread!\n", __func__);			
			break;
		}
		msleep(350);  // Change to 350ms as Ivan's request		
		
		if (!mpp_config_digital_out(18,MPP_CFG(MPP_DLOGIC_LVL_MSMP,MPP_DLOGIC_OUT_CTRL_LOW)))   //80;350
			printk(KERN_INFO "mt9p111_msg: close flash led success.\n");
		else
			printk(KERN_ERR "mt9p111_msg: close flash led fail.\n");
		
		flash_settime(0);
	}
	
    return 0;
}

/*
int mt9p111_reg_init_PATCH()
{
	long rc;
	printk(KERN_INFO "mt9p111_msg: mt9p111_reg_init_PATCH starts!\n");

		
        //Div6-PT2-MM-CH-Camera_VFS-01*]   

	//For section 3 - patch ++
	printk("++Start Patch Setting.\n");
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x0982, 0x00, BYTE_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x098A, 0x0000, WORD_LEN);
		
	//rc = mt9p111_i2c_write_table(&mt9p111_regs.patchtbl[0], mt9p111_regs.patchtbl_size);
	rc = mt9p111_i2c_write_table_burst_mode(&mt9p111_regs.patchtbl[0], mt9p111_regs.patchtbl_size);
	if (rc < 0)
		return rc;

	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8DFC, 0x0000, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8DFE, 0x0998, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x0016, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8016, 0x086C, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8002, 0x0001, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x800A, 0x07, BYTE_POLL);
	printk("--Finish Patch Setting.\n");
	//For section 3 - patch --
	printk("++Start Char Setting.\n");
	rc = mt9p111_i2c_write_table(&mt9p111_regs.chartbl[0], mt9p111_regs.chartbl_size);
	if (rc < 0)
		return rc;
	else
		printk("--Finish Char Setting.\n");
	msleep(5);
	
	printk("++Start IQ Setting.\n");		
	rc = mt9p111_i2c_write_table(&mt9p111_regs.iqtbl[0], mt9p111_regs.iqtbl_size);
	if (rc < 0)
		return rc;
	else
		printk("Finish IQ Setting.\n");

	//IQ registers will overwrite parameters set from AP, so workaround it++

	wb_tmp = mt9p111_whitebalance_mode;
	brightness_tmp = mt9p111_brightness_mode;
	ab_tmp = mt9p111_antibanding_mode;
	contrast_tmp = mt9p111_contrast_mode;
	saturation_tmp = mt9p111_saturation_mode;
	sharpness_tmp = mt9p111_sharpness_mode;

	mt9p111_whitebalance_mode=-1;
	mt9p111_brightness_mode=-1;
	mt9p111_antibanding_mode=-1;
	mt9p111_contrast_mode=-1;
	mt9p111_saturation_mode=-1;
	mt9p111_sharpness_mode=-1;

	mt9p111_set_brightness(0, brightness_tmp); 
	mt9p111_set_antibanding(0, ab_tmp); 
	mt9p111_set_contrast(0, contrast_tmp); 
	mt9p111_set_saturation(0, saturation_tmp); 
	mt9p111_set_sharpness(0, sharpness_tmp); 
	mt9p111_set_wb(0, wb_tmp);

	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8404, 0x06, BYTE_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8404, 0x00, BYTE_POLL);
	//IQ registers will overwrite parameters set from AP, so workaround it--
	
	printk(KERN_INFO "%s: Finish \n", __func__);
    return 0;
}
*/

static const struct mt9p111_i2c_reg_conf const AI1D_OTP[] = {
	{ 0x3640, 0x01D0, WORD_LEN, 0 },
	{ 0x3642, 0x0AAE, WORD_LEN, 0 },
	{ 0x3644, 0x2731, WORD_LEN, 0 },
	{ 0x3646, 0xE6CC, WORD_LEN, 0 },
	{ 0x3648, 0xBA90, WORD_LEN, 0 },
	{ 0x364A, 0x0190, WORD_LEN, 0 },
	{ 0x364C, 0x9CEC, WORD_LEN, 0 },
	{ 0x364E, 0x7F90, WORD_LEN, 0 },
	{ 0x3650, 0x6F0C, WORD_LEN, 0 },
	{ 0x3652, 0xDF30, WORD_LEN, 0 },
	{ 0x3654, 0x0330, WORD_LEN, 0 },
	{ 0x3656, 0x486D, WORD_LEN, 0 },
	{ 0x3658, 0x4430, WORD_LEN, 0 },
	{ 0x365A, 0x886D, WORD_LEN, 0 },
	{ 0x365C, 0xCF0F, WORD_LEN, 0 },
	{ 0x365E, 0x0150, WORD_LEN, 0 },
	{ 0x3660, 0xC4CC, WORD_LEN, 0 },
	{ 0x3662, 0x26D1, WORD_LEN, 0 },
	{ 0x3664, 0xCACB, WORD_LEN, 0 },
	{ 0x3666, 0xD8F0, WORD_LEN, 0 },
	{ 0x3680, 0x53AB, WORD_LEN, 0 },
	{ 0x3682, 0x97AE, WORD_LEN, 0 },
	{ 0x3684, 0xD98F, WORD_LEN, 0 },
	{ 0x3686, 0x2C2E, WORD_LEN, 0 },
	{ 0x3688, 0x26CF, WORD_LEN, 0 },
	{ 0x368A, 0x76ED, WORD_LEN, 0 },
	{ 0x368C, 0x70ED, WORD_LEN, 0 },
	{ 0x368E, 0x8CEF, WORD_LEN, 0 },
	{ 0x3690, 0xF30E, WORD_LEN, 0 },
	{ 0x3692, 0x00EE, WORD_LEN, 0 },
	{ 0x3694, 0xDEAC, WORD_LEN, 0 },
	{ 0x3696, 0x570D, WORD_LEN, 0 },
	{ 0x3698, 0xA9CC, WORD_LEN, 0 },
	{ 0x369A, 0xBC2E, WORD_LEN, 0 },
	{ 0x369C, 0x1989, WORD_LEN, 0 },
	{ 0x369E, 0xC32B, WORD_LEN, 0 },
	{ 0x36A0, 0x888E, WORD_LEN, 0 },
	{ 0x36A2, 0xEF2E, WORD_LEN, 0 },
	{ 0x36A4, 0x7B8E, WORD_LEN, 0 },
	{ 0x36A6, 0x674E, WORD_LEN, 0 },
	{ 0x36C0, 0x3791, WORD_LEN, 0 },
	{ 0x36C2, 0x416D, WORD_LEN, 0 },
	{ 0x36C4, 0x8711, WORD_LEN, 0 },
	{ 0x36C6, 0x2BCE, WORD_LEN, 0 },
	{ 0x36C8, 0x28F0, WORD_LEN, 0 },
	{ 0x36CA, 0x1BF1, WORD_LEN, 0 },
	{ 0x36CC, 0x9B0F, WORD_LEN, 0 },
	{ 0x36CE, 0xA6B1, WORD_LEN, 0 },
	{ 0x36D0, 0x2190, WORD_LEN, 0 },
	{ 0x36D2, 0x1512, WORD_LEN, 0 },
	{ 0x36D4, 0x7970, WORD_LEN, 0 },
	{ 0x36D6, 0x52EE, WORD_LEN, 0 },
	{ 0x36D8, 0xBFB0, WORD_LEN, 0 },
	{ 0x36DA, 0x188E, WORD_LEN, 0 },
	{ 0x36DC, 0x0851, WORD_LEN, 0 },
	{ 0x36DE, 0x3611, WORD_LEN, 0 },
	{ 0x36E0, 0xB8AF, WORD_LEN, 0 },
	{ 0x36E2, 0x95B1, WORD_LEN, 0 },
	{ 0x36E4, 0x49B0, WORD_LEN, 0 },
	{ 0x36E6, 0x0EAF, WORD_LEN, 0 },
	{ 0x3700, 0xDBCE, WORD_LEN, 0 },
	{ 0x3702, 0xB60D, WORD_LEN, 0 },
	{ 0x3704, 0xF72E, WORD_LEN, 0 },
	{ 0x3706, 0x072E, WORD_LEN, 0 },
	{ 0x3708, 0x1DD3, WORD_LEN, 0 },
	{ 0x370A, 0x804D, WORD_LEN, 0 },
	{ 0x370C, 0xA1CD, WORD_LEN, 0 },
	{ 0x370E, 0xD6B0, WORD_LEN, 0 },
	{ 0x3710, 0x1AB0, WORD_LEN, 0 },
	{ 0x3712, 0x3CF3, WORD_LEN, 0 },
	{ 0x3714, 0xA1CF, WORD_LEN, 0 },
	{ 0x3716, 0x998F, WORD_LEN, 0 },
	{ 0x3718, 0x178E, WORD_LEN, 0 },
	{ 0x371A, 0x1CF1, WORD_LEN, 0 },
	{ 0x371C, 0x21B3, WORD_LEN, 0 },
	{ 0x371E, 0x9DAF, WORD_LEN, 0 },
	{ 0x3720, 0x5E4B, WORD_LEN, 0 },
	{ 0x3722, 0x166C, WORD_LEN, 0 },
	{ 0x3724, 0x8650, WORD_LEN, 0 },
	{ 0x3726, 0x3F53, WORD_LEN, 0 },
	{ 0x3740, 0xA850, WORD_LEN, 0 },
	{ 0x3742, 0xBA2F, WORD_LEN, 0 },
	{ 0x3744, 0x84B4, WORD_LEN, 0 },
	{ 0x3746, 0x14B3, WORD_LEN, 0 },
	{ 0x3748, 0x3D36, WORD_LEN, 0 },
	{ 0x374A, 0xCB90, WORD_LEN, 0 },
	{ 0x374C, 0x2EEE, WORD_LEN, 0 },
	{ 0x374E, 0x9BD3, WORD_LEN, 0 },
	{ 0x3750, 0x04F2, WORD_LEN, 0 },
	{ 0x3752, 0x0A76, WORD_LEN, 0 },
	{ 0x3754, 0x7D2B, WORD_LEN, 0 },
	{ 0x3756, 0xF28F, WORD_LEN, 0 },
	{ 0x3758, 0xB333, WORD_LEN, 0 },
	{ 0x375A, 0x7A72, WORD_LEN, 0 },
	{ 0x375C, 0x0FF6, WORD_LEN, 0 },
	{ 0x375E, 0xF90F, WORD_LEN, 0 },
	{ 0x3760, 0x2AF0, WORD_LEN, 0 },
	{ 0x3762, 0x8C34, WORD_LEN, 0 },
	{ 0x3764, 0x00B1, WORD_LEN, 0 },
	{ 0x3766, 0x4776, WORD_LEN, 0 },
	{ 0x3782, 0x03A0, WORD_LEN, 0 },
	{ 0x3784, 0x0530, WORD_LEN, 0 }, 
};

static int mt9p111_reg_init_thread(void *arg)
{
	long rc;
	printk(KERN_INFO "mt9p111_msg: mt9p111_reg_init_thread running\n");

	daemonize("mt9p111_reg_init_thread");

//    while (1) {
		wait_for_completion(&mt9p111_reginit_comp);
		printk(KERN_INFO "%s: Got reginit complete signal\n", __func__);

        //Div6-PT2-MM-CH-Camera_VFS-01*[		
        #ifdef MT9P111_USE_VFS
        if (mt9p111_use_vfs_patch_setting)	
        {
		    rc = mt9p111_i2c_write_table(&mt9p111_vfs_patch_settings_tbl[0],	mt9p111_vfs_patch_settings_tbl_size);	
			if (mt9p111_use_vfs_patch2_setting)	
            {
		        rc = mt9p111_i2c_write_table(&mt9p111_vfs_patch2_settings_tbl[0],	mt9p111_vfs_patch2_settings_tbl_size);	
				if (mt9p111_use_vfs_patch3_setting)	
                {
		            rc = mt9p111_i2c_write_table(&mt9p111_vfs_patch3_settings_tbl[0],	mt9p111_vfs_patch3_settings_tbl_size);	
                }
            }
        }
        else
        #endif			
        //Div6-PT2-MM-CH-Camera_VFS-01*]   

	//For section 3 - patch ++
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x0982, 0x00, BYTE_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x098A, 0x0000, WORD_LEN);
		
	//rc = mt9p111_i2c_write_table(&mt9p111_regs.patchtbl[0], mt9p111_regs.patchtbl_size);
	rc = mt9p111_i2c_write_table_burst_mode(&mt9p111_regs.patchtbl[0], mt9p111_regs.patchtbl_size);
	if (rc < 0)
		return rc;

	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8DFC, 0x0000, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8DFE, 0x0998, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x098E, 0x0016, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8016, 0x086C, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8002, 0x0001, WORD_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x800A, 0x07, BYTE_POLL);
	printk("Finish Patch Setting.\n");
	//For section 3 - patch --

	rc = mt9p111_i2c_write_table(&mt9p111_regs.chartbl[0], mt9p111_regs.chartbl_size);
	if (rc < 0)
		return rc;
	else
		printk("Finish Char Setting.\n");


//Henry added this for load OTP starting address dynamicly.++
	mt9p111_i2c_write(mt9p111_client->addr,0x381C, 0x0000, WORD_LEN);// Aptina added this on 1223.
	mt9p111_i2c_write(mt9p111_client->addr,0xE02A, 0x0001, WORD_LEN);// IO_NV_MEM_COMMAND
	mt9p111_i2c_write(mt9p111_client->addr,0x3812, 0x2124, WORD_LEN);//
	mt9p111_i2c_write(mt9p111_client->addr,0xE023, 0xC1, BYTE_POLL);//{ 0xE023, 0x40, BIT_POLL, 0 },
	mt9p111_i2c_write(mt9p111_client->addr,0xD004, 0x04, BYTE_LEN);// PGA_SOLUTION  //{ 0xD004, 0x04, BYTE_LEN, 100 },	
	if(device_PRODUCT_ID==Project_AI1D || device_PRODUCT_ID== Project_AI1S)
	{

		mt9p111_i2c_write(mt9p111_client->addr, 0xD006, 0x0000, WORD_LEN);
		printk("Project Ai1, set D006 as 0x0000.\n");
	}
	else
	{
		mt9p111_i2c_write(mt9p111_client->addr,0xD006, 0x0008, WORD_LEN);// 	// PGA_ZONE_ADDR_0         
		printk("Project MCNEX sensor, set D006 as 0x0008.\n");
	}
	mt9p111_i2c_write(mt9p111_client->addr,0xD005, 0x00, BYTE_LEN);// 	// PGA_CURRENT_ZONE                   
	mt9p111_i2c_write(mt9p111_client->addr,0xD002, 0x8002, WORD_LEN);// 	// PGA_ALGO                         
	mt9p111_i2c_write(mt9p111_client->addr,0x3210, 0x49B8, WORD_LEN);// 
//Henry added this for load OTP starting address dynamicly.--

	if(device_PRODUCT_ID==Project_AI1D || device_PRODUCT_ID== Project_AI1S)
	{
		short unsigned int R0x380C=0;
		mt9p111_i2c_write(mt9p111_client->addr, 0x098e, 0x602a, WORD_LEN);
		mt9p111_i2c_write(mt9p111_client->addr, 0xE02a, 0x0001, WORD_LEN);
		msleep(100);
		mt9p111_i2c_write(mt9p111_client->addr, 0x3802, 0x0000, WORD_LEN);
		mt9p111_i2c_write(mt9p111_client->addr, 0x3804, 0x0000, WORD_LEN);
		mt9p111_i2c_write(mt9p111_client->addr, 0x3802, 0x0009, WORD_LEN);
		mt9p111_i2c_write(mt9p111_client->addr, 0x098e, 0x602a, WORD_LEN);
		mt9p111_i2c_read(mt9p111_client->addr,0x380C, &R0x380C, WORD_LEN);
		printk("Project Ai1,R0x380C = 0x%x.\n",R0x380C);
		if(R0x380C==0)
		{
			//write OTP default value dynamicly.
			rc = mt9p111_i2c_write_table(&AI1D_OTP[0], ARRAY_SIZE(AI1D_OTP));
			if (rc < 0)
				return rc;
			else
				printk("Finish AI1D_OTP dynamic writting.\n");

		}
	}
	
	msleep(5);
		
	rc = mt9p111_i2c_write_table(&mt9p111_regs.iqtbl[0], mt9p111_regs.iqtbl_size);
	if (rc < 0)
		return rc;
	else
		printk("Finish IQ Setting.\n");

	//IQ registers will overwrite parameters set from AP, so workaround it++

	wb_tmp = mt9p111_whitebalance_mode;
	brightness_tmp = mt9p111_brightness_mode;
	ab_tmp = mt9p111_antibanding_mode;
	contrast_tmp = mt9p111_contrast_mode;
	saturation_tmp = mt9p111_saturation_mode;
	sharpness_tmp = mt9p111_sharpness_mode;

	mt9p111_whitebalance_mode=-1;
	mt9p111_brightness_mode=-1;
	mt9p111_antibanding_mode=-1;
	mt9p111_contrast_mode=-1;
	mt9p111_saturation_mode=-1;
	mt9p111_sharpness_mode=-1;
#if 0
	mt9p111_set_brightness(0, brightness_tmp); 
	mt9p111_set_antibanding(0, ab_tmp); 
	mt9p111_set_contrast(0, contrast_tmp); 
	mt9p111_set_saturation(0, saturation_tmp); 
	mt9p111_set_sharpness(0, sharpness_tmp); 
	mt9p111_set_wb(0, wb_tmp);
#endif	

	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8404, 0x06, BYTE_LEN);
	(void)mt9p111_i2c_write(mt9p111_client->addr, 0x8404, 0x00, BYTE_POLL);
	//IQ registers will overwrite parameters set from AP, so workaround it--

	#if 0
    	/* AF Setting */
        //Div6-PT2-MM-CH-Camera_VFS-00+[
        #ifdef MT9P111_USE_VFS
        if (mt9p111_use_vfs_af_setting)	
		rc = mt9p111_i2c_write_table(&mt9p111_vfs_af_settings_tbl[0],	mt9p111_vfs_af_settings_tbl_size);	
        else
        #endif	
        //Div6-PT2-MM-CH-Camera_VFS-00+]		
	        rc = mt9p111_i2c_write_table(&mt9p111_regs.aftbl[0], mt9p111_regs.aftbl_size);
    	if (rc < 0)
	    	return rc;
	else
		printk("Finish AF Setting.\n");
    	/* AF Setting  */		
	#endif
	
	printk(KERN_INFO "%s: Finish \n", __func__);
//    }
	is_regThreadEnd=1;//add for standby mode,Henry.
    return 0;
}
//Div6-PT2-MM-CH-Camera_VFS-00+[

int mtp9111_sensor_standby(int onoff)
{
    int rc = 0;

    uint16_t standby_i2c = 0x00;
    uint16_t standby_i2c_state = 0;
    uint16_t en_vdd_dist_soft_state = 0;
    uint16_t retry_count=0; 

    if(onoff==0)//entering stanby mode
    {
        printk("mt9p111 entering stanby mode = %d\n", onoff);

        rc = mt9p111_i2c_write(mt9p111_client->addr, 0x060E, 0xFF, WORD_LEN);//This is needed to set if MSMP is 2.6V .Henry.

        rc = mt9p111_i2c_write(mt9p111_client->addr, 0x0028, 0x00, WORD_LEN);
	if (rc < 0)
		return rc;
	else
	{
            rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0028, &en_vdd_dist_soft_state, WORD_LEN);
		if (rc < 0)
			return rc;
		
            printk("mt9p111 en_vdd_dist_soft_state = 0x%x\n", en_vdd_dist_soft_state);
	}

        //================= 0x0018 =====================================
        rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0018, &standby_i2c_state, WORD_LEN);
	if (rc < 0)
		return rc;
	
        printk("mt9p111 standby_i2c_state = 0x%x\n", standby_i2c_state);
        standby_i2c=standby_i2c_state|0x01;

        rc = mt9p111_i2c_write(mt9p111_client->addr, 0x0018, standby_i2c, WORD_LEN);
        if (rc < 0)
            return rc;
        else
        {
            do 
            {
                msleep(50);
                rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0018, &standby_i2c_state, WORD_LEN);
                if(rc < 0)
                    return rc;
                 retry_count++;
                printk("mt9p111 standby_i2c_state = 0x%x\n", standby_i2c_state);
            } 
            while(((standby_i2c_state&0x4000)==0)&&(retry_count<50));
        }
        
        //msleep(150);
    }

    if(onoff==1)//exiting stanby mode
    {
        printk("mt9p111 exiting stanby mode = %d\n", onoff);

        rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0018, &standby_i2c_state, WORD_LEN);
        if (rc < 0)
            return rc;

        printk("mt9p111 read standby_i2c_state = 0x%x\n", standby_i2c_state);
        standby_i2c=standby_i2c_state&0xFFFE;

        rc = mt9p111_i2c_write(mt9p111_client->addr, 0x0018, standby_i2c, WORD_LEN);
        if (rc < 0)
            return rc;
        rc = mt9p111_i2c_read(mt9p111_client->addr, 0x0018, &standby_i2c_state, WORD_LEN);
        if (rc < 0)
            return rc;

        printk("mt9p111 read standby_i2c_state = 0x%x\n", standby_i2c_state);

        msleep(20);
    }
	
    return rc;
}


#ifdef MT9P111_USE_VFS
void mt9p111_get_param(const char *buf, size_t count, struct mt9p111_i2c_reg_conf *tbl, 
	unsigned short tbl_size, int *use_setting, int param_num)
{
	unsigned short waddr;
	unsigned short wdata;
	enum mt9p111_width width;
	unsigned short mdelay_time;

	char param1[10],param2[10],param3[10],param4[10];
	int read_count;
	const char *pstr;
	int vfs_index=0;
	pstr=buf;

	CDBG("count=%d\n",count);
 
	do
	{
		if (param_num ==3)
			read_count=sscanf(pstr,"%4s,%4s,%1s",param1,param2,param3); 
		else
			read_count=sscanf(pstr,"%4s,%4s,%1s,%4s",param1,param2,param3,param4); 
 	
		if (read_count ==3 && param_num ==3)
		{
			waddr=simple_strtoul(param1,NULL,16);
			wdata=simple_strtoul(param2,NULL,16);
			width=simple_strtoul(param3,NULL,16);
			mdelay_time=0;
 			
			tbl[vfs_index].waddr= waddr;
			tbl[vfs_index].wdata= wdata;
			tbl[vfs_index].width= width;
			tbl[vfs_index].mdelay_time= mdelay_time;
			vfs_index++;

			if (vfs_index <= tbl_size)
			{
				CDBG("Just match MAX_VFS_INDEX\n"); 			
				*use_setting=1;
			}else if (vfs_index > tbl_size)
			{
				CDBG("Out of range MAX_VFS_INDEX\n"); 								
				*use_setting=0;
         		pmic_flash_led_set_current(5);
        		msleep(100);
        		pmic_flash_led_set_current(0);		
				break;
			}
			
		}else if (read_count==4 && param_num ==4)
		{
			if (!strcmp(param1, "undo"))
			{
			    *use_setting=0;				
			    break;
			}	
			
			waddr=simple_strtoul(param1,NULL,16);
			wdata=simple_strtoul(param2,NULL,16);
			width=simple_strtoul(param3,NULL,16);
			mdelay_time=simple_strtoul(param4,NULL,10); 	
				
			tbl[vfs_index].waddr= waddr;
			tbl[vfs_index].wdata= wdata;
			tbl[vfs_index].width= width;
			tbl[vfs_index].mdelay_time= mdelay_time;
			vfs_index++;

			if (vfs_index <= tbl_size)
			{
				CDBG("Just match MAX_VFS_INDEX\n");
				*use_setting=1;
			}else if (vfs_index > tbl_size)
			{
				CDBG("Out of range MAX_VFS_INDEX\n");
				*use_setting=0;
         		pmic_flash_led_set_current(5);
        		msleep(100);
        		pmic_flash_led_set_current(0);					
				break;
			}			
		}else{
			tbl[vfs_index].waddr= 0xFFFF;
			tbl[vfs_index].wdata= 0xFFFF;
			tbl[vfs_index].width= 1;
			tbl[vfs_index].mdelay_time= 0; 
			*use_setting=0;
       		pmic_flash_led_set_current(5);
      		msleep(100);
       		pmic_flash_led_set_current(0);				
			break;
		}
		/* get next line */
		pstr=strchr(pstr, '\n');
		if (pstr==NULL) 
		{
			tbl[vfs_index].waddr= 0xFFFF;
			tbl[vfs_index].wdata= 0xFFFF;
			tbl[vfs_index].width= 1;
			tbl[vfs_index].mdelay_time= 0; 			
			break; 
		}
		pstr++;
	}while(read_count!=0);


}

static ssize_t mt9p111_write_prev_snap_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_prev_snap_settings_tbl[0], mt9p111_vfs_prev_snap_settings_tbl_size, &mt9p111_use_vfs_prev_snap_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

//Div6-PT2-MM-CH-Camera_VFS-01+[
static ssize_t mt9p111_write_prev_snap2_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_prev_snap2_settings_tbl[0], mt9p111_vfs_prev_snap2_settings_tbl_size, &mt9p111_use_vfs_prev_snap2_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_prev_snap3_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_prev_snap3_settings_tbl[0], mt9p111_vfs_prev_snap3_settings_tbl_size, &mt9p111_use_vfs_prev_snap3_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_prev_snap4_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_prev_snap4_settings_tbl[0], mt9p111_vfs_prev_snap4_settings_tbl_size, &mt9p111_use_vfs_prev_snap4_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_prev_snap5_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_prev_snap5_settings_tbl[0], mt9p111_vfs_prev_snap5_settings_tbl_size, &mt9p111_use_vfs_prev_snap5_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_patch2_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_patch2_settings_tbl[0], mt9p111_vfs_patch2_settings_tbl_size, &mt9p111_use_vfs_patch2_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_patch3_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_patch3_settings_tbl[0], mt9p111_vfs_patch3_settings_tbl_size, &mt9p111_use_vfs_patch3_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}
//Div6-PT2-MM-CH-Camera_VFS-01+]

static ssize_t mt9p111_write_patch_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_patch_settings_tbl[0], mt9p111_vfs_patch_settings_tbl_size, &mt9p111_use_vfs_patch_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_pll_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_pll_settings_tbl[0], mt9p111_vfs_pll_settings_tbl_size, &mt9p111_use_vfs_pll_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_write_af_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_af_settings_tbl[0], mt9p111_vfs_af_settings_tbl_size, &mt9p111_use_vfs_af_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}
static ssize_t mt9p111_write_reg(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	long rc = 0;

	mt9p111_get_param(buf, count, &mt9p111_vfs_writereg_settings_tbl[0], mt9p111_vfs_writereg_settings_tbl_size, &mt9p111_use_vfs_writereg_setting, MT9P111_VFS_PARAM_NUM); 
	if (mt9p111_use_vfs_writereg_setting)
	{
		rc = mt9p111_i2c_write_table(&mt9p111_vfs_writereg_settings_tbl[0],
			mt9p111_vfs_writereg_settings_tbl_size);
		mt9p111_use_vfs_writereg_setting =0;
	}
	return count;
}

static ssize_t mt9p111_setrange(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	mt9p111_get_param(buf, count, &mt9p111_vfs_getreg_settings_tbl[0], mt9p111_vfs_getreg_settings_tbl_size, &mt9p111_use_vfs_getreg_setting, MT9P111_VFS_PARAM_NUM); 
	return count;
}

static ssize_t mt9p111_getrange(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i,rc;
	char *str = buf;

	if (mt9p111_use_vfs_getreg_setting)
	{
		for (i=0;i<=mt9p111_vfs_getreg_settings_tbl_size;i++)
		{
			if (mt9p111_vfs_getreg_settings_tbl[i].waddr==0xFFFF)
				break;

			rc = mt9p111_i2c_read(mt9p111_client->addr,
				mt9p111_vfs_getreg_settings_tbl[i].waddr, &(mt9p111_vfs_getreg_settings_tbl[i].wdata), mt9p111_vfs_getreg_settings_tbl[i].width);
			if (rc <0)
			{
				pmic_flash_led_set_current(5);
      		    msleep(100);
        		pmic_flash_led_set_current(0);	
				break;
			}
			
			CDBG("mt9p111 reg 0x%4X = 0x%2X\n", mt9p111_vfs_getreg_settings_tbl[i].waddr, mt9p111_vfs_getreg_settings_tbl[i].wdata);

			str += sprintf(str, "%04X,%2X \n", mt9p111_vfs_getreg_settings_tbl[i].waddr, 
				mt9p111_vfs_getreg_settings_tbl[i].wdata);

		}
	}
	return (str - buf);
}

static ssize_t mt9p111_setflashtime(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf,"%d",&mt9p111_use_vfs_flashtime_setting);
	return count;
}

static ssize_t mt9p111_getflashtime(struct device *dev, struct device_attribute *attr, char *buf)
{
	return (sprintf(buf,"%d\n",mt9p111_use_vfs_flashtime_setting));
}

DEVICE_ATTR(prev_snap_reg_mt9p111, 0666, NULL, mt9p111_write_prev_snap_reg);
//Div6-PT2-MM-CH-Camera_VFS-01+[
DEVICE_ATTR(prev_snap2_reg_mt9p111, 0666, NULL, mt9p111_write_prev_snap2_reg);
DEVICE_ATTR(prev_snap3_reg_mt9p111, 0666, NULL, mt9p111_write_prev_snap3_reg);
DEVICE_ATTR(prev_snap4_reg_mt9p111, 0666, NULL, mt9p111_write_prev_snap4_reg);
DEVICE_ATTR(prev_snap5_reg_mt9p111, 0666, NULL, mt9p111_write_prev_snap5_reg);
DEVICE_ATTR(patch2_reg_mt9p111, 0666, NULL, mt9p111_write_patch2_reg);
DEVICE_ATTR(patch3_reg_mt9p111, 0666, NULL, mt9p111_write_patch3_reg);
//Div6-PT2-MM-CH-Camera_VFS-01+]
DEVICE_ATTR(patch_reg_mt9p111, 0666, NULL, mt9p111_write_patch_reg);
DEVICE_ATTR(pll_reg_mt9p111, 0666, NULL, mt9p111_write_pll_reg);
DEVICE_ATTR(af_reg_mt9p111, 0666, NULL, mt9p111_write_af_reg);
DEVICE_ATTR(writereg_mt9p111, 0666, NULL, mt9p111_write_reg);
DEVICE_ATTR(getreg_mt9p111, 0666, mt9p111_getrange, mt9p111_setrange);
DEVICE_ATTR(flashtime_mt9p111, 0666, mt9p111_getflashtime, mt9p111_setflashtime);

static int create_attributes(struct i2c_client *client)
{
	int rc;

	dev_attr_prev_snap_reg_mt9p111.attr.name = MT9P111_PREV_SNAP_REG;
    //Div6-PT2-MM-CH-Camera_VFS-01+[	
	dev_attr_prev_snap2_reg_mt9p111.attr.name = MT9P111_PREV_SNAP2_REG;
	dev_attr_prev_snap3_reg_mt9p111.attr.name = MT9P111_PREV_SNAP3_REG;
	dev_attr_prev_snap4_reg_mt9p111.attr.name = MT9P111_PREV_SNAP4_REG;	
	dev_attr_prev_snap5_reg_mt9p111.attr.name = MT9P111_PREV_SNAP5_REG;	
	dev_attr_patch2_reg_mt9p111.attr.name = MT9P111_PATCH2_REG;		
	dev_attr_patch3_reg_mt9p111.attr.name = MT9P111_PATCH3_REG;		
    //Div6-PT2-MM-CH-Camera_VFS-01+]
	dev_attr_patch_reg_mt9p111.attr.name = MT9P111_PATCH_REG;	
	dev_attr_pll_reg_mt9p111.attr.name = MT9P111_PLL_REG;	
	dev_attr_af_reg_mt9p111.attr.name = MT9P111_AF_REG;	
	dev_attr_writereg_mt9p111.attr.name = MT9P111_WRITEREG;
	dev_attr_getreg_mt9p111.attr.name = MT9P111_GETREG;
	dev_attr_flashtime_mt9p111.attr.name = MT9P111_FLASHTIME;	
	
	rc = device_create_file(&client->dev, &dev_attr_prev_snap_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"prev_snap_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
    //Div6-PT2-MM-CH-Camera_VFS-01+[
	rc = device_create_file(&client->dev, &dev_attr_prev_snap2_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"prev_snap2_reg\" failed!! <%d>", __func__, rc);

		return rc; 
	}    
	rc = device_create_file(&client->dev, &dev_attr_prev_snap3_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"prev_snap3_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	
	rc = device_create_file(&client->dev, &dev_attr_prev_snap4_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"prev_snap4_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	
	rc = device_create_file(&client->dev, &dev_attr_prev_snap5_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"prev_snap5_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	
	rc = device_create_file(&client->dev, &dev_attr_patch2_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"patch2_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}		
	rc = device_create_file(&client->dev, &dev_attr_patch3_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"patch3_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}		
    //Div6-PT2-MM-CH-Camera_VFS-01+]    
	rc = device_create_file(&client->dev, &dev_attr_patch_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"patch_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_pll_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"pll_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_af_reg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"af_reg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}
	
	rc = device_create_file(&client->dev, &dev_attr_writereg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"writereg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	

	rc = device_create_file(&client->dev, &dev_attr_getreg_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create mt9p111 attribute \"getreg\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	
	
	rc = device_create_file(&client->dev, &dev_attr_flashtime_mt9p111);
	if (rc < 0) {
		dev_err(&client->dev, "%s: Create ov5642 attribute \"flashtime\" failed!! <%d>", __func__, rc);
		
		return rc; 
	}	
	
	return rc;
}
#endif /* MT9P111_USE_VFS */
//Div6-PT2-MM-CH-Camera_VFS-00+]


static int mt9p111_reset(const struct msm_camera_sensor_info *dev)
{
	int rc = 0;
//	int HWID=FIH_READ_HWID_FROM_SMEM();

// add Camera interface switch++
    printk( "mt9p111 device_PRODUCT_ID:%d, device_PHASE_ID:%d\r\n", device_PRODUCT_ID, device_PHASE_ID );
    
    // reset value
    hwGpioPin_cam_if_sw = 255;
    if(device_PRODUCT_ID==Project_DMP )
    {			
    	printk( "mt9p111 Project DMP\r\n" );
    	if( device_PHASE_ID >= Phase_PR2 )
    	{	
    		// For DMP, the camera switch pin enabled after PR2
    		printk( "mt9p111 DMP Phase >= PR2\r\n" );
    		hwGpioPin_cam_if_sw = 17; // GPIO 17						
    	}
    }
    
    if(device_PRODUCT_ID==Project_AI1D || device_PRODUCT_ID==Project_AI1S)
    {			
        printk( "mt9p111 Project AI1D\r\n" );
        hwGpioPin_cam_if_sw = 20; // GPIO 20
    }
    
    if( hwGpioPin_cam_if_sw != 255 )
    {
        gpio_tlmm_config(GPIO_CFG( hwGpioPin_cam_if_sw, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);		
	rc = gpio_request(hwGpioPin_cam_if_sw, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(hwGpioPin_cam_if_sw, 0);
	}
	gpio_free(hwGpioPin_cam_if_sw);
	msleep( 5 );
	printk( "mt9p111 hwGpioPin_cam_if_sw:GPIO-%d value=%d\r\n", hwGpioPin_cam_if_sw,gpio_get_value(hwGpioPin_cam_if_sw) );
    }

// add Camera interface switch--

	gpio_tlmm_config(GPIO_CFG(g_mt9p111_gpio_reset, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(g_mt9p111_gpio_standby, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

// Add this to avoid Ai1 crash.Henry++
	if(!(device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S))
	{
	      gpio_tlmm_config(GPIO_CFG(g_mt9p111_gpio_pwd, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		rc = gpio_request(g_mt9p111_gpio_pwd, "mt9p111");
		if (!rc) {
			rc = gpio_direction_output(g_mt9p111_gpio_pwd, 0);
		}
		gpio_free(g_mt9p111_gpio_pwd);
	}
	else
	{
		printk( "mt9p111: This is Ai1D/Ai1S PR1. There is no PWD pin.\n" );                         
	}
// Add this to avoid Ai1 crash.Henry--
	rc = gpio_request(g_mt9p111_gpio_standby, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_standby, 0);
	}
	gpio_free(g_mt9p111_gpio_standby);
	
#if 0
	gpio_direction_output(g_mt9p111_gpio_standby, 0);	
	msleep(10);

	rc = gpio_request(g_mt9p111_gpio_pwd, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_pwd, 0);
	}
	gpio_free(g_mt9p111_gpio_pwd);
	
	rc = gpio_request(g_mt9p111_gpio_reset, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 0);
		msleep(20);
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 1);
	}
	gpio_free(g_mt9p111_gpio_reset);

#endif




	printk( KERN_INFO"Finish power up sequence.\n");

	
       printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_reset, gpio_get_value(g_mt9p111_gpio_reset));
       printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_standby, gpio_get_value(g_mt9p111_gpio_standby));
       if( !(device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S))
       	printk(KERN_INFO"[MT9P111 5M] gpio_get_value(%d) = %d\n",g_mt9p111_gpio_pwd, gpio_get_value(g_mt9p111_gpio_pwd));


	return rc;
}

static int mt9p111_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9p111_sensorw =
		kzalloc(sizeof(struct mt9p111_work), GFP_KERNEL);

	if (!mt9p111_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9p111_sensorw);
	mt9p111_init_client(client);
	mt9p111_client = client;

	printk("mt9p111_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(mt9p111_sensorw);
	mt9p111_sensorw = NULL;
	printk("mt9p111_probe failed!\n");
	return rc;
}

static const struct i2c_device_id mt9p111_i2c_id[] = {
	{ "mt9p111", 0},
	{ },
};

static struct i2c_driver mt9p111_i2c_driver = {
	.id_table = mt9p111_i2c_id,
	.probe  = mt9p111_i2c_probe,
	.remove = __exit_p(mt9p111_i2c_remove),
	.driver = {
		.name = "mt9p111",
	},
};

//#define  REG_MT9P111_MODEL_ID 0x0000
//#define  MT9T112_MODEL_ID     0x2682
//#define  MT9P111_I2C_READ_SLAVE_ID     0x7A >> 1  
//#define  MT9P111_I2C_WRITE_SLAVE_ID     0x7B >> 1  
static int mt9p111_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&mt9p111_i2c_driver);
	uint16_t model_id = 0;
	printk(KERN_INFO "mt9p111_msg: mt9p111_sensor_probe() is called.\n");
	
	if (rc < 0 || mt9p111_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

//add read HWID for Domino Series, ++
    if(fih_read_new_hwid_mech_from_orighwid())
    {
        device_PRODUCT_ID = fih_read_product_id_from_orighwid() ;      
        device_PHASE_ID = fih_read_phase_id_from_orighwid();
    }
//add read HWID for Domino Series, --

/*
	if( device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S)
	{
		return -ENOTSUPP;
	}
*/	
	if(device_PRODUCT_ID == Project_AI1D || device_PRODUCT_ID == Project_AI1S)
	{
		g_mt9p111_gpio_pwd=255;
		g_mt9p111_gpio_standby=MT9P111_GPIO_STANDBY_AI1;
		g_mt9p111_gpio_reset=MT9P111_GPIO_RESET;
	}
	else
	{
		g_mt9p111_gpio_pwd=MT9P111_GPIO_PWD;
		g_mt9p111_gpio_standby=MT9P111_GPIO_STANDBY;
		g_mt9p111_gpio_reset=MT9P111_GPIO_RESET;		
		}

	printk("mt9p111_msg: Product_ID=0x%x, Phase=0x%x.\n",device_PRODUCT_ID,device_PHASE_ID);	
	printk("mt9p111_msg: PWD_GPIO=%d,STANDBY_GPIO=%d,RESET_GPIO=%d.\n",g_mt9p111_gpio_pwd,g_mt9p111_gpio_standby,g_mt9p111_gpio_reset);
	
		
	rc = gpio_request(g_mt9p111_gpio_reset, "mt9p111");
		if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 0);
		}
	gpio_free(g_mt9p111_gpio_reset);
		
	rc = gpio_request(g_mt9p111_gpio_standby, "mt9p111");
	if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_standby, 0);
	}
	gpio_free(g_mt9p111_gpio_standby);
//only for re-work board.++		

		rc = gpio_request(g_mt9p111_gpio_standby, "mt9p111");
		if (!rc) {
			rc = gpio_direction_output(g_mt9p111_gpio_standby, 0);
	}
		gpio_free(g_mt9p111_gpio_standby);
		
//only for re-work board.--

	
	rc=mt9p111_power_on();
	if (rc < 0) {
		rc = -ENOTSUPP;
		goto probe_done;
	}
	rc=mt9p111_reset(info);
	if (rc < 0)
	{
		printk(KERN_ERR "mt9p111_msg: in mt9p111_sensor_probe() to mt9p111_reset  failed.rc=%d.\n",rc);
		rc = -ENOTSUPP;
		goto probe_done;
	}
	/* Input MCLK = 24MHz */
	msm_camio_clk_rate_set(24000000);
	mdelay(1);
	
	rc = gpio_request(g_mt9p111_gpio_reset, "mt9p111");
		if (!rc) {
		rc = gpio_direction_output(g_mt9p111_gpio_reset, 1);
		}
	gpio_free(g_mt9p111_gpio_reset);
		
	mdelay(20);


	/* Read REG_MT9T112_MODEL_ID_HI & REG_MT9T112_MODEL_ID_LO */
	rc = mt9p111_i2c_read(mt9p111_client->addr,REG_mt9p111_MODEL_ID, &model_id, WORD_LEN);
	if (rc < 0)
	{
		printk(KERN_ERR "mt9p111_msg: in mt9p111_sensor_probe() read model failed rc=%d.\n",rc);
	//	goto probe_done;
	}
	printk(KERN_ERR"mt9t112 model_id = 0x%x\n", model_id);

   //we will init sensor in mt9p111_sensor_init.++
   	ISmt9p111_init=1;
	rc = mt9p111_sensor_init_probe(info);
	ISmt9p111_init=0;
	if (rc < 0)
	{
		printk(KERN_ERR "mt9p111_msg: in mt9p111_sensor_probe() mt9p111_sensor_init_probe failed rc=%d.\n",rc);
		goto probe_done;
	}
   //we will init sensor in mt9p111_sensor_init.--
#if 1
	hrtimer_init(&mt9p111_flashled_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mt9p111_flashled_timer.function = mt9p111_flashled_timer_func;
//	mt9p111_thread_id = kernel_thread(mt9p111_flashled_off_thread, NULL, CLONE_FS | CLONE_FILES); //Henry remove flash led thread in probe.
	
//	mt9p111_reg_init_thread_id = kernel_thread(mt9p111_reg_init_thread, NULL, CLONE_FS | CLONE_FILES);	
#endif
/*
	rc=mt9p111_sensor_init(info);
	if (rc < 0) {
		printk(KERN_ERR "*********%s: mt9p111_sensor_init failed!! <%d>", __func__, rc);
		goto probe_done;
	}
*/


	//Div6-PT2-MM-CH-Camera_VFS-00+[	
	#ifdef MT9P111_USE_VFS
	rc = create_attributes(mt9p111_client);
	if (rc < 0) {
		dev_err(&mt9p111_client->dev, "%s: create attributes failed!! <%d>", __func__, rc);
		goto probe_done;
	}
	#endif /* MT9P111_USE_VFS */
	//Div6-PT2-MM-CH-Camera_VFS-00+]
	s->s_init = mt9p111_sensor_init;
	s->s_release = mt9p111_sensor_release;
	s->s_config  = mt9p111_sensor_config;

	//Henry added for supporting standby mode.++
	mtp9111_sensor_standby(0);
	gpio_tlmm_config(GPIO_CFG(15, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_DISABLE);
	printk("%s:initial successfully,  mt9p111 disable mclk and go to standby mode.\n", __func__);
	//Henry added for supporting standby mode.--
	dev_attr_mt9p111regset.attr.name = "MT9P111_SET_REGISTER";	
	rc = device_create_file(&mt9p111_client->dev, &dev_attr_mt9p111regset);
	if (rc < 0) {
		printk( "%s: Create mt9p111regset attribute \"MT9P111_SET_REGISTER\" failed!! <%d>", __func__, rc);
		
	}
	return 0;

probe_done:
	mt9p111_power_off();
	printk("prob failed:%s %s:%d:rc=%d\n", __FILE__, __func__, __LINE__,rc);
	return rc;
}

static int __mt9p111_probe(struct platform_device *pdev)
{
	printk(KERN_WARNING "mt9p111_msg: in __mt9p111_probe() because name of msm_camera_mt9p111 match.\n");
	return msm_camera_drv_start(pdev, mt9p111_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9p111_probe,
	.driver = {
		.name = "msm_camera_mt9p111",
		.owner = THIS_MODULE,
	},
};

static int __init mt9p111_init(void)
{
	//Div2-SW6-MM-SC-NewSensor-01+{
	int iHwid = 0;
	iHwid = FIH_READ_ORIG_HWID_FROM_SMEM();
	if (1)//(iHwid >= FIH_900_4I3_PR1 && iHwid <= FIH_900_4I3_PR5)
	{
		return platform_driver_register(&msm_camera_driver);
	}
	else
	{
		printk(KERN_WARNING "mt9p111_msg: This is SA401 device, so not load mt9p111 sensor, just return.\n");
		return 0;
	}
	//Div2-SW6-MM-SC-NewSensor-01+}
}

module_init(mt9p111_init);

