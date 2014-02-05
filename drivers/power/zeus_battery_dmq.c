/* drivers/power/goldfish_battery.c
 *
 * Power supply driver for the goldfish emulator
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

#include <linux/workqueue.h>  /* Modify to create a new work queue for BT play MP3 smoothly*/
#include "../../arch/arm/mach-msm/proc_comm.h"

#define T_FIH
#ifdef T_FIH	//+T_FIH
#include <asm/gpio.h>

#include <linux/wakelock.h> /* Add wake lock to avoid incompleted update battery information*/
#include<linux/cmdbgapi.h> /* Add for debug mask*/
#include <linux/eventlog.h>   /* FIHTDC, May adds event log, 2010.11.30*/

#define GPIO_CHR_DET 39		// Input power-good (USB port/adapter present indicator) pin
#define GPIO_CHR_FLT 32		// Over-voltage fault flag
#define GPIO_CHR_EN 33		//Charging enable pin
#define CHR_1A 26	//Charging current 1A/500mA
#define USBSET 123    //current 1 or1/5

#define FLAG_BATTERY_POLLING
#define FLAG_CHARGER_DETECT
#endif  //-T_FIH

/*Add misc device*/
#ifdef CONFIG_FIH_FXX
#include <linux/miscdevice.h>
#include <asm/ioctl.h>
#include <asm/fcntl.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>

/*Add misc device ioctl command*/
#define FTMBATTERY_MAGIC 'f'
#define FTMBATTERY_CAP		_IO(FTMBATTERY_MAGIC, 0)
#define FTMBATTERY_VOL		_IO(FTMBATTERY_MAGIC, 1)
#define FTMBATTERY_TEP		_IO(FTMBATTERY_MAGIC, 2)
#define FTMBATTERY_CUR		_IO(FTMBATTERY_MAGIC, 3)
#define FTMBATTERY_AVC		_IO(FTMBATTERY_MAGIC, 4)
#define FTMBATTERY_STA		_IO(FTMBATTERY_MAGIC, 5)
#define FTMBATTERY_BID		_IO(FTMBATTERY_MAGIC, 6)
#define FTMBATTERY_PRE_VOL		_IO(FTMBATTERY_MAGIC, 7)
#define FTMBATTERY_VBAT		_IO(FTMBATTERY_MAGIC, 8)

/* +++ New charging temperature protection for Greco hardware */
#define GET_TEMPERATURE_FROM_BATTERY_THERMAL    1
#if GET_TEMPERATURE_FROM_BATTERY_THERMAL
#define BATTERY_TEMP_LOW_LIMIT  0   //Low temperature limit for charging
#define BATTERY_TEMP_HIGH_LIMIT 45  //High temperature limit for charging
#define BATTERY_TEMP_COOL_DOWN_FROM_EMGCY   50  //Battery temp. lower than 50 degree leaves cool down mode.
#define BATTERY_TEMP_SHUTDOWN_AP    55  //Battery temperature threshod to shut down AP to cool down device.
#define BATTERY_TEMP_EMGCY_CALL_ONLY    58  //Battery temperature threshod for emergency call only.

//FIHTDC, MayLi adds for DQE~DP2 board thermal protection, 2010.12.22 {++
#define MSM_TEMP_COOL_DOWN_FROM_EMGCY   55//60  //Main board temperature lower than 60 degree leaves cool down mode.
#define MSM_TEMP_SHUTDOWN_AP    60//70  //Main board temperature threshod to shut down AP to cool down device.
#define MSM_TEMP_EMGCY_CALL_ONLY    63//75  //Main board temperature threshod for emergency call only.
//FIHTDC, MayLi adds for DQE~DP2 board thermal protection, 2010.12.22 {++
#define RECHARGE_TEMP_OFFSET    1   //Start to recharge if the temperature is > (0+1) or < (40-1)
#define BATTERY_THERMAL_TEMP_OFFSET 1   //When charging, the temp. read from thermistor is higher then actual battery temp. 
#define OLD_BATTERY_RESISTOR_TEMP   (-21)   //Old battery has 100k resistor => -21 degree
#define OLD_BATTERY_RESISTOR_TEMP_TOL   (5) //Tolerance for temperature read from adc.

/* We may misjudge the new battery as old battery due to the resistor and adc tolerance. */
/* If the temperature of battery thermistor becomes -11 degree, it means the battery is new battery. */
/* Then we should use the battery thermistor to do temperature protection. */
#define MAY_BE_BATTERY_THERMAL_TEMP     (-11)
#endif
/* --- New charging temperature protection for Greco hardware */
#endif
//FIHTDC, Michael add for battery test mode, 2010.12.29+++
#define BATTERY_TEST_MODE_ENABLE	1
//FIHTDC, Michael add for battery test mode, 2010.12.29---

enum {
	CHARGER_STATE_UNKNOWN,		
	CHARGER_STATE_CHARGING,		
	CHARGER_STATE_DISCHARGING,	
	CHARGER_STATE_NOT_CHARGING,	
	CHARGER_STATE_FULL,
	CHARGER_STATE_LOW_POWER,
};

typedef struct _VOLT_TO_PERCENT
{
    u16 dwVolt;
    u16 dwPercent;
} VOLT_TO_PERCENT;


//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 {++
//DPD, DMP, DP2
static VOLT_TO_PERCENT g_Volt2PercentModeP[11] =
{
    { 3450, 0},	   // empty,    Rx Table
    { 3599, 10},    // level 1
    { 3654, 20},    // level 2
    { 3674, 30},    // level 3
    { 3708, 40},    // level 4
    { 3737, 50},    // level 5
    { 3782, 60},    // level 6
    { 3839, 70},    // level 7
    { 3902, 80},    // level 8
    { 3995, 90},    // level 9
    { 4100, 100},   // full
};

//DQE, DMQ, DQD
static VOLT_TO_PERCENT g_Volt2PercentModeQ[11] =
{
    { 3450, 0},	   // empty,    Rx Table
    { 3562, 10},    // level 1
    { 3616, 20},    // level 2
    { 3651, 30},    // level 3
    { 3678, 40},    // level 4
    { 3711, 50},    // level 5
    { 3756, 60},    // level 6
    { 3818, 70},    // level 7
    { 3887, 80},    // level 8
    { 3973, 90},    // level 9
    { 4100, 100},   // full
};

//Default table.
static VOLT_TO_PERCENT g_Volt2PercentModeD[11] =
{
    { 3450, 0},	   // empty,    Rx Table 
    { 3556, 10},    // level 1
    { 3642, 20},    // level 2
    { 3685, 30},    // level 3
    { 3707, 40},    // level 4
    { 3740, 50},    // level 5
    { 3792, 60},    // level 6
    { 3862, 70},    // level 7
    { 3945, 80},    // level 8
    { 4026, 90},    // lebel 9
    { 4100, 100},   // full
};
//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 --}

static int g_charging_state = CHARGER_STATE_NOT_CHARGING;
static int g_charging_state_last = CHARGER_STATE_NOT_CHARGING;

struct workqueue_struct *zeus_batt_wq; /*Modify to create a new work queue for BT play MP3 smoothly*/


struct goldfish_battery_data {
	uint32_t reg_base;
	int irq;
	spinlock_t lock;
	struct power_supply battery;
	struct wake_lock battery_wakelock; /* [FXX_CR], Add wake lock to avoid incompleted update battery information*/
	struct	work_struct SetCurrentWork;

};

static struct goldfish_battery_data *data;
bool wakelock_flag;

unsigned int Battery_HWID;

/* temporary variable used between goldfish_battery_probe() and goldfish_battery_open() */
static struct goldfish_battery_data *battery_data;

struct battery_info{
	unsigned pre_batt_val;
	unsigned new_batt_val;
};

#if GET_TEMPERATURE_FROM_BATTERY_THERMAL
#define TRUE    1
#define FALSE   0
static int g_use_battery_thermal = FALSE;
static int g_cool_down_mode = FALSE;
#endif

//FIHTDC, Add for new HWID, check WCDMA or CMDA, MayLi, 2010.11.08 {++
static int g_orig_hw_id;
#define BAND_WCDMA 1
#define BAND_CDMA 0
int g_Band=BAND_WCDMA;  /*WCDMA:1, CDMA:0*/
int g_Product; /*DQE, DMQ, DQD:1, DPD,DMP, DP2:0*/
int g_PR_num;
//FIHTDC, Add for new HWID, check WCDMA or CMDA, MayLi, 2010.11.08 --}

signed msm_termal;  /* For save the correct format of thermal value with negative value*/

bool over_temper;

static int g_data_number;  /*Average array count number*/
static unsigned g_batt_data[10]; /*Average array*/

static struct battery_info	PMIC_batt;
static struct power_supply * g_ps_battery;

// Not update battery information when charger state changes {++
bool charger_state_change;//flag for charging state change
int system_time_second;//get the system time while charger plug in/out
int time_duration; //the time duration while charger plug in/out
// Not update battery information when charger state changes --}

/* [FXX_CR], Avoid weird value*/
int weird_count;//count for value goes down while charging

//Charging full protection {++
/* Modify for battery_full_falg abnormal changed*/
int battery_full_flag;//battery full flag
bool battery_full_flag2;//for show 100%
int battery_full_count;//for first time reach 100% to update time start point 
int battery_full_start_time;//battery full start time
int battery_full_time_duration;//battery full time duration
//Charging full protection --}

//Add for avoid too frequently update battery {++
int pre_time_cycle;
int pre_val;
//Add for avoid too frequently update battery --}

bool suspend_update_flag;  /* Add for update battery information more accuracy in suspend mode*/

int zeus_bat_val;  /*percentage*/
int VBAT_from_SMEM;
int battery_low;
bool weird_retry_flag;  /* Add a retry mechanism to prevent the sudden high value*/

//Add for update battery information more accuracy only for battery update in suspend mode {++
int pre_suspend_time;
int suspend_time_duration;
//Add for update battery information more accuracy only for battery update in suspend mode --}
int suspend_update_sample_gate;

//New charging temperature protection scenario {++
int charging_bat_vol;
//New charging temperature protection scenario --}
int zeus_bat_vol;

