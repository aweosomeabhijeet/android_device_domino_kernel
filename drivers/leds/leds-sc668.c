/*
 * leds-sc668.c - SC668 LEDs driver.
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <mach/sc668.h>
#include <mach/msm_smd.h>
#include <mach/gpio.h>
#include <linux/cmdbgapi.h>
#include <mach/pmic.h>

#define DEVICE_NAME "keypad-leds"
#define CHG_LED_CTRL 78
#define MAX_KEYPAD_BL_LEVEL	16

static int Product_ID;
static int Phase_ID;

static int flashOnMS=0;
static int flashOffMS=0;
int OnMS=0;
int OffMS=0;
int blink_enable=0;
int solid_light=0;

uint32_t Domino_LED_debug_mask = 0;
module_param_named(debug_mask, Domino_LED_debug_mask, uint, S_IRUGO | S_IWUSR | S_IWGRP);

int set_sc668_GROUP2_fade_effect(void)
{
	int ret;
	unsigned char data;
	unsigned char effectGrp1;
	
	data = sc668_read_register(EFFECT_RATE_SETTINGS, DEVICE_NAME);
	data = data << 5;
	effectGrp1 = data >> 5;
	
	ret = sc668_set_effect_rate(effectGrp1, BREATH_16_FADE_4, DEVICE_NAME); //set GROUP2 as BREATH_16_FADE_4
	
	return ret;
}

static void sc668_keypad_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{	
	int ret;
	
	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G0, "[BTN_LED]sc668_keypad_bl_led_set: %d\n", value);

	if(Product_ID <= Project_DQD)
	{
		/* Also turn on keyboard LED, because framework won't turn it on for no keyboard lid device*/
		ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
		if (ret)
			dev_err(led_cdev->dev, "can't set keyboard backlight\n");
		
		if(value)
		{
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_10_0, 0, NO_EFFECT, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			gpio_set_value(CHG_LED_CTRL, 0);
			fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]Q series Pull down GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
		}
		else
		{
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_0_0, 0, NO_EFFECT, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			gpio_set_value(CHG_LED_CTRL, 1);
			fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]Q series Pull up GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
		}
	}
	else if(Product_ID <= Project_FAD)//ProductID is Dominol+ series
	{
		if(value)
		{
			if(sc668_set_backLight_currentAndEffect(BANK_3, BACKLIGHT_CURRENT_10_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank3 keypad backlight!!\n", __func__);
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_10_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			if(!(Product_ID >= Project_DPD && Phase_ID <= Phase_PR1)) //Dominol+ series PR1 not to set CHG_LED_CTRL
			{
				gpio_set_value(CHG_LED_CTRL, 0);
				fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]PLUS series Pull down GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
			}
		}
		else
		{
			if(sc668_set_backLight_currentAndEffect(BANK_3, BACKLIGHT_CURRENT_0_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank3 keypad backlight!!\n", __func__);
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_0_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			if(!(Product_ID >= Project_DPD && Phase_ID <= Phase_PR1)) //Dominol+ series PR1 not to set CHG_LED_CTRL
			{
				gpio_set_value(CHG_LED_CTRL, 1);
				fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]PLUS series Pull up GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
			}
		}
	}
	else//ProductID is DTV series
	{
		if(value)
		{
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_10_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			gpio_set_value(CHG_LED_CTRL, 0);
			fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]Q series Pull down GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
		}
		else
		{
			if(sc668_set_backLight_currentAndEffect(BANK_4, BACKLIGHT_CURRENT_0_0, 0, ENABLE_FADE, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank4 keypad backlight!!\n", __func__);

			gpio_set_value(CHG_LED_CTRL, 1);
			fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G1, "[BTN_LED]Q series Pull up GPIO CHG_LED_CTRL:%d\n", CHG_LED_CTRL);
		}
	}
}

static ssize_t kpd_blink_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	int rc;

	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G0, "[NTF_LED]solid_light:%d, blink_enable:%d, flashOnMS:0x%x, flashOffMS:0x%x\n", solid_light, blink_enable, flashOnMS, flashOffMS);
	
	if(solid_light)
	{
		if(sc668_set_backLight_currentAndEffect(BANK_2, BACKLIGHT_CURRENT_10_0, 0, NO_EFFECT, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank2 notification LED!!\n", __func__);
	}
	else if(!blink_enable)
	{
		if(sc668_set_backLight_currentAndEffect(BANK_2, BACKLIGHT_CURRENT_0_0, 0, NO_EFFECT, DEVICE_NAME))
			printk(KERN_INFO "%s: can't set bank2 notification LED!!\n", __func__);
	}
	else
	{
		rc = sc668_set_effectTime_grp1(flashOffMS, flashOnMS, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			printk(KERN_ERR "%s: sc668_set_effectTime_grp1 failed!! <%d>", __func__, rc);
		}
		else{
			if(sc668_set_backLight_currentAndEffect(BANK_2, 0, BACKLIGHT_CURRENT_10_0, ENABLE_BLINK, DEVICE_NAME))
				printk(KERN_INFO "%s: can't set bank2 notification LED!!\n", __func__);
		}
	}

	return count;
}

static ssize_t blink_onMS_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	sscanf(buf, "%d\n", &OnMS);
	blink_enable = 1;

	if(OnMS <= 0 || OffMS <= 0) blink_enable = 0;
	
	if(OnMS <= 32) flashOnMS = TIME_32;
	else if(OnMS <= 64) flashOnMS = TIME_64;
	else if(OnMS <= 256) flashOnMS = TIME_256;
	else if(OnMS <= 512) flashOnMS = TIME_512;
	else if(OnMS <= 1024) flashOnMS = TIME_1024;
	else if(OnMS <= 2048) flashOnMS = TIME_2048;
	else if(OnMS <= 3072) flashOnMS = TIME_3072;
	else flashOnMS = TIME_4096;
	
	if(OnMS == 99999 && OffMS == 99999) solid_light = 1;
	else solid_light = 0;
	
	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G0, "[NTF_LED]OnMS: %d, solid_light: %d, blink_enable: %d\n", OnMS, solid_light, blink_enable);
	
	return count;
}

