
/**
 * @file          tiwlan_supplement.c
 * @brief         TIWLAN Sdio supplicant code</b>
 */
#define DEBUG

#include <linux/module.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>

#include <mach/hardware.h>
#include <mach/gpio.h>

//extern void set_irq_flags(unsigned int irq, unsigned int flags);

int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn,
		unsigned addr, u8 in, u8* out);

unsigned char tiwlan_direct_readb(struct sdio_func *func, int func_no ,unsigned int addr,
		int *err_ret)
{
	int ret;
	unsigned char val;

	BUG_ON(!func);

	if (err_ret)
		*err_ret = 0;

	ret = mmc_io_rw_direct(func->card, 0, func_no, addr, 0, &val);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFF;
	}

	return val;
}
EXPORT_SYMBOL(tiwlan_direct_readb);

void tiwlan_direct_writeb(struct sdio_func *func, int func_no , unsigned char b, unsigned int addr, int *err_ret)
{
	int ret;

	BUG_ON(!func);

	ret = mmc_io_rw_direct(func->card, 1, func_no, addr, b, NULL);
	if (err_ret)
		*err_ret = ret;
}

EXPORT_SYMBOL(tiwlan_direct_writeb);

#define DEFAULT_FORCE_RESCAN_DELAY_POWER_OFF      200
#define DEFAULT_FORCE_RESCAN_DELAY_POWER_ON       100
static struct mmc_host * g_tiwlan_mmc_host=NULL; 
void tiwlan_force_presence_change(int tiwlan_power);
void tiwlan_set_mmc_host( struct mmc_host * tiwlan_mmc );

void tiwlan_force_presence_change(int tiwlan_power)
{
	pr_debug(KERN_DEBUG "%s: Enter ...g_tiwlan_mmc_host:%p\n",__func__,g_tiwlan_mmc_host);
	if ( g_tiwlan_mmc_host) {
		if (tiwlan_power) 		 
			mmc_detect_change( g_tiwlan_mmc_host , msecs_to_jiffies(DEFAULT_FORCE_RESCAN_DELAY_POWER_ON));
		else  
			mmc_detect_change( g_tiwlan_mmc_host , msecs_to_jiffies(DEFAULT_FORCE_RESCAN_DELAY_POWER_OFF));

	}
}

void tiwlan_set_mmc_host( struct mmc_host * tiwlan_mmc ) 
{
	pr_debug(KERN_DEBUG "%s: Enter ...tiwlan_mmc:%p\n",__func__,tiwlan_mmc);
	g_tiwlan_mmc_host = tiwlan_mmc;
}


void tiwlan_dmac_flush_range(const void *start, const void *end) {
	dmac_flush_range(start,end);
}
EXPORT_SYMBOL(tiwlan_dmac_flush_range);

void tiwlan_dmac_clean_range(const void *start, const void *end) {
	dmac_clean_range(start,end);
}
EXPORT_SYMBOL(tiwlan_dmac_clean_range);
EXPORT_SYMBOL(tiwlan_force_presence_change);
EXPORT_SYMBOL(tiwlan_set_mmc_host);

static int tiwlan_wifi_en  = 0 ;

int tiwlan_wifi_power(int on) {
	if (on) { 
		gpio_direction_output(35,1);
		tiwlan_wifi_en++;
	} else {
		gpio_direction_output(35,0);
		tiwlan_wifi_en=0;
	}
	return 0;
}
EXPORT_SYMBOL(tiwlan_wifi_power);

int tiwlan_get_wifi_en(void) { return tiwlan_wifi_en ;} 
EXPORT_SYMBOL(tiwlan_get_wifi_en);


static int __init tiwlan_supp_init(void)
{
	int ret=0;
	pr_debug(KERN_INFO "TIWLAN Supplement Done\n",__func__);
	return ret;
}

static void __exit tiwlan_supp_exit(void)
{
	return 0;
}

module_init(tiwlan_supp_init);
module_exit(tiwlan_supp_exit);