bool Charging_GPIO_Init = 0; /*MayLi adds for charging current gpio init, 2010.10.28*/
int Charging_current;
int Battery_ID_status=1;
// Add for debug mask {++
static int battery_debug_mask;
//FIHTDC, Michael add for battery test mode, 2010.12.29+++
#if BATTERY_TEST_MODE_ENABLE
int test_temp=0;
int test_percent=0;
int test_status=0;
int test_health=0;
int Battery_test_mode=0;
int temp_test_mode=0;
int percent_test_mode=0;
int status_test_mode=0;
int health_test_mode=0;
#endif
//FIHTDC, Michael add for battery test mode, 2010.12.29---

module_param_named(debug_mask, battery_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);
// Add for debug mask --}

// FIHTDC, Add for getting battery id, 2010.11.08 +++{
#define W1_BQ2024
#ifdef W1_BQ2024
spinlock_t bq2024_spin_lock; /* global dsp lock */
#define W1_SKIP_ROM  0xCC
#define W1_READ_ROM  0x33
#define W1_READ_MEMORY_FIELD_CRC  0xF0
#define W1_READ_MEMORY_PAGE_CRC  0xC3
#define W1_READ_EPROM_STATUS  0xAA
int GPIO_W1_BQ2024=16;

//static BQ2024_EPROM BatteryID_Table[3] =
u8 BatteryID_Table[3][128]=
{
	//BP6X_Maxell, for DQE, DMQ, DQD
	{
		0xF8, 0x01, 0x98, 0x00, 0x00, 0x19, 0x14, 0x03, 0xE8, 0x23, 0x5A, 0x0E, 0x47, 0x95, 0xFF, 0x01, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

		0x7C, 0x67, 0x8F, 0x56, 0xD8, 0x00, 0x31, 0xFF, 0x5A, 0x40, 0xFF, 0x05, 0x01, 0x06, 0x0F, 0x00, 
		0x68, 0x01, 0xB0, 0x48, 0x9C, 0xA3, 0x11, 0x3C, 0x5F, 0x63, 0x67, 0x44, 0x52, 0x58, 0x5C, 0x1C,
		
		0xD7, 0xB0, 0x4A, 0x9C, 0xA4, 0x11, 0x3F, 0x6B, 0x6F, 0x72, 0x3F, 0x5F, 0x64, 0x68, 0x1B, 0x38, 
		0xC6, 0x00, 0xA4, 0xB3, 0x3A, 0xB6, 0x86, 0xD3, 0x97, 0x65, 0x6F, 0x7E, 0x90, 0x1C, 0x37, 0x64,

		0x43, 0x4F, 0x50, 0x52, 0x32, 0x30, 0x31, 0x30, 0x4D, 0x4F, 0x54, 0x4F, 0x52, 0x4F, 0x4C, 0x41, 
		0x20, 0x45, 0x2E, 0x50, 0x20, 0x43, 0x48, 0x41, 0x52, 0x47, 0x45, 0x20, 0x4F, 0x4E, 0x4C, 0x59
	},

	//BP6X_LGC, for DQE, DMQ, DQD
	{
		0xFA, 0x01, 0x98, 0x00, 0x00, 0x19, 0x14, 0x03, 0xE8, 0x23, 0x5A, 0x0E, 0x45, 0x95, 0xFF, 0x01, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			
		0x81, 0x76, 0x8B, 0x56, 0xD8, 0x00, 0x31, 0xFF, 0x5A, 0x40, 0xFF, 0x05, 0x01, 0x06, 0x0F, 0x00, 
		0x68, 0x01, 0xB0, 0x48, 0x9C, 0xA3, 0x12, 0x3C, 0x5F, 0x63, 0x67, 0x44, 0x4B, 0x51, 0x59, 0x1C,
		
		0x0D, 0xB0, 0x4A, 0x9D, 0xA4, 0x12, 0x3F, 0x6A, 0x6F, 0x72, 0x3F, 0x5B, 0x64, 0x69, 0x1B, 0x33, 
		0xC6, 0x1C, 0xA9, 0xB6, 0x3A, 0x95, 0xA3, 0xB8, 0x97, 0x54, 0x5F, 0x76, 0x8B, 0x1C, 0x35, 0x60,
		
		0x43, 0x4F, 0x50, 0x52, 0x32, 0x30, 0x30, 0x39, 0x4D, 0x4F, 0x54, 0x4F, 0x52, 0x4F, 0x4C, 0x41, 
		0x20, 0x45, 0x2E, 0x50, 0x20, 0x43, 0x48, 0x41, 0x52, 0x47, 0x45, 0x20, 0x4F, 0x4E, 0x4C, 0x59
	},

	//BP5X_Maxell, for DPD,DMP, DP2
	{
		0xF4, 0x01, 0x98, 0x00, 0x00, 0x19, 0x14, 0x03, 0xE8, 0x23, 0x5A, 0x0E, 0x4B, 0x95, 0xFF, 0x01, 
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

		0x66, 0x81, 0x96, 0x56, 0xD8, 0x00, 0x31, 0xFF, 0x5A, 0x40, 0xFF, 0x05, 0x01, 0x06, 0x0F, 0x00, 
		0x68, 0x01, 0xB0, 0x48, 0x9D, 0xA5, 0x0C, 0x3C, 0x5F, 0x63, 0x67, 0x44, 0x4E, 0x55, 0x5A, 0x1C,

		0xB0, 0xB0, 0x4A, 0x9E, 0xA6, 0x0C, 0x3F, 0x6B, 0x6F, 0x72, 0x3F, 0x5E, 0x62, 0x66, 0x1B, 0x46, 
		0xC6, 0x00, 0xA8, 0xB6, 0x35, 0xD6, 0x86, 0xD2, 0x97, 0x67, 0x70, 0x7E, 0x8E, 0x1C, 0x44, 0x54,

		0x43, 0x4F, 0x50, 0x52, 0x32, 0x30, 0x31, 0x30, 0x4D, 0x4F, 0x54, 0x4F, 0x52, 0x4F, 0x4C, 0x41, 
		0x20, 0x45, 0x2E, 0x50, 0x20, 0x43, 0x48, 0x41, 0x52, 0x47, 0x45, 0x20, 0x4F, 0x4E, 0x4C, 0x59
	},
};


void w1_bq2024_write_1(u8 byte)
{
	//Do not add debug message here.
	
	if(byte)
	{
		gpio_set_value(GPIO_W1_BQ2024, 0);
		udelay(5); /*this should be 1us ~ 15us*/
		gpio_set_value(GPIO_W1_BQ2024, 1);
		udelay(70);
	}
	else
	{
		gpio_set_value(GPIO_W1_BQ2024, 0);
		udelay(70);
		gpio_set_value(GPIO_W1_BQ2024, 1);
		udelay(5);
	}
}


void w1_bq2024_write_8(u8 byte)
{
	unsigned long flags;
	int i;
	
	spin_lock_irqsave(&bq2024_spin_lock, flags);
	for (i=0; i<8; i++) 
	{
		w1_bq2024_write_1((byte >> i) & 0x1);
	}
	spin_unlock_irqrestore(&bq2024_spin_lock, flags);
}


u8 w1_bq2024_read_1(void)
{
	int ret;
	
	//Do not add debug message here.
	
	gpio_set_value(GPIO_W1_BQ2024, 0);
	udelay(5); /*this should be 1us ~ 15us*/
	
	//Read the 1 bit from slave
	if (gpio_direction_input(GPIO_W1_BQ2024))
		printk(KERN_ERR "%s: gpio_direction_input %d fails!!!\r\n", __func__, GPIO_W1_BQ2024);
	udelay(5);   /*slave will pull low 17us~60us*/
	ret = gpio_get_value(GPIO_W1_BQ2024) ? 1 : 0;
	udelay(55); /*Do not change this value !!!!!!!, slave needs some time.*/
	
	gpio_direction_output(GPIO_W1_BQ2024, 1);
	udelay(5); /*this should be > 1us or 5us*/
	return ret;
}


u8 w1_bq2024_read_8(void)
{
	unsigned long flags;
	int i;
	u8 data=0;

	spin_lock_irqsave(&bq2024_spin_lock, flags);
	for (i=0; i<8; i++) 
	{
		data |= ((w1_bq2024_read_1() & 0x1) << i);
	}
	spin_unlock_irqrestore(&bq2024_spin_lock, flags);
	return data;
}


static u8 w1_crc8_table[] = {
	0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
	157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
	35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
	190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
	70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
	219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
	101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
	248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
	140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
	17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
	175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
	50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
	202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
	87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
	233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
	116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
};

u8 Check_CRC8(u8 * data, int len)
{
	u8 bq2024_crc=0;
	u8 crc = 0;
	
	//read back bq2024 calculation crc
	bq2024_crc = w1_bq2024_read_8();
	
	while (len--)
		crc = w1_crc8_table[crc ^ *data++];

	if(crc != bq2024_crc)
	{
		printk(KERN_INFO "%s: crc = 0x%x, bq2024_crc=0x%x\n", __func__, crc, bq2024_crc);
		return -1;
	}
	
	return 0;
}


/**
 * Issues a reset bus sequence.

 * @return     0=Device present, 1=No device present or error
 */
int w1_reset_bq2024(void)
{
	unsigned long flags;
	int ret;
	
	spin_lock_irqsave(&bq2024_spin_lock, flags);
	
	//Pull a reset signal
	gpio_set_value(GPIO_W1_BQ2024, 0);
	udelay(480); /*this should be more than 480us*/
	gpio_set_value(GPIO_W1_BQ2024, 1);
	udelay(15);  /*this should be 15us~60us*/

	//Read whether slave has a presence pulse
	if (gpio_direction_input(GPIO_W1_BQ2024))
		printk(KERN_ERR "w1_reset_bq2024: gpio_direction_input %d fails!!!\r\n", GPIO_W1_BQ2024);
	udelay(60);   /*slave will pull low 60us~240us*/

	ret = gpio_get_value(GPIO_W1_BQ2024) ? 1 : 0;
	udelay(480);  /*Do not small this value*/
	
	gpio_direction_output(GPIO_W1_BQ2024, 1);
	
	spin_unlock_irqrestore(&bq2024_spin_lock, flags);
	return ret;
}


