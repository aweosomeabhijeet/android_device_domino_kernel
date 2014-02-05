/*
 * leds-btn-pmic.c - MSM PMIC LEDs driver.
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

static int ProductID;
static int PhaseID;

extern uint32_t Domino_LED_debug_mask;

static void msm_button_bl_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;

	fih_printk(Domino_LED_debug_mask, FIH_DEBUG_ZONE_G3, "[KPD_LED] value: %d\n", value);

	if(ProductID == Project_DMQ && PhaseID <= Phase_PR1)
		ret = pmic_set_led_intensity(LED_LCD, value / MAX_KEYPAD_BL_LEVEL);
	else
		ret = pmic_set_led_intensity(LED_KEYPAD, value / MAX_KEYPAD_BL_LEVEL);
	
	if (ret)
		dev_err(led_cdev->dev, "can't set keypad backlight\n");
}

static struct led_classdev msm_bt_bl_led = {
	.name			= "keyboard-backlight",
	.brightness_set		= msm_button_bl_led_set,
	.brightness		= LED_OFF,
};

static int btn_pmic_led_probe(struct platform_device *pdev)
{
	int rc;

	rc = led_classdev_register(&pdev->dev, &msm_bt_bl_led);
	if (rc) {
		dev_err(&pdev->dev, "unable to register led class driver\n");
		return rc;
	}
	msm_button_bl_led_set(&msm_bt_bl_led, LED_OFF);
	return rc;
}

static int __devexit btn_pmic_led_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&msm_bt_bl_led);

	return 0;
}

#ifdef CONFIG_PM
static int btn_pmic_led_suspend(struct platform_device *dev,
		pm_message_t state)
{
	led_classdev_suspend(&msm_bt_bl_led);

	return 0;
}

static int btn_pmic_led_resume(struct platform_device *dev)
{
	led_classdev_resume(&msm_bt_bl_led);

	return 0;
}
#else
#define btn_pmic_led_suspend NULL
#define btn_pmic_led_resume NULL
#endif

static struct platform_driver btn_pmic_led_driver = {
	.probe		= btn_pmic_led_probe,
	.remove		= __devexit_p(btn_pmic_led_remove),
	.suspend	= btn_pmic_led_suspend,
	.resume		= btn_pmic_led_resume,
	.driver		= {
		.name	= "button-leds",
		.owner	= THIS_MODULE,
	},
};

static int __init btn_pmic_led_init(void)
{
	printk( KERN_INFO "Driver init: %s\n", __func__ );
	ProductID = fih_read_product_id_from_orighwid();
	PhaseID = fih_read_phase_id_from_orighwid();
	printk( KERN_INFO "PID : %d, PhaseID:%d\n", ProductID, PhaseID);

	if(ProductID >= Project_DPD)
	{
		printk( KERN_ERR "Dominol+ series no keyboard LED!\n");
		return -1;
	}
	
	return platform_driver_register(&btn_pmic_led_driver);
}
module_init(btn_pmic_led_init);

static void __exit btn_pmic_led_exit(void)
{
	platform_driver_unregister(&btn_pmic_led_driver);
}
module_exit(btn_pmic_led_exit);

MODULE_DESCRIPTION("Button PMIC LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:button-leds");
