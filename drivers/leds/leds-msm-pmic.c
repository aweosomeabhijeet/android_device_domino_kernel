/*
 * leds-msm-pmic.c - MSM PMIC LEDs driver.
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
#include <mach/msm_smd.h>
#include <mach/pmic.h>
#include <linux/cmdbgapi.h>

#define MAX_KEYPAD_BL_LEVEL	16

static int Product_ID;
static int Phase_ID;

extern uint32_t Domino_LED_debug_mask;

static void msm_keypad_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;

	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G2, "[BTN_LED] value: %d\n", value);

	ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
	/* FIH, Neo Chen, 2010/11/19 { */
	/* Also turn on keyboard LED, because framework won't turn it on for no keyboard lid device*/
	ret = pmic_set_led_intensity(LED_LCD, value / MAX_KEYPAD_BL_LEVEL);
	/* FIH, Neo Chen, 2010/11/19 } */
	
	if (ret)
		dev_err(led_cdev->dev, "can't set button backlight\n");
}

static struct led_classdev msm_kp_bl_led = {
	.name			= "button-backlight",
	.brightness_set		= msm_keypad_bl_led_set,
	.brightness		= LED_OFF,
};

static int msm_pmic_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &msm_kp_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
	msm_keypad_bl_led_set(&msm_kp_bl_led, LED_OFF);
	return rc;
}

static int __devexit msm_pmic_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&msm_kp_bl_led);

	return 0;
}

#ifdef CONFIG_PM
static int msm_pmic_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	led_classdev_suspend(&msm_kp_bl_led);

	return 0;
}

static int msm_pmic_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&msm_kp_bl_led);

	return 0;
}
#else
#define msm_pmic_led_suspend NULL
#define msm_pmic_led_resume NULL
#endif

static struct platform_driver msm_pmic_led_driver = {
	.probe		= msm_pmic_led_probe,
	.remove		= __devexit_p(msm_pmic_led_remove),
	.suspend	= msm_pmic_led_suspend,
	.resume		= msm_pmic_led_resume,
	.driver		= {
		.name	= "pmic-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init msm_pmic_led_init(void)
{
	printk( KERN_INFO "Driver init: %s\n", __func__ );
	Product_ID = fih_read_product_id_from_orighwid();
	Phase_ID = fih_read_phase_id_from_orighwid();
	printk( KERN_INFO "PID : %d, PhaseID:%d\n", Product_ID, Phase_ID);

	if((Product_ID == Project_DMQ && Phase_ID <= Phase_PR1) || (Product_ID >= Project_AI1S && Product_ID <= Project_AI1D))
	{
		printk( KERN_INFO "sc668 is not exist! Use PMIC for LED\n");
		return platform_driver_register(&msm_pmic_led_driver);
	}
	else
	{
		printk( KERN_ERR "sc668 is existed! Use SC668 for LED\n");
		return -1;
	}
	
	return platform_driver_register(&msm_pmic_led_driver);
}
module_init(msm_pmic_led_init);

static void __exit msm_pmic_led_exit(void)
{
	platform_driver_unregister(&msm_pmic_led_driver);
}
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pmic-leds");
