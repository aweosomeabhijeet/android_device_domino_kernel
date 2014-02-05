/*
 *     sc668.c - SC668 LED EXPANDER for BACKLIGHT and LEDs CONTROLLER
 *
 *     Copyright (C) 2009 Nicole Weng <nicolecfweng@fihtdc.com>
 *     Copyright (C) 2008 FIH CO., Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <mach/sc668.h>
#include <mach/gpio.h>
//#include <linux/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>

//debug mask.
//#include <linux/cmdbgapi.h>


/*Add misc device*/
#include <linux/miscdevice.h>
#include <asm/ioctl.h>
#include <asm/fcntl.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>


#include <linux/cmdbgapi.h>
static uint32_t sc668_debug_mask = 0;
module_param_named(debug_mask, sc668_debug_mask, uint, 0644);

#define DRIVER_NAME	"sc668"
#define myTest


#if (0)
#define my_debug(fmt, ...) \
	printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#else
#define my_debug(fmt, ...) \
	fih_printk(sc668_debug_mask, FIH_DEBUG_ZONE_G0, pr_fmt(fmt), ##__VA_ARGS__)
#endif

//#define DBG(f, x...) my_debug(DRIVER_NAME " [%s()]: " f, __func__,## x)

#define DBG(f, x...) \
	my_debug("%s(): " f, __func__,## x)

//#define	GPIO_SC668_EN	30
struct sc668_driver_data {	
	struct i2c_client *sc668_i2c_client;	
	struct mutex sc668_lock;
	int enable_pin;
	int pwm_pin;
	int state;
	u8 LDO1_setting;
	u8 LDO2_setting;
	u8 LDO3_setting;
	u8 LDO4_setting;
	int LDO1_count;
	int LDO2_count;
	int LDO3_count;
	int LDO4_count;
};

static struct sc668_driver_data sc668_drvdata;
int ProductID;
int	PhaseID;

// a copy of the registers that exist within the SC668
static unsigned char registerShadow[] =
                                 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                                  0x00, 0x00, 0x00, 0x00, 0x00};

static const unsigned char registerUsableBits[] = 
                                 {0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 
                                  0x3F, 0x3F, 0x3F, 0x3F, 0x0F, 0x07, 
                                  0x0F, 0x0F, 0x1F, 0x3F, 0x3F, 0x3F, 
                                  0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
                                  
// -1: Invalid -- Do not read
// 0: Valid
// 1: Needs update
static char shadowState[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; 

/*************I2C functions*******************/
static int sc668_i2c_read_reg( unsigned char reg, unsigned char *data, unsigned char count)
{
	unsigned char tmp[10];
	
	if(10 < count)
		return -1;
	
	tmp[0] = reg;
	if(1 != i2c_master_send(sc668_drvdata.sc668_i2c_client, tmp, 1))
	{
		//DBG( " set REGISTER address error\r\n");
		printk(KERN_ERR "[%s:%d]set REGISTER address error\r\n", __func__, __LINE__);			
		return SC668_I2C_ERROR;
	}

	if(count != i2c_master_recv(sc668_drvdata.sc668_i2c_client, tmp, count))
	{
		//DBG(" read REGISTER content error\r\n");
		printk(KERN_ERR "[%s:%d]read REGISTER content error\r\n", __func__, __LINE__);		
		return SC668_I2C_ERROR;
	}

	strncpy(data, tmp, count);

	return SC668_SUCCESS;
}

static int sc668_write_reg( u8 reg, u8 value )
{	
	int ret = -1;
	ret = i2c_smbus_write_byte_data(sc668_drvdata.sc668_i2c_client, reg, value);
	//DBG("d:0x%x, reg:0x%x\n", value, reg);
	fih_printk(sc668_debug_mask, FIH_DEBUG_ZONE_G1, "[sc668_write_reg] d:0x%x, reg:0x%x\n", value, reg);
	if(ret < 0)
	{
		//DBG(" ret:%d, command:0x%x)\n", ret, reg );		
		printk(KERN_ERR "[%s:%d]ret:%d, command:0x%x)\n", __func__, __LINE__, ret, reg);	
		return SC668_I2C_ERROR;
	}

	return SC668_SUCCESS;
}


static unsigned char sc668_get_register(int regNum) {	
	unsigned char data;
	int ret = -1;	
	if (shadowState[regNum]) {
		// Update this register's shadow first		
		ret = sc668_i2c_read_reg( regNum, &data, 1);
		if(ret){
			//DBG("read 0x%x fail!!!", regNum);
			printk(KERN_ERR "[%s:%d]read 0x%x fail!!!\n", __func__, __LINE__, regNum);	
			shadowState[regNum] = -1;
			return registerShadow[regNum];
		}
		registerShadow[regNum] = data;
		shadowState[regNum] = 0;
		//DBG("regNum:0x%x, data:0x%x\n", regNum, registerShadow[regNum]);
	}	
	return registerShadow[regNum];
}

static int sc668_set_register(unsigned char regNum, unsigned char data, unsigned char mask) {	
	int ret=-1;
	//DBG("d:0x%x, reg:0x%x", data, regNum);
	fih_printk(sc668_debug_mask, FIH_DEBUG_ZONE_G1, "[sc668_set_register] d:0x%x, reg:0x%x", data, regNum);
	mutex_lock(&sc668_drvdata.sc668_lock);
	data = ((data & mask) + (sc668_get_register(regNum) & ~mask)) & registerUsableBits[regNum];	
	printk(" d:0x%x, reg:0x%x\n", data, regNum);
	ret = sc668_write_reg(regNum, data);
	if(ret!=SC668_SUCCESS)
	{
		 mutex_unlock(&sc668_drvdata.sc668_lock);
		 return ret;
	}
	registerShadow[regNum] = data;
	shadowState[regNum] = data; 
	mutex_unlock(&sc668_drvdata.sc668_lock);
    return SC668_SUCCESS;
}

/*!
The sc668_write_register function is the lowest level API provided. All functionality 
of the chip may be accessed through this single function which writes data to 
the specified register. The function verifies that the register is within the 
valid range before attempting the write. In addition, all reserved bits are 
masked off before the data is transferred.

@param
	address	The register to write to. This must be one of the following values:
       BACKLIGHT_1TO6_ENABLE         
       BACKLIGHT_7TO8_AND_BANK_ENABLE
       BANK1_CONTROL                 
       BANK2_CONTROL                 
       BANK3_CONTROL                 
       BANK4_CONTROL                 
       BANK1_SETTINGS                
       BANK2_SETTINGS                
       BANK3_SETTINGS                
       BANK4_SETTINGS                
       LDO1_CONTROL                  
       LDO2_CONTROL                  
       LDO3_CONTROL                  
       LDO4_CONTROL                  
       LIGHTING_ASSIGNMENTS          
       EFFECT_RATE_SETTINGS          
       EFFECT_RATE1_TIME             
       EFFECT_RATE2_TIME             
       ADC_ENABLE                    
       ADC_OUTPUT                    
       ADC_RISE                      
       ADC_FALL                      
       ADP_OLE_CONTROLS              
	data	The data to be written to the register.

@return
	SC668_SUCCESS			The data has been written to the register of the SC668.
	SC668_INVALID_ADDRESS	The register is outside of the valid range and the data will not be sent to the SC668.

 */
SC668_ResultCode sc668_write_register(SC668_Register address, unsigned char data, const char * device_name) {
	SC668_ResultCode ret = SC668_SUCCESS;
	DBG("[%s]-addr:0x%x, d:0x%x\n", device_name, address, data);
    if ((address >= BACKLIGHT_1TO6_ENABLE)&&(address <= ADP_OLE_CONTROLS))
    {
		mutex_lock(&sc668_drvdata.sc668_lock);
		ret = sc668_set_register(address, data, registerUsableBits[address]);
		mutex_unlock(&sc668_drvdata.sc668_lock);
		return ret;
    }
    else
    {
		return SC668_INVALID_ADDRESS;
    }
}
EXPORT_SYMBOL(sc668_write_register);

/*!
The sc668_read_register function is the corresponding read function.  Any of the available 
registers can be read using this function.  The STATUS register may be the most 
useful and values for bit masking are provided (see the example).  
To read single status bits, the GetStatus function may be more convenient.

@param
	address	The register to write to. This must be one of the following values:
       BACKLIGHT_1TO6_ENABLE            
       BACKLIGHT_7TO8_AND_BANK_ENABLE	
       BANK1_CONTROL                 	
       BANK2_CONTROL                 	
       BANK3_CONTROL                 	
       BANK4_CONTROL                 	
       BANK1_SETTINGS                	
       BANK2_SETTINGS                	
       BANK3_SETTINGS                	
       BANK4_SETTINGS                	
       LDO1_CONTROL                  
       LDO2_CONTROL                  	
       LDO3_CONTROL                  	
       LDO4_CONTROL                  	
       LIGHTING_ASSIGNMENTS          	
       EFFECT_RATE_SETTINGS          	
       EFFECT_RATE1_TIME             	
       EFFECT_RATE2_TIME             	
       ADC_ENABLE                    	
       ADC_OUTPUT                    	
       ADC_RISE                      	
       ADC_FALL                      	
       ADP_OLE_CONTROLS           
          
@return
	An unsigned char containing the current value of the given register.  
	See the SC668 Datasheet for more details on these registers.

 */
unsigned char sc668_read_register(SC668_Register address, const char * device_name) {    
    DBG("[%s]-addr:0x%x\n", device_name, address);
    if ((address >= BACKLIGHT_1TO6_ENABLE)&&(address <= ADP_OLE_CONTROLS))
    {       
       return sc668_get_register(address);
       mutex_unlock(&sc668_drvdata.sc668_lock);
    }
    else
    {
       return 0;
    }
}
EXPORT_SYMBOL(sc668_read_register);

/*****************gpio function****************/

/************************************************************
 sc668_enable_chip                                                                         
param
	on:0-disable chip  
		 1-enable chip    	
 return 
		0:successful
		others:fail                                                               
************************************************************/
int sc668_enable_chip(int on, const char * device_name)
{	
  	int ret = 0;
  	
	DBG("[%s]---on:%d\n", device_name, on);
	if(sc668_drvdata.state==on) return 0;	
	
	ret = gpio_direction_output(sc668_drvdata.enable_pin, on);
	if(ret) {
	  	DBG("GPIO-%d gpio_direction_output failed: %d\n", sc668_drvdata.enable_pin, ret);
		return ret;
	}
	gpio_free(sc668_drvdata.enable_pin);
	
	msleep(20);
	sc668_drvdata.state = on;
	
	return 0;
}
//EXPORT_SYMBOL(sc668_enable_chip);

/************************************************************
 sc688_get_chip_state                                                                         
 return 
		0:chip disable 
		1:chip enable                                                                        
************************************************************/
int sc668_get_chip_state(void)
{
	return sc668_drvdata.state;
}
EXPORT_SYMBOL(sc668_get_chip_state);

/*************************************************************/
/*	LDO setting control                                                                                           */
/*param                                                                                                             */
/*		num : 1 -LDO1, 2- LDO2, 3 - LDO3, 4 - LDO4                                                 */
/*		command : voltage settings --- LDO_Voltage                                                   */
/*		device_name : dev->name                                                                            */ 
/*	return                                                                                                             */
/*		SC668_SUCCESS:setting ok                                                                          */
/*		SC668_INVALID_PARAMETER:voltage setting wrong                                        */
/*		SC668_I2C_ERROR:I2C command fail                                                                            */
/*************************************************************/
SC668_ResultCode sc668_LDO_setting(int num, u8 command, const char * device_name)
{
	int ret = -1;
	DBG("%s, LDO%d, command:0x%x\n", device_name, num, command);
	mutex_lock(&sc668_drvdata.sc668_lock);
	switch(num)
	{
		case 1:
		{
			if(command)
			{
				if(sc668_drvdata.LDO1_setting)
				{
					if(command==sc668_drvdata.LDO1_setting)
					{
						sc668_drvdata.LDO1_count++;
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_SUCCESS;
					}
					else
					{
						//DBG("voltage setting is different ! LDO%d_setting : 0x%x\n", num, sc668_drvdata.LDO1_setting);
						printk(KERN_ERR "[%s:%d]voltage setting is different ! LDO%d_setting : 0x%x\n", __func__, __LINE__, num, sc668_drvdata.LDO1_setting);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_INVALID_PARAMETER;
					}
				}
				else
				{
					if(sc668_drvdata.state==SC668_OFF) sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
					ret=sc668_write_reg(LDO1_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO1_setting=command;
						sc668_drvdata.LDO1_count ++;
					}
				}
			}
			else
			{
				if(sc668_drvdata.LDO1_setting)
				{
					sc668_drvdata.LDO1_count --;
					if(sc668_drvdata.LDO1_count >0)
					{
						mutex_unlock(&sc668_drvdata.sc668_lock);
						DBG("sc668_drvdata.LDO1_count = %d\n", sc668_drvdata.LDO1_count);
						return SC668_SUCCESS;
					}
					ret=sc668_write_reg(LDO1_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO1_setting = command;
						sc668_drvdata.LDO1_count --;
					}						
				}
				else
				{
					//DBG("LDO%d was formerly off !!!\n", num);
					printk(KERN_ERR "[%s:%d]LDO%d was formerly off !\n", __func__, __LINE__,num);
					mutex_unlock(&sc668_drvdata.sc668_lock);
					return SC668_INVALID_PARAMETER;
				}
			}
			break;	
		}
		case 2:
		{
			if(command)
			{
				if(sc668_drvdata.LDO2_setting)
				{
					if(command==sc668_drvdata.LDO2_setting)
					{
						sc668_drvdata.LDO2_count++;
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_SUCCESS;
					}
					else
					{
						//DBG("voltage setting is different ! LDO%d_setting : 0x%x\n", num, sc668_drvdata.LDO2_setting);
						printk(KERN_ERR "[%s:%d]voltage setting is different ! LDO%d_setting : 0x%x\n", __func__, __LINE__, num, sc668_drvdata.LDO2_setting);
						return SC668_INVALID_PARAMETER;
					}
				}
				else
				{
					if(sc668_drvdata.state==SC668_OFF) sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
					ret=sc668_write_reg(LDO2_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO2_setting=command;
						sc668_drvdata.LDO2_count ++;
					}
				}
			}
			else
			{
				if(sc668_drvdata.LDO2_setting)
				{
					sc668_drvdata.LDO2_count --;
					if(sc668_drvdata.LDO2_count >0){
						DBG("sc668_drvdata.LDO2_count = %d\n", sc668_drvdata.LDO2_count);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_SUCCESS;
					}
					ret=sc668_write_reg(LDO2_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO2_setting = command;
						sc668_drvdata.LDO2_count --;
					}						
				}
				else
				{
					//DBG("LDO%d was formerly off !!!\n", num);
					printk(KERN_ERR "[%s:%d]LDO%d was formerly off !\n", __func__, __LINE__,num);
					mutex_unlock(&sc668_drvdata.sc668_lock);
					return SC668_INVALID_PARAMETER;
				}
			}
			break;	
		}
		case 3:
		{
			if(command)
			{
				if(sc668_drvdata.LDO3_setting)
				{
					if(command==sc668_drvdata.LDO3_setting)
					{
						sc668_drvdata.LDO3_count++;
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_SUCCESS;
					}
					else
					{
						//DBG("voltage setting is different ! LDO%d_setting : 0x%x\n", num, sc668_drvdata.LDO3_setting);
						printk(KERN_ERR "[%s:%d]voltage setting is different ! LDO%d_setting : 0x%x\n", __func__, __LINE__, num, sc668_drvdata.LDO3_setting);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_INVALID_PARAMETER;
					}
				}
				else
				{
					if(sc668_drvdata.state==SC668_OFF) sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
					ret=sc668_write_reg(LDO3_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO3_setting=command;
						sc668_drvdata.LDO3_count ++;
					}
				}
			}
			else
			{
				if(sc668_drvdata.LDO3_setting)
				{
					sc668_drvdata.LDO3_count --;
					if(sc668_drvdata.LDO3_count >0)
					{
						mutex_unlock(&sc668_drvdata.sc668_lock);
						DBG("sc668_drvdata.LDO3_count = %d\n", sc668_drvdata.LDO3_count);
						return SC668_SUCCESS;					
					}
					ret=sc668_write_reg(LDO3_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO3_setting = command;
						sc668_drvdata.LDO3_count --;
					}						
				}
				else
				{
					//DBG("LDO%d was formerly off !!!\n", num);
					printk(KERN_ERR "[%s:%d]LDO%d was formerly off !\n", __func__, __LINE__,num);
					mutex_unlock(&sc668_drvdata.sc668_lock);
					return SC668_INVALID_PARAMETER;
				}
			}
			break;	
		}
		case 4:
		{
			if(command)
			{
				if(sc668_drvdata.LDO4_setting)
				{
					if(command==sc668_drvdata.LDO4_setting)
					{
						sc668_drvdata.LDO4_count++;
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_SUCCESS;
					}
					else
					{
						//DBG("voltage setting is different ! LDO%d_setting : 0x%x\n", num, sc668_drvdata.LDO4_setting);
						printk(KERN_ERR "[%s:%d]voltage setting is different ! LDO%d_setting : 0x%x\n", __func__, __LINE__, num, sc668_drvdata.LDO4_setting);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return SC668_INVALID_PARAMETER;
					}
				}
				else
				{
					if(sc668_drvdata.state==SC668_OFF) sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
					ret=sc668_write_reg(LDO4_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO4_setting=command;
						sc668_drvdata.LDO4_count ++;
					}
				}
			}
			else
			{
				if(sc668_drvdata.LDO4_setting)
				{
					sc668_drvdata.LDO4_count --;
					if(sc668_drvdata.LDO4_count >0)
					{
						mutex_unlock(&sc668_drvdata.sc668_lock);
						DBG("sc668_drvdata.LDO4_count = %d\n", sc668_drvdata.LDO4_count);
						return SC668_SUCCESS;					
					}
					ret=sc668_write_reg(LDO4_CONTROL, command);
					if(ret <0)
					{
						//DBG("write setting fail!!! ret = %d\n", ret);
						printk(KERN_ERR "[%s:%d]write setting fail! ret = %d\n", __func__, __LINE__, ret);
						mutex_unlock(&sc668_drvdata.sc668_lock);
						return ret;
					}
					else
					{
						sc668_drvdata.LDO4_setting = command;
						sc668_drvdata.LDO4_count --;
					}						
				}
				else
				{
					//DBG("LDO%d was formerly off !!!\n", num);
					printk(KERN_ERR "[%s:%d]LDO%d was formerly off !\n", __func__, __LINE__,num);
					mutex_unlock(&sc668_drvdata.sc668_lock);
					return SC668_INVALID_PARAMETER;
				}
			}
			break;	
		}
		default:
			DBG("LDO%d isn't exit !!!\n", num);
			mutex_unlock(&sc668_drvdata.sc668_lock);
			return SC668_INVALID_PARAMETER;
		
	}
	mutex_unlock(&sc668_drvdata.sc668_lock);
	return ret;
}
EXPORT_SYMBOL(sc668_LDO_setting);

/*! 
  The set_light_assignments function assigns individual lights to four different banks 
  and then assigns each bank to a group.

  @param bankAssignment Assigns lights to banks. 
         - BANK1_BL1_BL8 (Assignes backlights 1 to 8 to bank 1)                                   
         - BANK1_BL2_BL8_BANK2_BL1 (Assigns backlights 2 to 8 to bank 1 and backlight 1 to bank 2)                        
         - BANK1_BL3_BL8_BANK2_BL1_BL2                     
         - BANK1_BL3_BL8_BANK2_BL2_BANK3_BL1               
         - BANK1_BL4_BL8_BANK2_BL1_BL3                     
         - BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1     
         - BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1 
         - BANK1_BL6_BL8_BANK2_BL3_BL5_BANK3_BL2_BANK4_BL1 

  @param groupAssignment Assigns banks to two different groups. 
         - GROUP1_BK1_GROUP2_BK2_BK3_BK4   (Assigns Bank 1 to Group 1 and Bank 2 to Bank 4 to Group 2)
         - GROUP1_BK1_BK2_GROUP2_BK3_BK4
         - GROUP1_BK1_BK2_BK3_GROUP2_BK4
         - GROUP1_BK1_BK2_BK3_BK4

  @return SC668_ResultCode 
         - #SC668_SUCCESS The light assignement settings have been successfully changed.
         - #SC668_INVALID_PARAMETER The bankAssignment and/or the groupAssignment parameter is outside of the valid 
                                      range. The light assignment has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_light_assignments(unsigned char bankAssignment, unsigned char groupAssignment, const char * device_name)
{
   DBG("[%s] ---bankAssignment:0x%x, groupAssignment:0x%x\n", device_name, bankAssignment, groupAssignment);
   // if the input values are invalid, return without performing any action
   if ((bankAssignment > BANK1_BL6_BL8_BANK2_BL3_BL5_BANK3_BL2_BANK4_BL1)||(groupAssignment > GROUP1_BK1_BK2_BK3_BK4))
   {
      return SC668_INVALID_PARAMETER;
   }

   groupAssignment <<= 3;
   bankAssignment |= groupAssignment;
   
   // write the register out to the hardware      
   return sc668_set_register(LIGHTING_ASSIGNMENTS, bankAssignment,LIGHTING_ASSIGNMENTS_MASK);
}
EXPORT_SYMBOL(sc668_set_light_assignments);
/* 
  The sc668_enable_backlights function is used to enable a subset of backlights 
  without affecting the state of the others. 

  @param backlights    The bits corresponding to the backlights which are to be 
                       enabled are ORed together to create the backlights parameter. 
                       The state of all backlights not included in this parameter are 
                       unaffected. This parameter must be set to the bitwise ORed results 
                       of none, any, or all of the Backlight values. Use the following defines.
         - BACKLIGHT_8                   
         - BACKLIGHT_7         
         - BACKLIGHT_6
         - BACKLIGHT_5
         - BACKLIGHT_4
         - BACKLIGHT_3
         - BACKLIGHT_2
         - BACKLIGHT_1

  @return SC668_ResultCode 
          - #SC668_SUCCESS The backlight settings have been successfully changed.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_enable_backlights(unsigned char backlights, const char * device_name)
{
   SC668_ResultCode result = SC668_SUCCESS;   
   unsigned char backLights1to6, backLights7to8;
   DBG("[%s]---backlights:0x%x\n", device_name, backlights);
   backLights1to6 = backlights;
   backLights7to8 = backlights >> 6;

   // mask off bit 7 and 8
   backLights1to6 &=  (BACKLIGHT_6_ENABLE_MASK |
                       BACKLIGHT_5_ENABLE_MASK | 
                       BACKLIGHT_4_ENABLE_MASK | 
                       BACKLIGHT_3_ENABLE_MASK | 
                       BACKLIGHT_2_ENABLE_MASK |  
                       BACKLIGHT_1_ENABLE_MASK);
      
   
   // mask off bit 7 and 8
   backLights7to8 &=  (BACKLIGHT_7_ENABLE_MASK | 
                       BACKLIGHT_8_ENABLE_MASK);


   // Make sure DIS and EN are 00, or turn on of individual lights will not work
      
   if ((sc668_set_register(BACKLIGHT_1TO6_ENABLE, BACKLIGHT_1TO6_ENABLE_MASK, backLights1to6) == SC668_I2C_ERROR)
     ||(sc668_set_register(BACKLIGHT_7TO8_AND_BANK_ENABLE, BACKLIGHT_7TO8_AND_BANK_ENABLE_MASK, backLights7to8) == SC668_I2C_ERROR))
   {
      // if we have a result worse than the current result use it instead
      result = SC668_I2C_ERROR;
   }   
   return result;
}

/*! 
  The sc668_disable_backlights function is used to disable a subset of backlights 
  without affecting the state of the others. 
  @param backlight1to6 The bits corresponding to the backlights which are to be 
                       disabled are ORed together to create the backlights parameter. 
                       The state of all backlights not included in this parameter are 
                       unaffected. This parameter must be set to the bitwise ORed results 
                       of none, any, or all of the Backlight values. Use the following defines.
         - BACKLIGHT_8
         - BACKLIGHT_7                    
         - BACKLIGHT_6
         - BACKLIGHT_5
         - BACKLIGHT_4
         - BACKLIGHT_3
         - BACKLIGHT_2
         - BACKLIGHT_1

  @return SC668_ResultCode 
          - #SC668_SUCCESS The backlight settings have been successfully changed.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_disable_backlights(unsigned char backlights, const char * device_name)
{   
   SC668_ResultCode result = SC668_SUCCESS;     
   unsigned char backLights1to6, backLights7to8;
   
    DBG("[%s]---backlights:0x%x\n", device_name, backlights);
   backLights1to6 = backlights;
   backLights7to8 = backlights >> 6;


   // mask off bit 7 and 8
   backLights1to6 &=  (BACKLIGHT_6_ENABLE_MASK | 
                      BACKLIGHT_5_ENABLE_MASK | 
                      BACKLIGHT_4_ENABLE_MASK | 
                      BACKLIGHT_3_ENABLE_MASK | 
                      BACKLIGHT_2_ENABLE_MASK |  
                      BACKLIGHT_1_ENABLE_MASK);
   
   // mask off bit 7 and 8
   backLights7to8 &=  (BACKLIGHT_7_ENABLE_MASK | 
                      BACKLIGHT_8_ENABLE_MASK);
   // Make sure DIS and EN are 00, or turn on of individual lights will not work    
  
  
   // write the register out to the hardware   
   if ((sc668_set_register(BACKLIGHT_1TO6_ENABLE, 0, backLights1to6) == SC668_I2C_ERROR)||
       (sc668_set_register(BACKLIGHT_7TO8_AND_BANK_ENABLE, 0, backLights7to8) == SC668_I2C_ERROR))
   {
      // if we have a result worse than the current result use it instead
      result = SC668_I2C_ERROR;
   }   
   return result;
}
/* 
  The sc668_enable_Banks function is used to enable a subset of banks.

  @param bank    The bits corresponding to the banks which are to be 
                 enabled are ORed together to create the banks parameter. 
                 The state of all banks not included in this parameter are 
                 unaffected. This parameter must be set to the bitwise ORed results 
                 of none, any, or all of the Bank values. Use the following defines.
          - BANK_4_ENABLE
          - BANK_3_ENABLE
          - BANK_2_ENABLE
          - BANK_1_ENABLE
  @return SC668_ResultCode 
          - #SC668_SUCCESS The banks settings have been successfully changed.
          - #SC668_RESERVED_BITS_EXCLUDED The bank parameter contains bits which 
                                            should not be sent. These bits have been masked 
                                            off and the remaining bits have been used.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_enable_Banks(unsigned char bank, const char * device_name)
{
   SC668_ResultCode result = SC668_SUCCESS;
   DBG("[%s]--bank:%d\n", device_name, bank);
   
   // if there are any invalid bits in the backlight field set, mask them off
   // and send the rest to the SC668
   if (bank & ~(BANK_4_ENABLE_MASK | 
                BANK_3_ENABLE_MASK | 
                BANK_2_ENABLE_MASK | 
                BANK_1_ENABLE_MASK))
   {
      // mask off the unused bits
      bank &=  (BANK_4_ENABLE_MASK |
                BANK_3_ENABLE_MASK | 
                BANK_2_ENABLE_MASK | 
                BANK_1_ENABLE_MASK);
      
      result = SC668_RESERVED_BITS_EXCLUDED;
   }

   // Set bank ennable bit and clear disable bit
   bank |= BANK_ENABLE_BIT;
   bank &= ~BANK_DISABLE_BIT;

   // write the register out to the hardware   
   if (sc668_set_register(BACKLIGHT_7TO8_AND_BANK_ENABLE, bank, BACKLIGHT_7TO8_AND_BANK_ENABLE_MASK) != SC668_SUCCESS)
   {
      // if we have a result worse than the current result use it instead
      result = SC668_I2C_ERROR;
   }
   
   return result;
}
EXPORT_SYMBOL(sc668_enable_Banks);

/*! 
  The sc668_disable_banks function is used to disable a subset of banks. 

  @param bank    The bits corresponding to the backlights which are to be 
                 disabled are ORed together to create the backlights parameter. 
                 The state of all backlights not included in this parameter are 
                 unaffected. This parameter must be set to the bitwise ORed results 
                 of none, any, or all of the Backlight values. Use the following defines.
          - BANK_4_ENABLE
          - BANK_3_ENABLE
          - BANK_2_ENABLE
          - BANK_1_ENABLE
  @return SC668_ResultCode 
          - #SC668_SUCCESS The bank settings have been successfully changed.
          - #SC668_RESERVED_BITS_EXCLUDED The banks parameter contains bits which 
                                            should not be sent. These bits have been masked 
                                            off and the remaining bits have been used.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_disable_banks(unsigned char bank, const char * device_name)
{
   SC668_ResultCode result = SC668_SUCCESS;
   DBG("[%s]--bank:%d\n", device_name, bank);
   
   // if there are any invalid bits in the backlight field set mask them off
   // and send the rest to the SC668
   if (bank & ~(BANK_4_ENABLE_MASK | 
                BANK_3_ENABLE_MASK | 
                BANK_2_ENABLE_MASK | 
                BANK_1_ENABLE_MASK))
   {
      // mask off the unused bits
      bank &=  (BANK_4_ENABLE_MASK |
                BANK_3_ENABLE_MASK | 
                BANK_2_ENABLE_MASK | 
                BANK_1_ENABLE_MASK);
      
      result = SC668_RESERVED_BITS_EXCLUDED;
   }
   // Set bank disnable bit and clear enable bit
   bank |= BANK_DISABLE_BIT;
   bank &= ~BANK_ENABLE_BIT;
           
   // write the register out to the hardware  
   if (sc668_set_register(BACKLIGHT_7TO8_AND_BANK_ENABLE, bank, BACKLIGHT_7TO8_AND_BANK_ENABLE_MASK) != SC668_SUCCESS)
   {
      // if we have a result worse than the current result use it instead
      result = SC668_I2C_ERROR;
   }
   
   return result;
}
EXPORT_SYMBOL(sc668_disable_banks);

/*! 
  The sc668_set_effect_rate function sets light effect breathe and fade rates for group 1 and group 2 lights

  @param effectGrp1 Sets breathe and fade rate on group 1 assigned lights. Use the following defines.  
         - BREATH_0_FADE_0
         - BREATH_4_FADE_1
         - BREATH_8_FADE_2
         - BREATH_16_FADE_4
         - BREATH_24_FADE_6
         - BREATH_32_FADE_8
         - BREATH_48_FADE_12
         - BREATH_64_FADE_16
  @param effectGrp2 Sets breathe and fade rate on group 2 assigned lights. Use same defines as in effectGrp1. 
  @return SC668_ResultCode 
         - #SC668_SUCCESS The Effect Rate (0Fh) register has been successfully changed.
         - #SC668_INVALID_PARAMETER The effectGrp1 and/or the effectGrp2 parameter is outside of the valid 
                                      range. The effect rate has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_effect_rate(unsigned char effectGrp1, unsigned char effectGrp2, const char * device_name)
{
   
   DBG("[%s]--effectGrp1:0x%x, effectGrp2:0x%x\n", device_name, effectGrp1, effectGrp2);
   // if the input values are invalid, return without performing any action
   if ((effectGrp1 > BREATH_64_FADE_16)||(effectGrp2 > BREATH_64_FADE_16))
   {
      return SC668_INVALID_PARAMETER;
   }
   effectGrp2 <<= 3;
   effectGrp1 |= effectGrp2;
   
   // write the register out to the hardware    
   return sc668_set_register(EFFECT_RATE_SETTINGS, effectGrp1, EFFECT_RATE_SETTINGS_MASK);
}
EXPORT_SYMBOL(sc668_set_effect_rate);

/*! 
  The sc668_set_effectTime_grp1 function sets the duration of time that 
  the backlights will stay at the start value and at the target value. 
  This feature is an additional way to customize the fade, breathe, and blink lighting effects, 
  by pausing the brightness at the beginning and at the end of each lighting effect sequence.
  This function only effects group 1 assigned lights

  @param startTime Sets the starting time. Use the following defines.
         - TIME_32
         - TIME_64
         - TIME_256
         - TIME_512
         - TIME_1024
         - TIME_2048
         - TIME_3072
         - TIME_4096
  @param targetTime Sets the target time. Use same values as for starting time. 
  @return SC668_ResultCode 
         - #SC668_SUCCESS The target and start time register (10h) for Effect Rate 1 has been successfully changed.
         - #SC668_INVALID_PARAMETER The startTime and/or the targetTime parameter is outside of the valid 
                                      range. The effect time has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */ 
SC668_ResultCode sc668_set_effectTime_grp1(unsigned char startTime, unsigned char targetTime, const char * device_name)
{
   DBG("[%s]--startTime:0x%x, targetTime:0x%x\n", device_name, startTime, targetTime);
   // if the input values are invalid, return without performing any action
   if ((startTime > TIME_4096)||(targetTime > TIME_4096))
   {
      return SC668_INVALID_PARAMETER;
   }
   targetTime <<= 3;
   startTime |= targetTime;
   
   // write the register out to the hardware   
   return sc668_set_register(EFFECT_RATE1_TIME, startTime, EFFECT_RATE1_TIME_MASK);
}
EXPORT_SYMBOL(sc668_set_effectTime_grp1);

/*! 
  The sc668_set_effectTime_grp2 function sets the duration of time that 
  the backlights will stay at the start value and at the target value. 
  This feature is an additional way to customize the fade, breathe, and blink lighting effects, 
  by pausing the brightness at the beginning and at the end of each lighting effect sequence.
  This function only effects group 2 assigned lights

  @param startTime Sets the starting time. Use the following defines.
         - TIME_32
         - TIME_64
         - TIME_256
         - TIME_512
         - TIME_1024
         - TIME_2048
         - TIME_3072
         - TIME_4096
  @param targetTime Sets the target time. Use same values as for starting time. 
  @return SC668_ResultCode 
         - #SC668_SUCCESS The target and start time register (11h) for Effect Rate 2 has been successfully changed.
         - #SC668_INVALID_PARAMETER The startTime and/or the targetTime parameter is outside of the valid 
                                      range. The effect time has not been changed.
         - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_effectTime_grp2(unsigned char startTime, unsigned char targetTime, const char * device_name)
{
   DBG("[%s]--startTime:0x%x, targetTime:0x%x\n", device_name, startTime, targetTime);
   // if the input values are invalid, return without performing any action
   if ((startTime > TIME_4096)||(targetTime > TIME_4096))
   {
      return SC668_INVALID_PARAMETER;
   }
   targetTime <<= 3;
   startTime |= targetTime;

   // set the register value for group 1 and group 2 effect rates
   registerShadow[EFFECT_RATE2_TIME] = (unsigned char)startTime;
   
   // write the register out to the hardware   
   return sc668_set_register(EFFECT_RATE2_TIME, startTime, EFFECT_RATE2_TIME_MASK);
}
EXPORT_SYMBOL(sc668_set_effectTime_grp2);

/* 
  The sc668_set_backLight_currentAndEffect function is used to set the backlight current and target 
  current as well as enable fade, blink, breathe or no effect
  @param bank Specified bank for fade enable. Use the following defines.
     - BANK_1
     - BANK_2
     - BANK_3
     - BANK_4
  @param BL_current Sets the backlight current. Use the following defines. 
     - BACKLIGHT_CURRENT_0_0                              
     - BACKLIGHT_CURRENT_0_0_5
     - BACKLIGHT_CURRENT_0_1
     - BACKLIGHT_CURRENT_0_2
     - BACKLIGHT_CURRENT_0_5
     - BACKLIGHT_CURRENT_1_0
     - BACKLIGHT_CURRENT_1_5 
     - BACKLIGHT_CURRENT_2_0
     - BACKLIGHT_CURRENT_2_5
     - BACKLIGHT_CURRENT_3_0 
     - BACKLIGHT_CURRENT_3_5
     - BACKLIGHT_CURRENT_4_0
     - BACKLIGHT_CURRENT_4_5
     - BACKLIGHT_CURRENT_5_0
     - BACKLIGHT_CURRENT_6_0
     - BACKLIGHT_CURRENT_7_0 
     - BACKLIGHT_CURRENT_8_0
     - BACKLIGHT_CURRENT_9_0
     - BACKLIGHT_CURRENT_10_0
     - BACKLIGHT_CURRENT_11_0
     - BACKLIGHT_CURRENT_12_0
     - BACKLIGHT_CURRENT_13_0
     - BACKLIGHT_CURRENT_14_0
     - BACKLIGHT_CURRENT_15_0
     - BACKLIGHT_CURRENT_16_0
     - BACKLIGHT_CURRENT_17_0
     - BACKLIGHT_CURRENT_18_0
     - BACKLIGHT_CURRENT_19_0
     - BACKLIGHT_CURRENT_20_0
     - BACKLIGHT_CURRENT_21_0
     - BACKLIGHT_CURRENT_23_0
     - BACKLIGHT_CURRENT_25_0 
  @param targetCurrent Set targeted backlight current. Use same defines as for the current variable.
  @param effect Enable Fade, Breathe, Blink or no effect. Use the following defines.
     - ENABLE_FADE
     - ENABLE_BREATHE
     - ENABLE_BLINK
     - NO_EFFECT
  @return SC668_ResultCode 
          - #SC668_SUCCESS The settings have been successfully changed.
          - #SC668_INVALID_PARAMETER The bank, current, or targetCurrent parameter contains invalid bits. The register has not been changed.
          - #SC668_I2C_ERROR   read/write I2C fail
 */
SC668_ResultCode sc668_set_backLight_currentAndEffect(BankType bank, unsigned char BL_current, unsigned char targetCurrent, unsigned char effect, const char * device_name)
{
   SC668_ResultCode result = SC668_SUCCESS;
	//DBG("[%s]-bank:%d, BL:0x%x, target:0x%x, effect:0x%x\n", device_name, bank, BL_current, targetCurrent, effect);
   fih_printk(sc668_debug_mask, FIH_DEBUG_ZONE_G1, "[sc668_set_current]bank:%d, BL:0x%x, target:0x%x, effect:0x%x\n", device_name, bank, BL_current, targetCurrent, effect);
   if (bank > BANK_4)
   {
      return SC668_INVALID_PARAMETER;
   }
   // if the current value is invalid, return without performing any action
   if (((unsigned char)BL_current > BACKLIGHT_CURRENT_25_0)||((unsigned char)targetCurrent > BACKLIGHT_CURRENT_25_0))
   {
      return SC668_INVALID_PARAMETER;
   }
   if (effect > ENABLE_BREATHE)
   {
      return SC668_INVALID_PARAMETER;
   }
   // Set effect bit
   if (effect == ENABLE_FADE)
   {
      BL_current |= BANK_EFFECT_ENABLE_BIT;
   }
   else if (effect == ENABLE_BLINK)
   {
      targetCurrent |= BANK_EFFECT_ENABLE_BIT;
   }
   else if (effect == ENABLE_BREATHE)
   {
      BL_current |= BANK_EFFECT_ENABLE_BIT;
      targetCurrent |= BANK_EFFECT_ENABLE_BIT;
   }

   switch (bank)
   {
      case BANK_1:
      {
         // write the register out to the hardware   
         if ((sc668_set_register(BANK1_CONTROL, BL_current, BANK1_CONTROL_MASK) == SC668_I2C_ERROR)
           ||(sc668_set_register(BANK1_SETTINGS, targetCurrent, BANK1_SETTINGS_MASK) == SC668_I2C_ERROR))
         {
            // if we have a result worse than the current result use it instead
            result = SC668_I2C_ERROR;
         }
         return result;
         break;
      }
      case BANK_2:
      {
         // write the register out to the hardware  
         if ((sc668_set_register(BANK2_CONTROL, BL_current, BANK2_CONTROL_MASK) == SC668_I2C_ERROR)
           ||(sc668_set_register(BANK2_SETTINGS, targetCurrent, BANK2_SETTINGS_MASK) == SC668_I2C_ERROR))
         {
            // if we have a result worse than the current result use it instead
            result = SC668_I2C_ERROR;
         }
         return result;
         break;
      }
      case BANK_3:
      {
         // write the register out to the hardware   
         if ((sc668_set_register(BANK3_CONTROL, BL_current, BANK3_CONTROL_MASK) == SC668_I2C_ERROR)
           ||(sc668_set_register(BANK3_SETTINGS, targetCurrent, BANK3_SETTINGS_MASK) == SC668_I2C_ERROR))
         {
            // if we have a result worse than the current result use it instead
            result = SC668_I2C_ERROR;
         }
         return result;
         break;
      }
      case BANK_4:
      {
         // write the register out to the hardware   
         if ((sc668_set_register(BANK4_CONTROL, BL_current, BANK4_CONTROL_MASK) == SC668_I2C_ERROR)
           ||(sc668_set_register(BANK4_SETTINGS, targetCurrent, BANK4_SETTINGS_MASK) == SC668_I2C_ERROR))
         {
            // if we have a result worse than the current result use it instead
            result = SC668_I2C_ERROR;
         }
         return result;
         break;
      }
   }
   return SC668_INVALID_PARAMETER;
}


#ifdef myTest
static ssize_t sc668Test_run(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	int i=0;
	unsigned LogRun;		
	int ret = -1;		
	u8 data = 0;
	sscanf(buf, "%1d\n", &LogRun);		
	dev_dbg(dev, "%s: %d %d\n", __func__, count, LogRun);		
	

	if(LogRun == 0)//idle
	{	
		if(sc668_drvdata.state == SC668_OFF) sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
		ret = sc668_write_reg(  0x00, 0x3F);
		if(ret)
		{
			DBG("1 write fail(ret:%d)\n",ret);
		}
		else	
		{
			//ret = sc668_read_reg(  0x00, &data);
			ret = sc668_i2c_read_reg( 0x00, &data, 1);
			if(ret)
			{
				DBG("1 read fail(ret:%d)\n",ret);
			}
			else
			{
				DBG("1 data(ret:%d)\n",data);
			}
		}		
	}
	else if (LogRun == 1)//active
	{	
		sc668_LDO_setting(1, 0x06, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 2)//reset
	{	
		sc668_LDO_setting(2, 0x06, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 3)//active ps
	{	
		sc668_LDO_setting(3, 0x06, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 4)//read ps
	{	
		sc668_LDO_setting(4, 0x06, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 11)//read 
	{		
		sc668_LDO_setting(1, 0x00, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 12)//read 
	{		
		sc668_LDO_setting(2, 0x00, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 13)//read 
	{		
		sc668_LDO_setting(3, 0x00, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 14)//read 
	{		
		sc668_LDO_setting(4, 0x00, sc668_drvdata.sc668_i2c_client->name);		
	}
	else if (LogRun == 6)//read  CONFIGREG
	{				
		sc668_set_light_assignments(BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1,GROUP1_BK1_GROUP2_BK2_BK3_BK4, sc668_drvdata.sc668_i2c_client->name);
		sc668_enable_backlights(BACKLIGHT_3, sc668_drvdata.sc668_i2c_client->name);
		//sc668_enable_Banks(BANK_2_ENABLE, sc668_drvdata.sc668_i2c_client->name);//0x01,0x12
		//sc668_disable_backlights(BACKLIGHT_4, sc668_drvdata.sc668_i2c_client->name);//		
		//sc668_set_effect_rate(unsigned char effectGrp1, unsigned char effectGrp2, sc668_drvdata.sc668_i2c_client->name);
		sc668_set_effectTime_grp2(TIME_1024, TIME_256, sc668_drvdata.sc668_i2c_client->name);//0x11,0x0c
		sc668_set_backLight_currentAndEffect(BANK_2, BACKLIGHT_CURRENT_0_0, BACKLIGHT_CURRENT_2_0, ENABLE_BLINK, sc668_drvdata.sc668_i2c_client->name);//0x07,0x27
		//sc668_write_reg(0x03,0x00);
		//sc668_write_reg(0x07,0x27);
	}
	else if(LogRun == 7)//
	{	
		sc668_set_light_assignments(BANK1_BL3_BL8_BANK2_BL1_BL2,GROUP1_BK1_GROUP2_BK2_BK3_BK4, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 8)//
	{			
		sc668_enable_Banks(BANK_2_ENABLE, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 9)//
	{		
		sc668_set_backLight_currentAndEffect(BANK_2, BACKLIGHT_CURRENT_2_0, BACKLIGHT_CURRENT_2_0, NO_EFFECT, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 10)//
	{			
		sc668_disable_backlights(BACKLIGHT_2, sc668_drvdata.sc668_i2c_client->name);
	}
	/*
	else if (LogRun == 11)//
	{		
		ret = sc668_write_reg(  0x0A, 0x09);//2.5
	}
	else if (LogRun == 12)//
	{			
		ret = sc668_write_reg(  0x0A, 0x06);//2.8
	}				
	else if (LogRun == 14)//
	{	
		ret = sc668_write_reg(  0x0B, 0x01);
	}	
	*/
	else if (LogRun == 15)//
	{	
		//sc668_set_light_assignments(BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1,GROUP1_BK1_GROUP2_BK2_BK3_BK4, sc668_drvdata.sc668_i2c_client->name);
		sc668_enable_Banks(BANK_1_ENABLE, sc668_drvdata.sc668_i2c_client->name);
		//sc668_disable_backlights(BACKLIGHT_4, sc668_drvdata.sc668_i2c_client->name);		
		sc668_set_backLight_currentAndEffect(BANK_1, BACKLIGHT_CURRENT_2_0, BACKLIGHT_CURRENT_2_0, NO_EFFECT, sc668_drvdata.sc668_i2c_client->name);
		
	}	
	else if (LogRun == 16)//
	{		
		sc668_enable_Banks(BANK_4_ENABLE, sc668_drvdata.sc668_i2c_client->name);				
		sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_2_0, BACKLIGHT_CURRENT_2_0, NO_EFFECT, sc668_drvdata.sc668_i2c_client->name);
	}
	else if (LogRun == 17)//
	{		
		DBG("data0x00:0x%x\n",sc668_read_register(0x00, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x01:0x%x\n",sc668_read_register(0x01, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x02:0x%x\n",sc668_read_register(0x02, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x03:0x%x\n",sc668_read_register(0x03, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x04:0x%x\n",sc668_read_register(0x04, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x05:0x%x\n",sc668_read_register(0x05, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x06:0x%x\n",sc668_read_register(0x06, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x07:0x%x\n",sc668_read_register(0x07, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x08:0x%x\n",sc668_read_register(0x08, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x09:0x%x\n",sc668_read_register(0x09, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0A:0x%x\n",sc668_read_register(0x0A, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0B:0x%x\n",sc668_read_register(0x0B, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0C:0x%x\n",sc668_read_register(0x0C, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0D:0x%x\n",sc668_read_register(0x0D, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0E:0x%x\n",sc668_read_register(0x0E, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x0F:0x%x\n",sc668_read_register(0x0F, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x10:0x%x\n",sc668_read_register(0x10, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x11:0x%x\n",sc668_read_register(0x11, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x12:0x%x\n",sc668_read_register(0x12, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x13:0x%x\n",sc668_read_register(0x13, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x14:0x%x\n",sc668_read_register(0x14, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x15:0x%x\n",sc668_read_register(0x15, sc668_drvdata.sc668_i2c_client->name));
		DBG("data0x16:0x%x\n",sc668_read_register(0x16, sc668_drvdata.sc668_i2c_client->name));
		
	}
	else if (LogRun == 18)//
	{
	sc668_set_light_assignments(

                BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1, 

                GROUP1_BK1_GROUP2_BK2_BK3_BK4, sc668_drvdata.sc668_i2c_client->name);       

        mdelay(10);       

        //enable bank1 for lcd backlight

        sc668_enable_Banks(BANK_1_ENABLE, sc668_drvdata.sc668_i2c_client->name);
		 //sc668_write_register(0x00,0x3b, sc668_drvdata.sc668_i2c_client->name);


        mdelay(10);

        //set backlight

        sc668_set_backLight_currentAndEffect(BANK_1, 

                BACKLIGHT_CURRENT_25_0, 

                BACKLIGHT_CURRENT_25_0, NO_EFFECT, sc668_drvdata.sc668_i2c_client->name);
                
        for(  i=0; i<=0x16; i++ ){

                sc668_read_register(i, sc668_drvdata.sc668_i2c_client->name);

        }


       
	}
		/*	if (LogRun == 1)	{		// Open log file and start capture		diag_start_log();	}	else if (LogRun == 0)	{		// Close log file and stop capture		diag_stop_log();	}	else if (LogRun == 2)	{		open_qxdm_filter_file();	}	else if (LogRun == 3)	{		close_qxdm_filter_file();	}*/
	return count;
}

DEVICE_ATTR(sc668Test, 0644, NULL, sc668Test_run);

#endif


#ifdef CONFIG_PM
static int sc668_suspend(struct i2c_client *nLeds, pm_message_t mesg)
{
	return 0;
}

static int sc668_resume(struct i2c_client *nLeds)
{

	return 0;
}
#else
# define sc668_suspend NULL
# define tc668_resume  NULL
#endif


/*Add misc device ioctl command functions*/

/*******************File control function******************/
// devfs
static int sc668_miscdev_open( struct inode * inode, struct file * file )
{
	printk( KERN_INFO "sc668 open\n" );
	if( ( file->f_flags & O_ACCMODE ) == O_WRONLY )
	{
		printk( KERN_INFO "sc668's device node is readonly\n" );
		return -1;
	}
	else
		return 0;
}
static int sc668_miscdev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd2, unsigned long arg )
{
  	int ret = 0;
	int index = 0;
	printk( KERN_INFO "sc668 ioctl\n" );
    if(copy_from_user(&index, (int __user*)arg, sizeof(int)))
	{
		printk(KERN_ERR "Get user-space data error\n");
		return -1;
	}
	if(index < 0 || index > 7)
	{
	  	printk(KERN_ERR "[%s:%d]Wrong LED index\n", __func__, __LINE__);
	  	return -1;
	}
	if(cmd2 == SC668_IO_OFF)
		sc668_enable_chip(SC668_OFF, sc668_drvdata.sc668_i2c_client->name);
	else if(cmd2 == SC668_IO_ON)
	  	sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);
	else
	{
	  	printk("[%s:%d]Unknow ioctl cmd", __func__, __LINE__);
	  	ret = -1;
	}
	return ret;
}
static int sc668_miscdev_release( struct inode * inode, struct file * filp )
{
	printk(KERN_INFO "sc668 release\n");
	return 0;
}
static const struct file_operations sc668_miscdev_fops = {
	.open = sc668_miscdev_open,
	.ioctl = sc668_miscdev_ioctl,
	.release = sc668_miscdev_release,
};
static struct miscdevice sc668_miscdev = {
 	.minor = MISC_DYNAMIC_MINOR,
	.name = "sc668_miscdev",
	.fops = &sc668_miscdev_fops,
};

/******************Driver Functions*******************/
static int __devinit sc668_probe(struct i2c_client *client,
				    const struct i2c_device_id *id)
{
	int ret = -EINVAL;	
	struct sc668_platform_data *sc668_pd = client->dev.platform_data;
	if(ProductID == Project_DMO || ProductID == Project_DMT) sc668_pd->sc668_pwm_pin = 92;		
	
	//DBG("(addr:0x%x)\n",client->addr);
	//DBG("(name:%s)\n",client->name);	
	printk(KERN_INFO "[%s:%d](addr:0x%x)\n", __func__, __LINE__,client->addr);
	printk(KERN_INFO "[%s:%d](name:%s)\n", __func__, __LINE__,client->name);
	
	i2c_set_clientdata(client, &sc668_drvdata);
	sc668_drvdata.enable_pin		= sc668_pd->sc668_enable_pin;
	sc668_drvdata.pwm_pin		= sc668_pd->sc668_pwm_pin;
	sc668_drvdata.sc668_i2c_client	= client;
	sc668_drvdata.state = SC668_OFF;
	//ORIGHWID = FIH_READ_ORIG_HWID_FROM_SMEM();//FIH, Nicoel, 2010/07/20
	sc668_drvdata.LDO1_setting = 0;
	sc668_drvdata.LDO2_setting = 0;
	sc668_drvdata.LDO3_setting = 0;
	sc668_drvdata.LDO4_setting = 0;
	sc668_drvdata.LDO1_count= 0;
	sc668_drvdata.LDO2_count= 0;
	sc668_drvdata.LDO3_count= 0;
	sc668_drvdata.LDO4_count= 0;
	
	printk("sc668 sc668_pwm_pin :%d\n", sc668_drvdata.pwm_pin);
#ifdef myTest

	ret = device_create_file(&client->dev, &dev_attr_sc668Test);		
	if (ret < 0)	
	{		
		dev_err(&client->dev, "%s: Create sc668Test attribute \"qxdm2sd\" failed!! <%d>", __func__, ret);	
		//DBG("Create sc668Test fail!!<%d>\n",ret);
		printk(KERN_ERR "%s: Create sc668Test fail!!<%d>\n",  __func__, ret);
	}
#endif		
	
	mutex_init(&sc668_drvdata.sc668_lock);
	//PWM pin is high
	gpio_tlmm_config(GPIO_CFG(sc668_drvdata.pwm_pin, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	if (ret) {		
		//DBG("GPIO-%d gpio_tlmm_config failed: %d\n",sc668_drvdata.pwm_pin, ret);
		printk(KERN_ERR "%s: GPIO-%d gpio_tlmm_config failed: %d\n",  __func__, sc668_drvdata.pwm_pin, ret);		
		return ret;
	}
	//request gpio
	ret = gpio_request(sc668_drvdata.pwm_pin, "sc668_PWM_pin");
	if(ret) {
	  	dev_err(&client->dev ,"%s: GPIO-%d request failed!! <%d>\n", __func__, sc668_drvdata.pwm_pin, ret);
		return ret;
	}
	ret = gpio_direction_output(sc668_drvdata.pwm_pin, 1);
	if(ret) {
	  	//DBG("GPIO-%d gpio_direction_output failed: %d\n", sc668_drvdata.pwm_pin, ret);
		printk(KERN_ERR "%s: GPIO-%d gpio_direction_output failed: %d\n",  __func__, sc668_drvdata.pwm_pin, ret);	
		return ret;
	}
	gpio_free(sc668_drvdata.pwm_pin);
	//config gpio enable_pin
	gpio_tlmm_config(GPIO_CFG(sc668_drvdata.enable_pin, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	if (ret) {		
		//DBG("GPIO-%d gpio_tlmm_config failed: %d\n", sc668_drvdata.enable_pin, ret);
		printk(KERN_ERR "%s: GPIO-%d gpio_tlmm_config failed: %d\n",  __func__, sc668_drvdata.enable_pin, ret);		
		return ret;
	}
	//request gpio enable_pin
	ret = gpio_request(sc668_drvdata.enable_pin, "sc668_enable_pin");
	if(ret) {
	  	dev_err(&client->dev ,"%s: GPIO-%d request failed!! <%d>\n", __func__, sc668_drvdata.enable_pin, ret);
		return ret;
	}	
	
	//enable CHIP
	ret = sc668_enable_chip(SC668_ON, sc668_drvdata.sc668_i2c_client->name);//default
	
	if(ret <0)
	{
        //DBG("Enable sc668 fail!!!.\n");		
		printk(KERN_ERR "%s: Enable sc668 fail!\n",  __func__);
		mutex_destroy(&sc668_drvdata.sc668_lock);
	}
	if(ProductID > Project_DQD && ProductID < Project_FAD && PhaseID == Phase_PR1)//DPD and DP2 PR1
	{
		//WilsonWHLee added for Always enable USB PHY3.3++
		while(!sc668_get_chip_state( )){
			//DBG("wait sc688 enable");
			printk(KERN_INFO "wait sc688 enable\n"); 
		}

		ret = sc668_LDO_setting(3, LDO_VOLTAGE_3_3, sc668_drvdata.sc668_i2c_client->name);

		
		if (ret) {

			//DBG( "%s: LDO3 voltage setting failed (%d)\n",  __func__, ret);
			printk(KERN_ERR "%s: LDO3 voltage setting failed (%d)\n",  __func__, ret);
			return -EIO;

		}
		//WilsonWHLee added for Always enable USB PHY3.3++
	}
	if(ProductID > Project_DQD)
	{
	/* FIH, SimonSSChang TI WIFI */
		ret = sc668_LDO_setting(2, LDO2_VOLTAGE_1_8, sc668_drvdata.sc668_i2c_client->name);

		if (ret) {
			//DBG( "%s: LDO2 voltage setting failed (%d)\n",  __func__, ret);
			printk(KERN_ERR "%s: LDO2 voltage setting failed (%d)\n",  __func__, ret);
			return -EIO;
		}
		//enable WL_EN
		mdelay(50);
		printk(KERN_INFO "WIFI: pull down WL_EN pin 35\n");
		gpio_direction_output(35,0);    
		
		printk(KERN_INFO "BT: pull down BT_EN pin 27\n");    
		gpio_direction_output(27,0);        

	/* FIH, SimonSSChang TI WIFI */
	}
	return ret;
}

static int __devexit sc668_remove(struct i2c_client *client)
{
	int ret = 0;
	mutex_destroy(&sc668_drvdata.sc668_lock);
	

	return ret;
}

static const struct i2c_device_id sc668_idtable[] = {
       { "sc668", 0 },
       { }
};

static struct i2c_driver sc668_driver = {
	.driver = {
		.name	= "sc668",
	},
	.probe		= sc668_probe,
	.remove		= __devexit_p(sc668_remove),
	.suspend  	= sc668_suspend,
	.resume   	= sc668_resume,
	.id_table	= sc668_idtable,
};

static int __init sc668_init(void)
{
	int ret = 0;
	sc668_debug_mask = *(uint32_t *)SC668_DEBUG_MASK_OFFSET;
	printk( KERN_INFO "Driver init: %s\n", __func__ );
	ProductID = fih_read_product_id_from_orighwid();
	PhaseID = fih_read_phase_id_from_orighwid();	
	printk( KERN_INFO "[%s]PID : %d, PhaseID:%d\n", __func__, ProductID, PhaseID);

	switch (ProductID){
		case Project_DQE://0x1
			break;
		case Project_DMQ://0x2
			if(PhaseID <= Phase_PR1){
				printk(KERN_ERR "sc668 is not exist!!!");
				return -ENODEV;
			}
			break;
		case Project_DQD://0x3			
		case Project_DPD://0x4	
		case Project_DMP://0x5
		case Project_DP2://0x6
		case Project_FAD://0x7
			break;
		case Project_AI1S://0x8
		case Project_AI1D://0x9
			printk(KERN_ERR "sc668 is not exist!!!");
			return -ENODEV;
		case Project_DMO://0xA
		case Project_DMT://0xB
			break;
		default:
			printk(KERN_ERR "%s: unknown ProductID : %d\n", __func__, ProductID);
			printk(KERN_ERR "sc668 is not exist!!!");
			return -ENODEV;		
	}

	ret = i2c_add_driver(&sc668_driver);
	if (ret) {
		printk(KERN_ERR "%s: Driver registration failed, module not inserted.\n", __func__);
		goto driver_del;
	}


/*Use miscdev*/
	//register and allocate device, it would create an device node automatically.
	//use misc major number plus random minor number, and init device
	ret = misc_register(&sc668_miscdev);
	
	if (ret){
		DBG("sc668 register failed.\n");
		goto register_del;
	}

	//all successfully.
	return ret;

/*new label name for remove misc device*/
register_del:
	misc_deregister(&sc668_miscdev);


driver_del:
	i2c_del_driver(&sc668_driver);
	
	return -1;
}

static void __exit sc668_exit(void)
{
	printk( KERN_INFO "Driver exit: %s\n", __func__ );

	misc_deregister(&sc668_miscdev);
	i2c_del_driver(&sc668_driver);
}

module_init(sc668_init);
module_exit(sc668_exit);

MODULE_AUTHOR( "Nicole Weng <nicolecfweng@fihtdc.com>" );
MODULE_DESCRIPTION( "sc668 driver" );
MODULE_LICENSE( "GPL" );