/* Resets the bus and then selects the slave by sending either a skip rom
 * or a rom match.
 * The w1 master lock must be held.

 * @return 	0=success, anything else=error
 */
#define SKIP_ROM
int w1_reset_select_slave(void)
{
#ifndef SKIP_ROM
	u8 data[7] = {0};
	int i;
#endif

	//printk(KERN_INFO "%s\n", __func__);
	if (w1_reset_bq2024())
		return -1;

#ifdef SKIP_ROM
	//We just have one slave, bq2024, so use skip rom
	w1_bq2024_write_8(W1_SKIP_ROM);
#else  /*read rom*/
	w1_bq2024_write_8(W1_READ_ROM);
	for(i=0; i<7; i++)
	{
		data[i] = w1_bq2024_read_8();
		printk(KERN_ERR "%s: data[%d]=0x%x\r\n", __func__,i,data[i] );
	}
	if(Check_CRC8(&data[0], 7) != 0)
	{
		printk(KERN_ERR "%s: Check ROM CRC fail !!\r\n", __func__);
		return -1;
	}
#endif

	return 0;
}


/* Return: 0 = success, other value = fail */
int w1_bq2024_read_page(u8 PNum, u8* PData)
{
	u8 MemCmdAddr[3];
	u8 TempPData[32];
	int count = 0;
	int i;

	//printk(KERN_ERR "%s: %d +++\r\n", __func__, PNum);
	
	// 1. Reset bq2024. If fail, retry 5 times.
	while(count < 5)
	{
		if(w1_reset_select_slave() != 0)
		{
			count ++;
			printk(KERN_ERR "%s: w1_reset_select_slave fail %d times!!\r\n", __func__, count);

			if(count == 5)
				return -1;
		}
		else
			break;
	}

	// 2. Send Memory command, address low, and address high
	MemCmdAddr[0] = W1_READ_MEMORY_PAGE_CRC;
	MemCmdAddr[1] = 0x20*PNum;
	MemCmdAddr[2] = 0;
	for(i=0; i<3; i++)
		w1_bq2024_write_8(MemCmdAddr[i]);
	if(Check_CRC8(&MemCmdAddr[0], 3) != 0 /*Check_Mem_Addr_CRC(W1_READ_MEMORY_PAGE_CRC, 0x20*PNum, 0x00) != 0*/)
	{
		printk(KERN_ERR "%s: Check MemCmd_Addr CRC fail !!\r\n", __func__);
		return -1;
	}
	
	// 3. Read page data.	
	for(i=0; i<32; i++)
	{
		TempPData[i] = w1_bq2024_read_8();
	}
	if(Check_CRC8(&TempPData[0], 32) != 0 /*Check_Page_CRC(&TempPData[0], 32) != 0*/)
	{
		printk(KERN_ERR "%s: Check_Page_CRC fail !!\r\n", __func__);
		return -1;
	}

	memcpy(PData, &TempPData[0], 32);

	//test +
	#if 0
	for(i=0; i<32; i++)
		printk(KERN_ERR "%s: TempPData[%d] = 0x%x \r\n", __func__, i, TempPData[i]);
	#endif
	//test -
	
	//printk(KERN_ERR "%s: %d ---\r\n", __func__, PNum);
	return 0;
}


int CheckBatteryID(void)
{
	u8 EPROM[128];
	int i;

	//Do not check battery id in DMQ_PR1, just return success.
	if(g_Product == Project_DMQ && g_PR_num == Phase_PR1)
	{
		printk(KERN_ERR "%s: Do not check DMQ_PR1 BatID. \r\n", __func__);
		return 0;
	}

	// Clear EPROM buffer
	memset(&EPROM[0], 0, 128);
	
	// Read back page0~page3
	for(i=0; i<4; i++)
	{
		if(w1_bq2024_read_page(i, &EPROM[i*32]) !=0 )
		{
			printk(KERN_ERR "%s: read page%d fail !!\r\n", __func__, i);
			if(w1_bq2024_read_page(i, &EPROM[i*32]) !=0 )
			{
				printk(KERN_ERR "%s: read page%d fail again !!\r\n", __func__, i);
				return -1;
			}
			
		}
	}

	#if 0
	//Test +
	for(i=0; i<128; i++)
		printk(KERN_ERR "%s: page[%d][%d] = 0x%x\r\n", __func__, (i/32),  (i%32), EPROM[i]);
	//Test -
	#endif

	//DQE, DMQ, DQD have two kinds of battery ID, BatteryID_Table[0] and BatteryID_Table[1]
	if(g_Product == Project_DQE ||
		g_Product == Project_DMQ ||
		g_Product == Project_DQD)
	{
		// compare battery id table.
		if( memcmp(&BatteryID_Table[0][0], &EPROM[0], 128) != 0 
		&& memcmp(&BatteryID_Table[1][0], &EPROM[0], 128) != 0)
		{
			printk(KERN_ERR "%s: compare DQE, DMQ, DQD ID fail !!\r\n", __func__);
			return -1;
		}
	}
	//DPD,DMP, DP2 have one battery ID, BatteryID_Table[2]
	else if(g_Product == Project_DPD ||
			g_Product == Project_DMP ||
			g_Product == Project_DP2)
	{
		if(memcmp(&BatteryID_Table[2][0], &EPROM[0], 128) != 0)
		{
			printk(KERN_ERR "%s: compare DPD,DMP, DP2 ID fail !!\r\n", __func__);
			return -1;
		}
	}
	
	return 0;
}

#endif
// FIHTDC, Add for getting battery id, 2010.11.08 ---}

static int Average_Voltage(void)
{
	int i, j;
	unsigned temp_sum=0;
	g_batt_data[g_data_number%10]=PMIC_batt.new_batt_val;
	g_data_number++;
	
	if(suspend_update_flag&&(suspend_update_sample_gate==1))
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1, "%s(): update three sample =%d\r\n",
																__func__, PMIC_batt.new_batt_val);
		for(i=0;i<=1;i++)
		{
			g_batt_data[g_data_number%10]=PMIC_batt.new_batt_val;
			g_data_number++;
		}

	}
	else if((suspend_update_flag&&(suspend_update_sample_gate==2))||battery_low)
	{	
		/*Add for update battery information more accuracy in suspend mode*/
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1, "%s(): update five sample =%d\r\n",
																__func__, PMIC_batt.new_batt_val);

		for(i=0;i<=3;i++)
		{
			g_batt_data[g_data_number%10]=PMIC_batt.new_batt_val;
			g_data_number++;
		}
	}
	suspend_update_flag=false;
	for(j=0;j<10;j++)
	{
		temp_sum+=g_batt_data[j];
	}
	PMIC_batt.new_batt_val=temp_sum/10;
	return	PMIC_batt.new_batt_val;
}


static int	GetPMIC_MSM_TERMAL(void)
{
	int adc_read_id;
   	int t;
	//FIHTDC, Michael add for battery test mode, 2010.12.29+++
	#if BATTERY_TEST_MODE_ENABLE
	if(temp_test_mode==1)
	{
		return test_temp ;
	}
	#endif
	//FIHTDC, Michael add for battery test mode, 2010.12.29---

	#if GET_TEMPERATURE_FROM_BATTERY_THERMAL    //Enable it after the algorithm for two thermistor is ready
   	if(g_Product>=Project_DQE && g_Product<=Project_DP2
		&& !(g_Product==Project_DMQ &&  g_PR_num==Phase_PR1)) /*Do not use battery thermal when DMQ_PR1, HW issue. */
    	{
    		if (g_use_battery_thermal)
    		{
    			if(g_Band == BAND_WCDMA)
        			adc_read_id = 16; //Use the ADC_BATT_THERM_DEGC channel of 7227 modem
        		else
				adc_read_id = 18;	
    		}
    		else
    		{
    			if(g_Band == BAND_WCDMA)
        			adc_read_id = 16; //Use the ADC_BATT_THERM_DEGC channel of 7227 modem
        		else
				adc_read_id = 18;
				
        			t = proc_comm_read_adc(&adc_read_id);
        			if ((t >= MAY_BE_BATTERY_THERMAL_TEMP)&&(t!=90))
        			{
            				g_use_battery_thermal = TRUE;
            				return t;
        			}
        			else
        			{
        				//Here we need to check WCDMA or CMDA, not implement now.
        				if(g_Band == BAND_WCDMA)
            				adc_read_id = 22;
				else
					adc_read_id = 24;
        			}
    		}
    	}
	else
	#endif  /*Enable it after the algorithm for two thermistor is ready*/
	{
		//Here we need to check WCDMA or CMDA, not implement now.
		if(g_Band == BAND_WCDMA)
			adc_read_id = 22;
		else
			adc_read_id = 24;
	}

	t = proc_comm_read_adc(&adc_read_id);

	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G2, "%s(): adc_read_id=%d, temp=%d\r\n", __func__, adc_read_id, t);

	return t;
}

