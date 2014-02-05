/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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
 *
 */

#include "msm_fb.h"
#include <mach/gpio.h>
#include <mach/msm_smd.h>

extern void lcdc_ili9338_panel_on(void);
extern void lcdc_ili9338_panel_off(void);
extern void lcdc_ili9481_panel_on(void);
extern void lcdc_ili9481_panel_off(void);
extern void lcdc_hx8368_panel_on(void);
extern void lcdc_hx8368_panel_off(void);

static int lcdc_panel_on(struct platform_device *pdev)
{
	int rc;
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcm_innolux: lcdc_panel_on()\n");
	
    rc=gpio_tlmm_config(GPIO_CFG(98, 1, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	if(rc){
		printk( KERN_ERR "%s():gpio lcdc_r5(%d) config failed.(%d), just ignore\n", __func__, 98, rc);
	}
    //chandler_boot_lcm
    if(Project_DQE <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_DQD)
	    lcdc_ili9338_panel_on();
	else if(Project_DPD <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_FAD)
	    lcdc_ili9481_panel_on();
	else
		lcdc_hx8368_panel_on();

	return 0;
}

static int lcdc_panel_off(struct platform_device *pdev)
{
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcm_innolux: lcdc_panel_off()\n");

    //chandler_boot_lcm
    if(Project_DQE <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_DQD)
	    lcdc_ili9338_panel_off();
	else if(Project_DPD <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_FAD)
	    lcdc_ili9481_panel_off();
	else
		lcdc_hx8368_panel_off();

	return 0;
}

static int __init lcdc_panel_probe(struct platform_device *pdev)
{
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcm_innolux: lcdc_panel_probe()\n");
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_panel",
	},
};

static struct msm_fb_panel_data lcdc_panel_data = {
	.on = lcdc_panel_on,
	.off = lcdc_panel_off,
};

static int lcdc_dev_id;

int lcdc_device_register(struct msm_panel_info *pinfo)
{
	struct platform_device *pdev = NULL;
	int ret;

	pdev = platform_device_alloc("lcdc_panel", ++lcdc_dev_id);
	if (!pdev)
		return -ENOMEM;

	lcdc_panel_data.panel_info = *pinfo;
	ret = platform_device_add_data(pdev, &lcdc_panel_data,
		sizeof(lcdc_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init lcdc_panel_init(void)
{
	return platform_driver_register(&this_driver);
}

module_init(lcdc_panel_init);
