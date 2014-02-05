
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <asm/gpio.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>

#include <mach/hardware.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <mach/gpio.h>

void rfkill_switch_all(enum rfkill_type type, enum rfkill_user_states state);

static struct rfkill *bt_rfk;
static const char bt_name[] = "wl1271";

//static int bluetooth_set_power(void *data, enum rfkill_user_states state)
static int bluetooth_toggle_radio(void *data, bool blocked)
{
	
	printk(KERN_INFO "%s: data:%p blocked:%d\n",__func__, data , blocked);

	switch ( blocked ) { 
	
		case RFKILL_USER_STATE_SOFT_BLOCKED:
							printk(KERN_INFO "%s: Turn On WL1271 BT_EN.\n",__func__);
                                                        gpio_direction_output(27,1);
							break;
		case RFKILL_USER_STATE_UNBLOCKED:
							printk(KERN_INFO "%s: Turn Off WL1271 BT_EN.\n",__func__);
                                                        gpio_direction_output(27,0);
							break;

	}

	return 0;
}



static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	        .set_block = bluetooth_toggle_radio,
};

static int tiwlan_rfkill_probe(struct platform_device *pdev)
{
	int ret;
	printk(KERN_INFO "%s:TIWLAN Bluetooth RFKILL Init\n",__func__);
  	bt_rfk = rfkill_alloc("wl1271_bt", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
                                &bluetooth_power_rfkill_ops,
			        pdev->dev.platform_data);


	 /* force Bluetooth off during init to allow for user control */
        rfkill_init_sw_state(bt_rfk, 1);

        ret = rfkill_register(bt_rfk);
        if (ret) {
                printk(KERN_DEBUG
                        "%s: rfkill register failed=%d\n", __func__,
                        ret);
                rfkill_destroy(bt_rfk);
                return ret;
        }

        platform_set_drvdata(pdev, bt_rfk);
	printk(KERN_INFO "%s:TIWLAN Bluetooth RFKILL Done\n",__func__);

	return 0;
}

static struct platform_driver tiwlan_rfkill_driver = {
	.probe = tiwlan_rfkill_probe,
	.driver = {
		.name = "tiwlan_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init tiwlan_rfkill_init(void)
{
	return platform_driver_register(&tiwlan_rfkill_driver);
}



module_init(tiwlan_rfkill_init);