static int	GetPMICBatteryInfo(void)
{
	int adc_read_id, i;
	unsigned battery_sum=0;
	int smpnum=3;
	int countB;
	int weird_count_number;
	int iC, Vbat_threshold; /* Create different thresholds in diffrerent voltage range to prevent voltage drop issue*/

	unsigned ModemBatVol=0;  /*May adds for test, 2010.11.09*/
	
	if(suspend_update_flag)
		weird_count_number=3;
	else
		weird_count_number=7;
	
	time_duration=get_seconds()-system_time_second;
	if(charger_state_change&&(time_duration<3)&&(PMIC_batt.pre_batt_val!=0))
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s(): PMIC_batt.pre_batt_val=%d\r\n",
																__func__, PMIC_batt.pre_batt_val);
		return PMIC_batt.pre_batt_val;
	}
	else
	{
		charger_state_change=false;
	}

	// Check adc read channel
	if(g_Band == BAND_WCDMA)
		adc_read_id=11;
	else
		adc_read_id=13;
	//spin_lock_irqsave(&data->lock, flags);//Michael add to avoid read wrong VBAT

	//if charger exists, disable charging before reading voltage
	if(!gpio_get_value(GPIO_CHR_DET) /*&&!over_temper&&!battery_full_flag&&Battery_ID_status*/)
	{
		charging_bat_vol=proc_comm_read_adc(&adc_read_id);
		if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1))) /*Do not disable charging. DMQ_PR1 HW issue*/
		{
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s(): Disable charging before read vol +\r\n", __func__);
			gpio_set_value(GPIO_CHR_EN,1); /*Disable chager before read voltage */
		}
		msleep(2000);
	}

	//Read VBat from PMIC 3 times and get average value.
	for(countB=0;countB<smpnum;countB++)
	{
		//battery_sum+=proc_comm_read_adc(&adc_read_id);
		ModemBatVol = proc_comm_read_adc(&adc_read_id);
		battery_sum += ModemBatVol;
		//printk(KERN_ERR "%s(): adc_read_id=%d, battery_sum=%d \r\n", __func__, adc_read_id, battery_sum);
	}
	//spin_unlock_irqrestore(&data->lock, flags);//Michael add to avoid read wrong VBAT

	//enable charing after reading voltage.
	if(!gpio_get_value(GPIO_CHR_DET)&&!over_temper&&!battery_full_flag&&Battery_ID_status)
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s(): Enable charging after read vol -\r\n", __func__);
		gpio_set_value(GPIO_CHR_EN,0);//Enable chager after read voltage
	}

	PMIC_batt.new_batt_val=battery_sum/smpnum;
	VBAT_from_SMEM=PMIC_batt.new_batt_val;
	
	/* [FXX_CR],update battery sample data when battery capacity is low*/
	if(PMIC_batt.new_batt_val<=3600 && gpio_get_value(GPIO_CHR_DET))
		battery_low=true;
	else
		battery_low=false;

	//Set default voltage when first boot
	if(PMIC_batt.pre_batt_val==0)
	{
		for(i=0;i<10;i++)
		{
			g_batt_data[i]=PMIC_batt.new_batt_val;
		}
		PMIC_batt.pre_batt_val=PMIC_batt.new_batt_val;
	}
	
	//When charging plug in
	if(!gpio_get_value(GPIO_CHR_DET)) 
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[CHG_IN] ADC_VBATT=%d, pre_batt=%d \r\n",
														__func__, PMIC_batt.new_batt_val,PMIC_batt.pre_batt_val);

		/* Add a retry mechanism to prevent the sudden high value*/
		//Avoid sudden high value  value
		if((PMIC_batt.pre_batt_val>3600)&&!weird_retry_flag&&(PMIC_batt.new_batt_val>(PMIC_batt.pre_batt_val+150)))
		{
			weird_retry_flag=true;
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[CHG_IN] weird_retry_flag, diff=%d\r\n",
																__func__, PMIC_batt.new_batt_val-PMIC_batt.pre_batt_val);
			return PMIC_batt.pre_batt_val;
		}
		else
			weird_retry_flag=false;
		
		//Voltage only can go up
		/*Avoid weird value*/
		if(PMIC_batt.new_batt_val<PMIC_batt.pre_batt_val)
		{
			weird_count++;
			if(weird_count<weird_count_number)
			{
				suspend_update_flag=false;
				fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[CHG_IN] weird_count=%d \r\n",
																	__func__, weird_count);
				return PMIC_batt.pre_batt_val;
			}
		}
		else
		{
			weird_count=0;
		}

		//Get average voltage
		PMIC_batt.new_batt_val=Average_Voltage();
		PMIC_batt.pre_batt_val=PMIC_batt.new_batt_val;
		//fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "<GetPMICBatteryInfo>[CHG_IN]EXIT pre_batt=%d \r\n", PMIC_batt.pre_batt_val);
	}
	else//Charging unplug
	{
		if(PMIC_batt.pre_batt_val>3900)
			suspend_update_sample_gate=0;
		else if((PMIC_batt.pre_batt_val<=3900)&&(PMIC_batt.pre_batt_val>3700))
			suspend_update_sample_gate=1;
		else
			suspend_update_sample_gate=2;
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[CHG_OUT] ADC_VBATT=%d, pre_batt=%d \r\n",
															__func__, PMIC_batt.new_batt_val,PMIC_batt.pre_batt_val);

		//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 {++
		/*Create different thresholds in diffrerent voltage range to prevent voltage drop issue*/
		//get threshold value
		for(iC=0;iC<11;iC++)
		{
			if(g_Product == Project_DQE ||
				g_Product == Project_DMQ||
				g_Product == Project_DQD)
			{
				if(PMIC_batt.pre_batt_val <= g_Volt2PercentModeQ[iC].dwVolt)
           				break;
			}
			else if(g_Product == Project_DPD || 
					g_Product == Project_DMP||
					g_Product == Project_DP2)
			{
				if(PMIC_batt.pre_batt_val <= g_Volt2PercentModeP[iC].dwVolt)
           				break;
			}
			else
			{
				if(PMIC_batt.pre_batt_val <= g_Volt2PercentModeD[iC].dwVolt)
           				break;
			}
		}
		
		if(g_Product == Project_DQE || g_Product == Project_DMQ|| g_Product == Project_DQD)
			Vbat_threshold=(g_Volt2PercentModeQ[iC].dwVolt -g_Volt2PercentModeQ[iC-1].dwVolt);
		else if(g_Product == Project_DPD || g_Product == Project_DMP|| g_Product == Project_DP2)
			Vbat_threshold=(g_Volt2PercentModeP[iC].dwVolt -g_Volt2PercentModeP[iC-1].dwVolt);
		else
			Vbat_threshold=(g_Volt2PercentModeD[iC].dwVolt -g_Volt2PercentModeD[iC-1].dwVolt);
		//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 --}
		
		if(iC==2)
			Vbat_threshold=Vbat_threshold*6/5;
		else if(iC==1)
			Vbat_threshold=Vbat_threshold*3/2;
		else if(iC==0)
			Vbat_threshold=0;
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[CHG_OUT] Vbat_threshold=%d\r\n",
																__func__, Vbat_threshold);

		//Voltage only can go down
		if(PMIC_batt.new_batt_val>PMIC_batt.pre_batt_val)
		{
			suspend_update_flag=false;
			return PMIC_batt.pre_batt_val;
		}
		
		//Filter weird value
		else if((PMIC_batt.pre_batt_val -PMIC_batt.new_batt_val)>300)
		{
			suspend_update_flag=false;
			return PMIC_batt.pre_batt_val;
		}

		/* Use different thresholds in diffrerent voltage range to prevent voltage drop issue in suspend mode*/
		else if((PMIC_batt.pre_batt_val -PMIC_batt.new_batt_val)>Vbat_threshold)
		{
		/* FIH, Michael Kao, 2009/12/01{ */
			PMIC_batt.new_batt_val=PMIC_batt.pre_batt_val-Vbat_threshold;
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s():[voltage drop] new_voltage=%d \r\n",
																__func__, PMIC_batt.new_batt_val);
		}

		//Get average voltage
		PMIC_batt.new_batt_val=Average_Voltage();
		PMIC_batt.pre_batt_val=PMIC_batt.new_batt_val;
	}

	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s(): new_batt_val=%d\r\n", __func__, PMIC_batt.new_batt_val);

	return PMIC_batt.new_batt_val;			
}

static int	ChangeToVoltPercentage(unsigned Vbat)
{
	int Volt_pec=0;
	int iC;
	//FIHTDC, Michael add for battery test mode, 2010.12.29+++
	#if BATTERY_TEST_MODE_ENABLE
	if(percent_test_mode==1)
	{
		return test_percent;
	}
	#endif
	//FIHTDC, Michael add for battery test mode, 2010.12.29---

	//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 {++
	/* Modofy for using different profile for different battery*/
	for(iC=0;iC<11;iC++)
	{
		if(g_Product == Project_DQE || 
			g_Product == Project_DMQ||
			//Michael add for AI1 HWID+++
			g_Product == Project_DQD||
			g_Product == Project_AI1D||
			g_Product == Project_AI1S)
			//Michael add for AI1 HWID---
		{
			if(Vbat <= g_Volt2PercentModeQ[iC].dwVolt)
           			break;
		}
		else if(g_Product == Project_DPD || 
				g_Product == Project_DMP||
				g_Product == Project_DP2)
		{
			if(Vbat <= g_Volt2PercentModeP[iC].dwVolt)
           			break;
		}
		else 
		{
			if(Vbat <= g_Volt2PercentModeD[iC].dwVolt)
           			break;
		}
	}
	
	if(iC==0)
		Volt_pec=0;
	else if(iC==11)
		Volt_pec=100;
	else if((iC>0)&&(iC<11))
	{
		if(g_Product == Project_DQE || 
			g_Product == Project_DMQ||
			//Michael add for AI1 HWID+++
			g_Product == Project_DQD||
			g_Product == Project_AI1D||
			g_Product == Project_AI1S)
			//Michael add for AI1 HWID---
		{
			Volt_pec=g_Volt2PercentModeQ[iC-1].dwPercent + 
				( Vbat -g_Volt2PercentModeQ[iC-1].dwVolt) * ( g_Volt2PercentModeQ[iC].dwPercent -g_Volt2PercentModeQ[iC-1].dwPercent)/( g_Volt2PercentModeQ[iC].dwVolt -g_Volt2PercentModeQ[iC-1].dwVolt);
		}
		else if(g_Product == Project_DPD || 
				g_Product == Project_DMP||
				g_Product == Project_DP2)
		{
			Volt_pec=g_Volt2PercentModeP[iC-1].dwPercent + 
				( Vbat -g_Volt2PercentModeP[iC-1].dwVolt) * ( g_Volt2PercentModeP[iC].dwPercent -g_Volt2PercentModeP[iC-1].dwPercent)/( g_Volt2PercentModeP[iC].dwVolt -g_Volt2PercentModeP[iC-1].dwVolt);
		}
		else
		{
			Volt_pec=g_Volt2PercentModeD[iC-1].dwPercent + 
				( Vbat -g_Volt2PercentModeD[iC-1].dwVolt) * ( g_Volt2PercentModeD[iC].dwPercent -g_Volt2PercentModeD[iC-1].dwPercent)/( g_Volt2PercentModeD[iC].dwVolt -g_Volt2PercentModeD[iC-1].dwVolt);
		}
	}
	//FIHTDC, MayLi updates DQE~DP2 OCV table, 2010.12.22 --}
	
	/* [FXX_CR], Improve charging full protection scenario*/
	#if 0
	if(Volt_pec==100)
	{
		if(battery_full_count==0)
			battery_full_start_time=get_seconds();
		battery_full_count++;
	}
	else
		battery_full_count=0;
	#endif
	
	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G1,  "%s(): Vbat=%d, Volt_pec=%d \r\n", __func__, Vbat, Volt_pec);
	//return 50;//paul, Temp workaround//
	
	/* Divide battery level (0~100) into 10 pieces*/
	if (Volt_pec == 0)      
    		return 0;
    	else if (Volt_pec <= 5)
    		return 5;
    	else if (Volt_pec <= 11)
    		return 10;
    	else if (Volt_pec <= 16)
    		return 15;
    	else if (Volt_pec <= 22)
    		return 20;
    	else if (Volt_pec <= 33)
    		return 30;
    	else if (Volt_pec <= 44)
    		return 40;
    	else if (Volt_pec <= 54)
    		return 50;
    	else if (Volt_pec <= 64)
    		return 60;
    	else if (Volt_pec <= 74)
    		return 70;
    	else if (Volt_pec <= 84)
    		return 80;
    	else if (Volt_pec <= 94)
    		return 90;
    	else if (Volt_pec <= 100)
    		return 100;
    	else
    		return -1;
		
	return Volt_pec;
}