static ssize_t blink_offMS_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{	
	sscanf(buf, "%d\n", &OffMS);
	blink_enable = 1;

	if(OnMS <= 0 || OffMS <= 0) blink_enable = 0;
	
	if(OffMS <= 32) flashOffMS = TIME_32;
	else if(OffMS <= 64) flashOffMS = TIME_64;
	else if(OffMS <= 256) flashOffMS = TIME_256;
	else if(OffMS <= 512) flashOffMS = TIME_512;
	else if(OffMS <= 1024) flashOffMS = TIME_1024;
	else if(OffMS <= 2048) flashOffMS = TIME_2048;
	else if(OffMS <= 3072) flashOffMS = TIME_3072;
	else flashOffMS = TIME_4096;
	
	if(OnMS == 99999 && OffMS == 99999) solid_light = 1;
	else solid_light = 0;
	
	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G0, "[NTF_LED]OffMS: %d, solid_light: %d, blink_enable: %d\n", OffMS, solid_light, blink_enable);
	
	return count;
}

static struct led_classdev sc668_kp_bl_led = {
	.name			= "button-backlight",
	.brightness_set		= sc668_keypad_bl_led_set,
	.brightness		= LED_OFF,
};

DEVICE_ATTR(kpd_blink, 0666, NULL, kpd_blink_store);
DEVICE_ATTR(blink_onMS, 0666, NULL, blink_onMS_store);
DEVICE_ATTR(blink_offMS, 0666, NULL, blink_offMS_store);