//MayLi, For update battery information per 10 minutes in suspend mode 2010.10.27 {+++
#if 1 /*Remove TCA6507 notify*/
void Battery_power_supply_change(void)
{
	/* Add for update battery information more accuracy only for battery update in suspend mode*/
	suspend_time_duration=get_seconds()-pre_suspend_time;
	if(suspend_time_duration>300)
	{
		suspend_update_flag=true;
		g_ps_battery->changed=1;
		queue_work(zeus_batt_wq, &g_ps_battery->changed_work);
		/* Add wake lock to avoid incompleted update battery information*/
		wake_lock(&data->battery_wakelock);
		wakelock_flag=true;
		pre_suspend_time=get_seconds();
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): suspend_update_flag=%d,  wake_lock start\n", __func__, suspend_update_flag);
	}
	else
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): suspend_time_duration=%d\n", __func__, suspend_time_duration);
}
EXPORT_SYMBOL(Battery_power_supply_change);
/* } FIH, Michael Kao, 2009/06/08 */
#endif
//MayLi, For update battery information per 10 minutes in suspend mode 2010.10.27 ---}


//MayLi, adds for set charging current, 2010.10.28 {+
static void Set_Charging_Current_work(struct work_struct *work)
{
	//May adds for set charging current, 2010.10.28 {+
	if(!Charging_GPIO_Init)
	{
		printk(KERN_INFO "%s(), init charging gpio !!\r\n", __func__);
		gpio_request(CHR_1A, "chr_1a");
		gpio_request(USBSET, "usbset");
		gpio_request(GPIO_CHR_EN, "gpio_chr_en");

		gpio_direction_output(CHR_1A, 0);
		gpio_direction_output(USBSET, 0);
		if(!(g_Product==Project_DMQ &&  g_PR_num==Phase_PR1))
			gpio_direction_output(GPIO_CHR_EN, 1);
		else
			gpio_direction_output(GPIO_CHR_EN, 0);
		Charging_GPIO_Init = 1;
	}
	//May adds for set charging current, 2010.10.28 -}
	
	// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 +++{
	if(Charging_current != 0)
	{
		if(Battery_ID_status == 0)
		{
			printk(KERN_INFO "%s(), Battery ID is not right, do NOT enable charging !!!\r\n", __func__);
			eventlog("[BAT] BatID Wrong, dis-charge\n");
			return;
		}
	}
	// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 ---}
	
	switch(Charging_current)
	{
		case 0:
			gpio_set_value(CHR_1A, 0);
			gpio_set_value(USBSET, 0);
			if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1)))
				gpio_set_value(GPIO_CHR_EN, 1);
			break;

		case 500:
			gpio_set_value(CHR_1A, 0);
			gpio_set_value(USBSET, 1);
			if(!over_temper && !battery_full_flag) /*Please enable after ARM9 ready*/
				gpio_set_value(GPIO_CHR_EN, 0);
			break;

		case 1000:
			gpio_set_value(CHR_1A, 1);
			gpio_set_value(USBSET, 1);
			if(!over_temper && !battery_full_flag) /*Please enable after ARM9 ready*/
				gpio_set_value(GPIO_CHR_EN, 0);
			break;

		default:
			printk(KERN_ERR "%s(): Do not support current: %d \r\n", __func__, Charging_current);
			break;
		}
}

void SetChargingCurrent(int iCurrent)
{
	printk(KERN_INFO "%s(), iCurrent=%d \r\n", __func__, iCurrent);
	eventlog("[BAT] set current=%d\n", iCurrent);
	
	Charging_current=iCurrent;
	queue_work(zeus_batt_wq, &data->SetCurrentWork);
}
EXPORT_SYMBOL(SetChargingCurrent);
//MayLi, adds for set charging current, 2010.10.28 -}


static int goldfish_battery_get_property(struct power_supply *psy,
				 enum power_supply_property psp,
				 union power_supply_propval *val)
{
	int ret = 0;
	unsigned	vbatt;
	int time_cycle=0;
	static bool bFirstboot=1;  /*FIHTDC, MayLi adds for eventlog, 2010.12.01 */
	
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++
		#if BATTERY_TEST_MODE_ENABLE
		if(status_test_mode==1)
		{
			val->intval= test_status;
			break;
		}
		#endif
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++
		//FIHTDC, Michael modify for fix charging status wrong when device is over temp, 2010.12.29+++
		if((g_charging_state==CHARGER_STATE_CHARGING)&&(over_temper||((health_test_mode==1)&&(test_health!=POWER_SUPPLY_HEALTH_GOOD))))
		{
			val->intval=CHARGER_STATE_DISCHARGING;
			
		}else if (g_charging_state != CHARGER_STATE_LOW_POWER)
		//FIHTDC, Michael modify for fix charging status wrong when device is over temp, 2010.12.29---
			val->intval = g_charging_state;
		else 
			val->intval = CHARGER_STATE_NOT_CHARGING;

		//FIHTDC, MayLi adds for wrong battery id, 2010.12.01 {++
		if(Battery_ID_status == 0)
			val->intval = CHARGER_STATE_UNKNOWN;   /*battery icon will be like "?" */
		//FIHTDC, MayLi adds for wrong battery id, 2010.12.01 --}
		
		break;
		
	case POWER_SUPPLY_PROP_HEALTH:
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++
		#if BATTERY_TEST_MODE_ENABLE
		if(health_test_mode==1)
		{
			val->intval= test_health;
			break;
		}
		#endif
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++

		#if GET_TEMPERATURE_FROM_BATTERY_THERMAL
		//DQE, DMQ, DQD, DPD,DMP, DP2 have battery thermal pin
		if(g_Product >= Project_DQE &&  g_Product <= Project_DP2)
		{
			if (g_use_battery_thermal)  /*Battery thermistor case*/
			{
				if (g_cool_down_mode)
				{
					if (msm_termal < BATTERY_TEMP_COOL_DOWN_FROM_EMGCY)
					{
						val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
						g_cool_down_mode = FALSE;
					}
					else
						val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_EMGCY_CALL_ONLY;
				}
				else
				{
					if (msm_termal > BATTERY_TEMP_EMGCY_CALL_ONLY)
					{
						g_cool_down_mode = TRUE;    //Enter cool-down mode by customer's request
						val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_EMGCY_CALL_ONLY;
					}
					else if (msm_termal > BATTERY_TEMP_SHUTDOWN_AP && msm_termal <= BATTERY_TEMP_EMGCY_CALL_ONLY)
						val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_SHUTDOWN_AP;
					else if (over_temper)   // (BATTERY_TEMP_HIGH_LIMIT + BATTERY_THERMAL_TEMP_OFFSET) <= msm_termal <= BATTERY_TEMP_LOW_LIMIT
						val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
					else
						val->intval = POWER_SUPPLY_HEALTH_GOOD;
				}
			}
            		else  /*MSM thermistor case*/
            		{   
           			if (g_cool_down_mode)
            			{
                			if (msm_termal < MSM_TEMP_COOL_DOWN_FROM_EMGCY)
                			{
                    				g_cool_down_mode = FALSE;
                    				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
                			}
                			else
                    				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_EMGCY_CALL_ONLY;
           			}
            			else
            			{
            		
               			if (msm_termal > MSM_TEMP_EMGCY_CALL_ONLY)
                			{
                    				g_cool_down_mode = TRUE;
                    				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_EMGCY_CALL_ONLY;
                			}
                			else if (msm_termal > MSM_TEMP_SHUTDOWN_AP && msm_termal <= MSM_TEMP_EMGCY_CALL_ONLY)
                    				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT_SHUTDOWN_AP;
                			else if (over_temper)
        					val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
        				else
        					val->intval = POWER_SUPPLY_HEALTH_GOOD;
           			}
            		}
            		printk("battery_thermal=%d health=%d cool-down=%d msm_termal=%d, battery_id=%d\n", 
						g_use_battery_thermal, val->intval, g_cool_down_mode,msm_termal, Battery_ID_status);
		}
        	else  /*FAD does not have battery thermal pin*/
		#endif
    		{
			if(over_temper)
				val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			else
				val->intval = POWER_SUPPLY_HEALTH_GOOD;

			printk("2: battery_thermal=%d health=%d cool-down=%d msm_termal=%d, battery_id=%d\n", 
						g_use_battery_thermal, val->intval, g_cool_down_mode,msm_termal, Battery_ID_status);
    		}
		break;
		
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = 1;
		break;
		
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
		
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = msm_termal*10;
		break;
		
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = zeus_bat_vol*1000;  /* [GREE.B-383], Due to Eclaire voltage_now is in microvolts, not millivolts*/
		break;
		
	case POWER_SUPPLY_PROP_CAPACITY:

		// Add for avoid too frequently update battery {+
		time_cycle=get_seconds()-pre_time_cycle;
		pre_time_cycle=get_seconds();
		if(time_cycle<5&&pre_val!=0)
		{
			val->intval=pre_val;
		}  // Add for avoid too frequently update battery -}
		else
		{
			// 1. Get battery voltage 
			/* [FXX_CR], add a retry mechanism to prevent the sudden high value*/
			vbatt=GetPMICBatteryInfo();
			if(weird_retry_flag)
			{
				weird_retry_flag=false;
				vbatt=GetPMICBatteryInfo();
				if(weird_retry_flag)
				{
					fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "POWER_SUPPLY_PROP_CAPACITY : weird_retry_flag2\n");
					suspend_update_flag=false;
					vbatt=GetPMICBatteryInfo();
				}
			}

			// 2. Change voltage to percentage.
			val->intval=ChangeToVoltPercentage(vbatt);
			zeus_bat_val=val->intval;
			zeus_bat_vol=vbatt;

			// 3. Get temperature from PMIC
			msm_termal=GetPMIC_MSM_TERMAL();
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "POWER_SUPPLY_PROP_CAPACITY : msm_termal=%d\n",msm_termal);

			// 4. Check Battery full when charging
			if(!gpio_get_value(GPIO_CHR_DET))
			{
				fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "POWER_SUPPLY_PROP_CAPACITY : charging_bat_vol=%d\n",
																									charging_bat_vol);
				if(vbatt>=4120)
				{
					if(battery_full_count==0)
						battery_full_start_time=get_seconds();
					battery_full_count++;
				}
				else
					battery_full_count=0;
					
				if((battery_full_flag==0)&&(vbatt>=4120))
				{
					battery_full_time_duration=get_seconds()-battery_full_start_time;
					fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "POWER_SUPPLY_PROP_CAPACITY : BAT_Full time_du=%d\n",battery_full_time_duration);

					if(battery_full_time_duration>=5400)
					{
						printk("%s: BatFull=%d, discharge \n", __func__, vbatt);
						eventlog("BatFull:%d, dis-charge\n", vbatt);
						
						battery_full_flag=1;
						
						if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1))) /*Do not disable charging. DMQ_PR1 HW issue*/
							gpio_set_value(GPIO_CHR_EN,1);//Disable chager, please enable after ARM9 ready.
					}

				}
				else if((battery_full_flag==1)&&(vbatt<=4120))
				{
					printk("%s: BatFull=%d, re-charge \n", __func__, vbatt);
					eventlog("BatFull:%d, re-charge\n", vbatt);

					battery_full_flag=0;
					battery_full_flag2=true;
					
					if(!over_temper && Battery_ID_status)
						gpio_set_value(GPIO_CHR_EN,0);//Enable charger
					else
					{
						printk("%s: Not enable. BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d \n", __func__, 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
						eventlog("Not-En:BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d\n", 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
					}
				}

				if(battery_full_flag2&&(val->intval<100)&&(val->intval>90))
					val->intval=100;
			}
			else  /* without charger */
			{
				battery_full_flag=0;
				battery_full_flag2=false;
			}
			
			// 5. Check over temperature
		#if GET_TEMPERATURE_FROM_BATTERY_THERMAL	
			//FIHTDC, MayLi modify for DQE~DP2 board thermal threshold, 2010.12.22 {++
			if(g_Product >= Project_DQE &&  g_Product <= Project_DP2)
			{
				if(g_use_battery_thermal)  //use battery thermal threshold to check
				{
					if (!over_temper
						&& (msm_termal <= (BATTERY_TEMP_LOW_LIMIT-BATTERY_THERMAL_TEMP_OFFSET)
						|| msm_termal >= (BATTERY_TEMP_HIGH_LIMIT + BATTERY_THERMAL_TEMP_OFFSET)))
					{	
						printk("%s: Battery OTP=%d, discharge \n", __func__, msm_termal);
						eventlog("B_OTP:%d, dis-charge\n", msm_termal);

						over_temper=true;
						
						if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1))) /*Do not disable charging. DMQ_PR1 HW issue*/
						{
							if(!gpio_get_value(GPIO_CHR_DET))
								gpio_set_value(GPIO_CHR_EN,1);  //Disable chager, please enable when ARM9 ready.
						}
					}

					if (over_temper
						&&(msm_termal >= (BATTERY_TEMP_LOW_LIMIT + RECHARGE_TEMP_OFFSET))
						&&(msm_termal <= (BATTERY_TEMP_HIGH_LIMIT - RECHARGE_TEMP_OFFSET)))
					{
						printk("%s: Battery OTP=%d, re-charge \n", __func__, msm_termal);
						eventlog("B_OTP:%d, re-charge\n", msm_termal);

						over_temper=false;	
						
						if(Battery_ID_status && !battery_full_flag)
						{
							if(!gpio_get_value(GPIO_CHR_DET))
								gpio_set_value(GPIO_CHR_EN,0); //Enable chager
						}
						else
						{
							printk("%s: Not enable. BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d \n", __func__, 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
							eventlog("Not-En:BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d\n", 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
						}
					}
				}
				else  //use DQE~DP2 board thermal threshold to check
				{
					if (!over_temper && (msm_termal <= 3 ||msm_termal >= 50))
					{	
						printk("%s: MSM OTP=%d, discharge \n", __func__, msm_termal);
						eventlog("M_OTP:%d, dis-charge\n", msm_termal);

						over_temper=true;
						
						//if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1))) /*Do not disable charging. DMQ_PR1 HW issue*/
						{
							if(!gpio_get_value(GPIO_CHR_DET))
								gpio_set_value(GPIO_CHR_EN,1);  //Disable chager, please enable when ARM9 ready.
						}
					}
					
					if (over_temper && (msm_termal >= 8 && msm_termal <= 45))
					{
						printk("%s: MSM OTP=%d, re-charge \n", __func__, msm_termal);
						eventlog("M_OTP:%d, re-charge\n", msm_termal);

						over_temper=false;	
						
						if(Battery_ID_status && !battery_full_flag)
						{
							if(!gpio_get_value(GPIO_CHR_DET))
								gpio_set_value(GPIO_CHR_EN,0); //Enable chager
						}
						else
						{
							printk("%s: Not enable. BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d \n", __func__, 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
							eventlog("Not-En:BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d\n", 
							Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
						}
					}
				}
                	}
			//FIHTDC, MayLi modify for DQE~DP2 board thermal threshold, 2010.12.22 --}
			else
		#endif  //end of GET_TEMPERATURE_FROM_BATTERY_THERMAL
			{
				if(!over_temper&&((msm_termal<=10)||(msm_termal>=60)))
				{
					printk("%s: MSM OTP=%d, discharge \n", __func__, msm_termal);
					eventlog("M_OTP:%d, dis-charge\n", msm_termal);

					over_temper=true;
					
					//if(!((g_Product==Project_DMQ)&&(g_PR_num==Phase_PR1)))
					if(!gpio_get_value(GPIO_CHR_DET))
						gpio_set_value(GPIO_CHR_EN,1);  //Disable chager, please enable when ARM9 ready.
				}

				if(over_temper&&((msm_termal>=15)&&(msm_termal<=55)))
				{
					printk("%s: MSM OTP1 , re-charge \n", __func__);
					eventlog("M_OTP:%d, re-charge\n", msm_termal);

					over_temper=false;

					if(Battery_ID_status && !battery_full_flag)
					{
						if(!gpio_get_value(GPIO_CHR_DET))
							gpio_set_value(GPIO_CHR_EN,0);//Enable chager
					}
					else
					{
						printk("%s: Not enable. BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d \n", __func__, 
						Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
						eventlog("Not-En:BatID=%d, BatFull=%d, BatFull2=%d, over_t=%d\n", 
						Battery_ID_status, battery_full_flag, battery_full_flag2, over_temper);
					}
				}
			}   
		}

		// Update charging state
		if ((val->intval == 100)&&(g_charging_state == CHARGER_STATE_CHARGING))
		{
			g_charging_state = CHARGER_STATE_FULL;
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): set the charging status to full\n", __func__);
		}
		else if (g_charging_state == CHARGER_STATE_FULL) 
		{
			if (val->intval < 98) 
			{
				g_charging_state = CHARGER_STATE_CHARGING;
				fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): set the charging status to charging\n", __func__);
			}
		}
		else if (g_charging_state == CHARGER_STATE_NOT_CHARGING)
		{
			if (val->intval < 15) 
			{
				fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): set the charging status to low power\n", __func__);
				g_charging_state = CHARGER_STATE_LOW_POWER;
			}
		}

		// Add wake lock to avoid incompleted update battery information {++
		if(wakelock_flag)
		{
			wake_unlock(&data->battery_wakelock);
			wakelock_flag=false;
			fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "POWER_SUPPLY_PROP_CAPACITY : wake_unlock");
		}
		// Add wake lock to avoid incompleted update battery information --}


		printk("batt : %d - %d, EN = %d, 1A = %d, SET = %d\n", 
			val->intval, g_charging_state, gpio_get_value(GPIO_CHR_EN), gpio_get_value(CHR_1A), gpio_get_value(USBSET));
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++
		#if BATTERY_TEST_MODE_ENABLE
		if(Battery_test_mode)
			printk("Test :temp = %d, percent = %d, status = %d, health = %d", 
				test_temp, test_percent, test_status, test_health);
		#endif
		//FIHTDC, Michael add for battery test mode, 2010.12.29+++

		//FIHTDC, MayLi adds for eventlog, 2010.12.01 {++
		if(bFirstboot)
		{
			bFirstboot = 0;
			pre_val = zeus_bat_val;
			eventlog("[BAT] VBat=%d, BatP=%d\n", zeus_bat_vol, zeus_bat_val);
		}

		if(pre_val != zeus_bat_val)
		{
			eventlog("[BAT] VBat=%d, BatP=%d\n", zeus_bat_vol, zeus_bat_val);
		}
		//FIHTDC, MayLi adds for eventlog, 2010.12.01 --}
	
            	pre_val=val->intval;
            	g_charging_state_last = g_charging_state;
		break;
		
	default:
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): psp(%d)\n", __func__, psp);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static enum power_supply_property goldfish_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};