static int sc668_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &sc668_kp_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "%s: unable to register led class driver <%d>\n", __func__, rc);
		return rc;
	}
	sc668_keypad_bl_led_set(&sc668_kp_bl_led, LED_OFF);
	
	rc = device_create_file(sc668_kp_bl_led.dev, &dev_attr_kpd_blink);
	if (rc < 0) {
		dev_err(sc668_kp_bl_led.dev, "%s: Create keypad LED attribute \"kpd_blink\" failed!! <%d>", __func__, rc);
	}
	rc = device_create_file(sc668_kp_bl_led.dev, &dev_attr_blink_onMS);
	if (rc < 0) {
		dev_err(sc668_kp_bl_led.dev, "%s: Create keypad LED attribute \"blink_onMS\" failed!! <%d>", __func__, rc);
	}
	rc = device_create_file(sc668_kp_bl_led.dev, &dev_attr_blink_offMS);
	if (rc < 0) {
		dev_err(sc668_kp_bl_led.dev, "%s: Create keypad LED attribute \"blink_offMS\" failed!! <%d>", __func__, rc);
	}

	if(Product_ID <= Project_DQD)
	{
		rc = sc668_set_light_assignments(BANK1_BL5_BL8_BANK2_BL3_BL4_BANK3_BL2_BANK4_BL1, GROUP1_BK1_BK2_GROUP2_BK3_BK4, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: sc668_set_light_assignments failed!! <%d>", __func__, rc);
		}

		rc = sc668_enable_backlights(BACKLIGHT_3, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: BACKLIGHT_3 enable failed!! <%d>", __func__, rc);
		}

		rc = sc668_enable_Banks(BANK_4_ENABLE, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: BANK_4_ENABLE failed!! <%d>", __func__, rc);
		}
	}
	else if(Product_ID <= Project_FAD)//ProductID is Dominol+ series
	{
		rc = sc668_set_light_assignments(BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1, GROUP1_BK1_BK2_GROUP2_BK3_BK4, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: sc668_set_light_assignments failed!! <%d>", __func__, rc);
		}
		
		rc = sc668_enable_Banks(BANK_3_ENABLE | BANK_4_ENABLE, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: BANK_3_ENABLE & BANK_4_ENABLE failed!! <%d>", __func__, rc);
		}
		
		if(!(Phase_ID <= Phase_PR1)) //Dominol+ series PR1 not to enable BANK2 which is NC
		{
			rc = sc668_enable_Banks(BANK_2_ENABLE, DEVICE_NAME);
			if (rc != SC668_SUCCESS) {
				dev_err(sc668_kp_bl_led.dev, "%s: BANK_2_ENABLE failed!! <%d>", __func__, rc);
			}
		}

		rc = set_sc668_GROUP2_fade_effect();
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: set_sc668_GROUP2_fade_effect failed!! <%d>", __func__, rc);
		}
	}
	else//ProductID is DTV series
	{
		rc = sc668_set_light_assignments(BANK1_BL4_BL8_BANK2_BL3_BANK3_BL2_BANK4_BL1, GROUP1_BK1_BK2_GROUP2_BK3_BK4, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: sc668_set_light_assignments failed!! <%d>", __func__, rc);
		}
		
		rc = sc668_enable_Banks(BANK_2_ENABLE | BANK_4_ENABLE, DEVICE_NAME);
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: BANK_2_ENABLE & BANK_4_ENABLE failed!! <%d>", __func__, rc);
		}
		
		rc = set_sc668_GROUP2_fade_effect();
		if (rc != SC668_SUCCESS) {
			dev_err(sc668_kp_bl_led.dev, "%s: set_sc668_GROUP2_fade_effect failed!! <%d>", __func__, rc);
		}
	}
	
	return rc;
}

static int __devexit sc668_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&sc668_kp_bl_led);

	return 0;
}

#ifdef CONFIG_PM
static int sc668_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	led_classdev_suspend(&sc668_kp_bl_led);

	return 0;
}

static int sc668_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&sc668_kp_bl_led);

	return 0;
}
#else
#define sc668_led_suspend NULL
#define sc668_led_resume NULL
#endif

static struct platform_driver sc668_led_driver = {
	.probe		= sc668_led_probe,
	.remove		= __devexit_p(sc668_led_remove),
	.suspend	= sc668_led_suspend,
	.resume		= sc668_led_resume,
	.driver		= {
		.name	= DEVICE_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init sc668_led_init(void)
{
	printk( KERN_INFO "Driver init: %s\n", __func__ );
	Product_ID = fih_read_product_id_from_orighwid();
	Phase_ID = fih_read_phase_id_from_orighwid();
	printk( KERN_INFO "PID : %d, PhaseID:%d\n", Product_ID, Phase_ID);
	
	Domino_LED_debug_mask = 0xFFFFFF; //  *(int*)NLED_DEBUG_MASK_OFFSET;

	if((Product_ID == Project_DMQ && Phase_ID <= Phase_PR1) || (Product_ID >= Project_AI1S && Product_ID <= Project_AI1D))
	{
		printk( KERN_ERR "sc668 is not exist!\n");
		return -1;
	}

	if(!(Product_ID >= Project_DPD && Phase_ID <= Phase_PR1) || (Product_ID >= Project_DMO)) //Some Dominol+ series PR1 not to set CHG_LED_CTRL
	{
		printk( KERN_INFO "Configure CHG_LED_CTRL GPIO: %d\n", CHG_LED_CTRL);
		gpio_tlmm_config(GPIO_CFG(CHG_LED_CTRL, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
		gpio_set_value(CHG_LED_CTRL, 1);
	}
	
	return platform_driver_register(&sc668_led_driver);
}
module_init(sc668_led_init);

static void __exit sc668_led_exit(void)
{
	platform_driver_unregister(&sc668_led_driver);
}
module_exit(sc668_led_exit);

MODULE_DESCRIPTION("SC668 Keypad LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:keypad-leds");