#ifdef T_FIH	///+T_FIH
#ifdef FLAG_BATTERY_POLLING
static struct timer_list polling_timer;
#define BATTERY_POLLING_TIMER  60000 /* Update battery information per 60 seconds*/
static void polling_timer_func(unsigned long unused)
{
	g_ps_battery->changed=1;
	queue_work(zeus_batt_wq, &g_ps_battery->changed_work);
	
	/* Add wake lock to avoid incompleted update battery information*/
	wake_lock(&data->battery_wakelock);
	wakelock_flag=true;
	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s(): wake_lock start\n", __func__);

	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));
}
#endif	// FLAG_BATTERY_POLLING

#ifdef FLAG_CHARGER_DETECT
static irqreturn_t chgdet_irqhandler(int irq, void *dev_id)
{
	g_charging_state = (gpio_get_value(GPIO_CHR_DET)) ? CHARGER_STATE_NOT_CHARGING : CHARGER_STATE_CHARGING;

	/*Not update battery information when charger state changes*/
	charger_state_change=true;
	system_time_second=get_seconds();

	g_ps_battery->changed=1;
	queue_work(zeus_batt_wq, &g_ps_battery->changed_work);  /*Modify to create a new work queue for BT play MP3 smoothly*/
	
	/* Add wake lock to avoid incompleted update battery information*/
	wake_lock(&data->battery_wakelock);
	wakelock_flag=true;
	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "chgdet_irqhandler : wake_lock start\n");

	//write logs
	if(g_charging_state == CHARGER_STATE_NOT_CHARGING)
	{
		printk(KERN_ERR "%s(): Charger Plug OUT!!!\n", __func__);
		eventlog("[BAT] CHR OUT\n");
		SetChargingCurrent(0);
	}
	else
	{
		printk(KERN_ERR "%s(): Charger Plug IN!!!\n", __func__);
		eventlog("[BAT] CHR IN\n");

		// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 +++{
		//if(g_Product >= Project_DQE && g_Product <= Project_DP2)
		if(g_Product == Project_DPD ||
			g_Product == Project_DMQ ||
			g_Product == Project_DQE ||
			g_Product == Project_DP2 ||
			g_Product == Project_DMP ||
			g_Product == Project_DQD)
		{	
			if(CheckBatteryID() != 0)
			{
				//g_charging_state=CHARGER_STATE_NOT_CHARGING;//Michael add to show not charging when Battery ID is wrong
				Battery_ID_status=0;
				printk(KERN_INFO "%s(), Battery ID is not right !!!\r\n", __func__);
			}
			else
				Battery_ID_status=1;
		}
		// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 ---}
	}
	
	return IRQ_HANDLED;
}
#endif	// FLAG_CHARGER_DETECT
#endif	// T_FIH	///-T_FIH


#ifdef CONFIG_FIH_FXX
/*Add misc device ioctl command functions*/
/*******************File control function******************/
// devfs
static int Zeus_battery_miscdev_open( struct inode * inode, struct file * file )
{
	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_open\n" );
	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "zeus_battery's device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}

static int Zeus_battery_miscdev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd2, unsigned long arg )
{
    	int ret = 0;
	int data=0;
    	//uint8_t BatteryID;
    	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl\n" );
    	#if 1
    	if(copy_from_user(&data, (int __user*)arg, sizeof(int)))
    	{
       	 	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Get user-space data error\n");
        		return -1;
    	}
    	#endif
    
    	if(cmd2 == FTMBATTERY_VOL)
    	{
		ret = zeus_bat_vol;	
        		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl: FTMBATTERY_VOL=%d",zeus_bat_vol);
    	}
	else if(cmd2 ==FTMBATTERY_PRE_VOL)
	{
		ret =PMIC_batt.pre_batt_val;
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl: FTMBATTERY_PRE_VOL=%d",PMIC_batt.pre_batt_val);
	}
	else if(cmd2 == FTMBATTERY_VBAT)
	{
		ret=VBAT_from_SMEM;
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl: FTMBATTERY_VBAT=%d",VBAT_from_SMEM);
	}
	else if(cmd2 == FTMBATTERY_BID)
	{
       	 	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl: FTMBATTERY_BID");
	}
	else if(cmd2==FTMBATTERY_STA)
	{
		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_ioctl: FTMBATTERY_STA\r\n");
	}
    	else
    	{
        		fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "[%s:%d]Unknow ioctl cmd", __func__, __LINE__);
        		ret = -1;
    	}
    	return ret;
}


static int Zeus_battery_miscdev_release( struct inode * inode, struct file * filp )
{
	fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G3,  "Zeus_battery_miscdev_release\n");
	return 0;
}


static const struct file_operations Zeus_battery_miscdev_fops = {
	.open = Zeus_battery_miscdev_open,
	.ioctl = Zeus_battery_miscdev_ioctl,
	.release = Zeus_battery_miscdev_release,
};


static struct miscdevice Zeus_battery_miscdev = {
 	.minor = MISC_DYNAMIC_MINOR,
	.name = "ftmbattery",
	.fops = &Zeus_battery_miscdev_fops,
};
#endif
//FIHTDC, Michael add for battery test mode, 2010.12.29+++
#if BATTERY_TEST_MODE_ENABLE
static ssize_t Battery_set_value(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int battery_set_value=0;
	sscanf(buf, "%d\n", &battery_set_value);
	printk(KERN_INFO "%s: %d\n", __func__, battery_set_value);

	if(Battery_test_mode==1)//set percentage
	{
		test_percent=battery_set_value;
		percent_test_mode=1;
	}
	else if(Battery_test_mode==2)//set temperature
	{
		test_temp=battery_set_value;
		temp_test_mode=1;
	}
	else if(Battery_test_mode==3)//set status
	{
		test_status=battery_set_value;
		status_test_mode=1;
	}
	else if(Battery_test_mode==4)//set health
	{
		test_health=battery_set_value;
		health_test_mode=1;
	}
 	return count;
}

DEVICE_ATTR(set_value, 0664, NULL, Battery_set_value);//Modify for CTS issue
static ssize_t Battery_set_mode(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int Battery_Mode_value=0;
	sscanf(buf, "%d\n", &Battery_Mode_value);
	printk(KERN_INFO "%s: %d\n", __func__, Battery_Mode_value);
	Battery_test_mode=Battery_Mode_value;
	if(Battery_test_mode==0)//normal mode
	{
		percent_test_mode=0;
		temp_test_mode=0;
		status_test_mode=0;
		health_test_mode=0;	
	}
 	return count;
}

static DEVICE_ATTR(battery_mode, 0664, NULL, Battery_set_mode);//Modify for CTS issue+++
#endif
//FIHTDC, Michael add for battery test mode, 2010.12.29---
static int goldfish_battery_probe(struct platform_device *pdev)
{
	int ret;
	//FIHTDC, Michael add for battery test mode, 2010.12.29+++
	#if BATTERY_TEST_MODE_ENABLE
	int rc;
	#endif
	//FIHTDC, Michael add for battery test mode, 2010.12.29---

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto err_data_alloc_failed;
	}
	spin_lock_init(&data->lock);  /* 好像沒用到 */
	
	// Add wake lock to avoid incompleted update battery information {++
	wake_lock_init(&data->battery_wakelock, WAKE_LOCK_SUSPEND, "zeus_battery");
	wakelock_flag=false;
	// Add wake lock to avoid incompleted update battery information --}

	data->battery.properties = goldfish_battery_props;
	data->battery.num_properties = ARRAY_SIZE(goldfish_battery_props);
	data->battery.get_property = goldfish_battery_get_property;
	data->battery.name = "battery";
	data->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	Battery_HWID = FIH_READ_HWID_FROM_SMEM();
	printk("%s(): Battery_HWID=0x%x\n", __func__,  Battery_HWID);
/*	data->ac.properties = goldfish_ac_props;
	data->ac.num_properties = ARRAY_SIZE(goldfish_ac_props);
	data->ac.get_property = goldfish_ac_get_property;
	data->ac.name = "ac";
	data->ac.type = POWER_SUPPLY_TYPE_MAINS;*/
	msm_termal=25;
	over_temper=false;

	/* Not update battery information when charger state changes*/
	charger_state_change=false;
	time_duration=0;

	/* Add for update battery information more accuracy only for battery update in suspend mode*/
	battery_full_flag=0;
	battery_full_count=0;
	battery_full_flag2=false;
	pre_val=0;
	pre_time_cycle=0;
	

	/* Add for update battery information more accuracy in suspend mode*/
	suspend_update_flag=false;
	
	battery_low=false;
	zeus_bat_val=0;
	VBAT_from_SMEM=0;
	
	weird_retry_flag=false;  /* Add a retry mechanism to prevent the sudden high value*/

	/*Add for update battery information more accuracy only for battery update in suspend mode*/
	pre_suspend_time=0;

	// Add for new charging temperature protection scenario {++
	charging_bat_vol=0;
	suspend_update_sample_gate=0;
	zeus_bat_vol=0;
	// Add for new charging temperature protection scenario --}
	Charging_current=0;

	// FIHTDC, Add for getting battery id, 2010.11.08 +++{
	spin_lock_init(&bq2024_spin_lock);
	// FIHTDC, Add for getting battery id, 2010.11.08 ---}

	g_orig_hw_id = FIH_READ_ORIG_HWID_FROM_SMEM();
	printk("%s(): g_orig_hw_id=0x%x\n", __func__,  g_orig_hw_id);

	eventlog("                                                            \n");
	eventlog("[BAT] HWID[0x%x] ===================== \n", g_orig_hw_id);

	//FIHTDC, Add for new HWID, MayLi, 2010.11.08 {++
	g_Product = g_orig_hw_id & 0xFF;
	g_PR_num = (g_orig_hw_id & 0xFF00)>>8;

	if(g_Product == Project_DMQ ||
		g_Product == Project_DQD ||
		g_Product == Project_DMP ||
		g_Product == Project_DP2 ||
		//Michael add for AI1 HWID+++
		g_Product == Project_FAD||
		g_Product == Project_AI1D||
		g_Product == Project_AI1S)
		//Michael add for AI1 HWID---
	{
		g_Band = BAND_WCDMA;
	}
	else if(((g_orig_hw_id & 0xFF) == Project_DQE) ||
			((g_orig_hw_id & 0xFF) == Project_DPD))
	{
		g_Band = BAND_CDMA;
	}
	//FIHTDC, Add for new HWID, MayLi, 2010.11.08 --}

#if GET_TEMPERATURE_FROM_BATTERY_THERMAL
	if(g_Product>=Project_DQE && g_Product<=Project_DP2 
		&& !(g_Product==Project_DMQ &&  g_PR_num==Phase_PR1)) /*Do not use battery thermal when DMQ_PR1, HW issue. */
	{
		int adc_read_id; //Use the ADC_BATT_THERM_DEGC channel of 7227 modem
		int battery_thermal = 0;

		if(g_Band == BAND_WCDMA)
			adc_read_id = 16;
		else
			adc_read_id = 18;

		printk("%s(): Battery adc_read_id %d\n", __func__, adc_read_id);
		battery_thermal = proc_comm_read_adc(&adc_read_id);
		printk("%s(): Battery battery_thermal %d\n", __func__, battery_thermal);

		if (battery_thermal >= (OLD_BATTERY_RESISTOR_TEMP - OLD_BATTERY_RESISTOR_TEMP_TOL) 
			&& battery_thermal <= (OLD_BATTERY_RESISTOR_TEMP + OLD_BATTERY_RESISTOR_TEMP_TOL))
		{
			g_use_battery_thermal = FALSE;
			printk("%s(): Warning! Maybe no battery thermistor!\n", __func__);
		}
		else
		{
			g_use_battery_thermal = TRUE;
			printk("%s(): Battery thermistor exists\n", __func__);
		}
	}
#endif
		
	ret = power_supply_register(&pdev->dev, &data->battery);
	if (ret)
		goto err_battery_failed;

	platform_set_drvdata(pdev, data);
	battery_data = data;
	//FIHTDC, Michael add for battery test mode, 2010.12.29+++
	#if BATTERY_TEST_MODE_ENABLE
	rc=device_create_file(&pdev->dev, &dev_attr_battery_mode);
	if (rc < 0)
	{
		printk("%s: Create attribute \"battery_mode\" failed!! <%d>", __func__, rc);
		goto err_battery_failed;
	}
	rc=device_create_file(&pdev->dev, &dev_attr_set_value);
	if (rc < 0)
	{
		printk("%s: Create attribute \"set_value\" failed!! <%d>", __func__, rc);
		goto err_battery_failed;
	}
	#endif
	//FIHTDC, Michael add for battery test mode, 2010.12.29---


	/* Modify to create a new work queue for BT play MP3 smoothly*/
	zeus_batt_wq=create_singlethread_workqueue("zeus_battery");
	if (!zeus_batt_wq) {
		printk("%s(): create workque failed \n", __func__);
		return -ENOMEM;
	}
	INIT_WORK(&data->SetCurrentWork, Set_Charging_Current_work);

#ifdef T_FIH	///+T_FIH
#ifdef FLAG_BATTERY_POLLING
	setup_timer(&polling_timer, polling_timer_func, 0);
	mod_timer(&polling_timer,
		  jiffies + msecs_to_jiffies(BATTERY_POLLING_TIMER));
	g_ps_battery = &(data->battery);
#endif	// FLAG_BATTERY_POLLING

	// FIHTDC, Add for getting battery id, 2010.11.08 +++{
	if(g_Product == Project_DPD)
		GPIO_W1_BQ2024 = 16;
	else if(g_Product == Project_DMQ ||
			g_Product == Project_DQE || 
			g_Product == Project_DP2 ||
			g_Product == Project_DMP||
			g_Product == Project_DQD)
		GPIO_W1_BQ2024 = 88;

	printk("%s(): GPIO_W1_BQ2024=%d !!!\n", __func__, GPIO_W1_BQ2024);

	if(g_Product == Project_DPD ||
		g_Product == Project_DMQ ||
		g_Product == Project_DQE || 
		g_Product == Project_DP2 ||
		g_Product == Project_DMP||
		g_Product == Project_DQD)
	{
		ret = gpio_request(GPIO_W1_BQ2024, "w1_bq2024");
		if (ret)
			printk(KERN_ERR "goldfish_battery_probe: gpio_request w1_bq2024 fails!!!\r\n");
		else
			gpio_direction_output(GPIO_W1_BQ2024, 1);
	}
	// FIHTDC, Add for getting battery id, 2010.11.08 ---}

#ifdef FLAG_CHARGER_DETECT
	//May adds for set charging current, 2010.10.28 {+
	if(!Charging_GPIO_Init)
	{
		gpio_request(CHR_1A, "chr_1a");
		gpio_request(USBSET, "usbset");
		gpio_request(GPIO_CHR_EN, "gpio_chr_en");
		
		gpio_direction_output(CHR_1A, 0);
		gpio_direction_output(USBSET, 0);
		
		if(!(g_Product==Project_DMQ &&  g_PR_num==Phase_PR1))
			gpio_direction_output(GPIO_CHR_EN, 1);
		else
			gpio_direction_output(GPIO_CHR_EN, 0);
		Charging_GPIO_Init = 1;
	}
	//May adds for set charging current, 2010.10.28 -}
	
	gpio_tlmm_config( GPIO_CFG(GPIO_CHR_DET, 0, GPIO_INPUT, GPIO_NO_PULL, GPIO_2MA ), GPIO_ENABLE );
	ret = gpio_request(GPIO_CHR_DET, "gpio_chg_irq");
	if (ret)
		printk("%s(): GPIO_CHR_DETIRQ init fails!!! \r\n", __func__);

	ret = gpio_direction_input(GPIO_CHR_DET);
	if (ret)
		printk("%s(): gpio_direction_input GPIO_CHR_DET fails!!!\n", __func__);
		
	ret = request_irq(MSM_GPIO_TO_INT(GPIO_CHR_DET), &chgdet_irqhandler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, pdev->name, NULL);
	if (ret)
		printk("%s(): request_irq GPIO_CHR_DET fails!!!\n", __func__);
	else
		enable_irq_wake(MSM_GPIO_TO_INT(GPIO_CHR_DET));
#endif	// FLAG_CHARGER_DETECT

	// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 +++{
	//if(g_Product >= Project_DQE && g_Product <= Project_DP2)
	if(g_Product == Project_DPD ||
		g_Product == Project_DMQ ||
		g_Product == Project_DQE ||
		g_Product == Project_DP2 ||
		g_Product == Project_DMP ||
		g_Product == Project_DQD)
	{	
		if(CheckBatteryID() != 0)
		{
			//g_charging_state=CHARGER_STATE_NOT_CHARGING;//Michael add to show not charging when Battery ID is wrong
			Battery_ID_status=0;
			printk(KERN_INFO "%s(), Battery ID is not right !!!\r\n", __func__);
			eventlog("BatID Wrong.\n");
		}
		else
			Battery_ID_status=1;
	}
	// FIHTDC, Add for checking battery ID before enable charging., 2010.11.08 ---}

#endif	// T_FIH	///-T_FIH

	return 0;

err_battery_failed:
	kfree(data);
err_data_alloc_failed:
	return ret;
}

static int goldfish_battery_remove(struct platform_device *pdev)
{
	struct goldfish_battery_data *data = platform_get_drvdata(pdev);

#ifdef T_FIH	///+T_FIH
#ifdef FLAG_CHARGER_DETECT
	free_irq(MSM_GPIO_TO_INT(GPIO_CHR_DET), NULL);
	gpio_free(GPIO_CHR_DET);
#endif	// FLAG_CHARGER_DETECT

#ifdef FLAG_BATTERY_POLLING
	del_timer_sync(&polling_timer);
#endif	// FLAG_BATTERY_POLLING
#endif	// T_FIH	///-T_FIH

	power_supply_unregister(&data->battery);
	///power_supply_unregister(&data->ac);

	free_irq(data->irq, data);
	kfree(data);
	battery_data = NULL;
	return 0;
}

//FIHTDC, Adds for update battery information when suspend, MayLi, 2010.10.27 {+
static int battery_resume(struct platform_device *pdev)
{
	printk(KERN_ERR "%s(): battery resume!\n", __func__);
	Battery_power_supply_change();
	return 0;
}

static int battery_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	printk(KERN_ERR "%s(): battery suspend!\n", __func__);
	//Battery_power_supply_change();
	return 0;
}
//FIHTDC, Adds for update battery information when suspend, MayLi, 2010.10.27 -}

static struct platform_driver goldfish_battery_device = {
	.probe		= goldfish_battery_probe,
	.remove		= goldfish_battery_remove,
	.suspend        = battery_suspend,
	.resume         = battery_resume,
	.driver = {
		.name = "goldfish-battery"
	}
};

static int __init goldfish_battery_init(void)
{
	int ret;
	ret = platform_driver_register(&goldfish_battery_device);
	if(ret)
	{
		printk(KERN_ERR "%s(): register battery device failed! \n", __func__);
		goto ERROR;
	}

	/* FIH, Michael Kao, 2010/01/03{ */
	/* [FXX_CR], add for debug mask*/
	battery_debug_mask = *(int*)BAT_DEBUG_MASK_OFFSET;
	/* FIH, Michael Kao, 2010/01/03{ */
    #ifdef CONFIG_FIH_FXX
        /*Use miscdev*/
        //register and allocate device, it would create an device node automatically.
        //use misc major number plus random minor number, and init device
        ret = misc_register(&Zeus_battery_miscdev);
        if (ret)
        {
        	printk(KERN_ERR "%s(): Register misc device failed! \n", __func__);
            //fih_printk(battery_debug_mask, FIH_DEBUG_ZONE_G0,  "%s: Register misc device failed.\n", __func__);
            misc_deregister(&Zeus_battery_miscdev);
        }        
    #endif
    
    ERROR:    
	return ret;
    
}

static void __exit goldfish_battery_exit(void)
{
	platform_driver_unregister(&goldfish_battery_device);

	#ifdef CONFIG_FIH_FXX
    	/*new label name for remove misc device*/
		misc_deregister(&Zeus_battery_miscdev);	
	#endif
}

module_init(goldfish_battery_init);
module_exit(goldfish_battery_exit);

MODULE_AUTHOR("Mike Lockwood lockwood@android.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery driver for the Goldfish emulator");
