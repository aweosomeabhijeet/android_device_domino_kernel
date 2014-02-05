#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/st1332_i2c.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <mach/gpio.h>
//Dynamic to load RES or CAP touch driver++
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
//Dynamic to load RES or CAP touch driver--
#include <mach/vreg.h>  //Add for VREG_WLAN power in, 07/08
#include <mach/7x27_kybd.h>  //Added for FST by Stanley (F0X_2.B-414)
/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#include <linux/earlysuspend.h>
/* } FIH, SimonSSChang, 2009/09/04 */
//Added by Stanley for dump scheme++
#include <linux/unistd.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
//Added by Stanley for dump scheme--
#include <linux/cmdbgapi.h>  //Added for debug mask (2010/01/04)
#include <linux/pm.h>  //Added for new kernel suspend scheme (2010.11.16)


/************************************************
 *  attribute marco
 ************************************************/
#define TOUCH_NAME		"st1332"	/* device name */
#define ST1332_DEBUG	1			/* print out receive packet */
#define KEYCODE_BROWSER 192  //Modified for Home and AP key (2009/07/31)
#define KEYCODE_SEARCH 217  //Modified for Home and AP key (2009/07/31)

/************************************************
 *  function marco
 ************************************************/
#define st1332_msg(lv, fmt, arg...)	\
	printk(KERN_ ##lv"[%s@%s@%d] "fmt"\n",__FILE__,__func__,__LINE__,##arg)

static struct proc_dir_entry *msm_touch_proc_file = NULL;  //Added by Stanley for dump scheme++
/************************************************
 *  Global variable
 ************************************************/
u8 st1332_buffer[8];
bool bST1332_Soft1CapKeyPressed = 0;	
bool bST1332_Soft2CapKeyPressed = 0;
bool bST1332_HomeCapKeyPressed = 0;
bool bST1332_BackCapKeyPressed = 0;
//bool bCenterKeyPressed = 0;  //Added for FST
//bool bIsF913 = 0;  //Modified for Home and AP key (2009/07/31)
//bool bIsGRECO = 0;  //Added for GRECO
//bool bIsFxxPR2 = 0;  //Added for PR2
bool bST1332_IsPenUp = 1;  //Added for touch behavior (2009/08/14)
bool bIsKeyLock = 0;  //Added for new behavior (2009/09/27)
bool bIsNewFWVer = 0;  //Add for detect firmware version
bool bPrintPenDown = 0;  //Added log for debugging
/* FIH, Henry Juang, 2009/11/20 ++*/
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
bool bIsNeedSkipTouchEvent = 0;  
/* FIH, Henry Juang, 2009/11/20 --*/
bool bPhoneCallState = 0;  //Added for FST by Stanley (F0X_2.B-414)
bool bIsNeedtoEnableIRQ = 0;  //Added to fix IRQ sync issue for F0X_2.B-414
int iX_max = 0, iY_max = 0;
u32 uST1332_VkeySoft1MiddleBoundary = 0, uST1332_VkeySoft2MiddleBoundary = 0,
    uST1332_VkeyHomeLeftBoundary = 0, uST1332_VkeyHomeRightBoundary = 0,
    uST1332_VkeyApLeftBoundary = 0, uST1332_VkeyApRightBoundary = 0;

#if 0
static unsigned int key_mapping[6]={
	KEY_SEND,
	KEY_END,
	KEY_BACK,	
	KEYCODE_SEARCH,
	KEY_HOME,
	KEY_KBDILLUMDOWN,
};
#endif

//Added for debug mask definitions++
/************************************************
 * Debug Definitions
 ************************************************/
enum {
	MSM_PM_DEBUG_COORDINATES = 1U << 0,
};

static uint32_t msm_cap_touch_debug_mask;
module_param_named(
    debug_mask, msm_cap_touch_debug_mask, uint, S_IRUGO | S_IWUSR | S_IWGRP
);  //Added for debug mask (2010/01/04)

//
static int msm_cap_touch_gpio_pull_fail;
module_param_named(
    gpio_pull, msm_cap_touch_gpio_pull_fail, int, S_IRUGO | S_IWUSR | S_IWGRP
);
//

//Added for show FW version on FQC++
static int msm_cap_touch_fw_version;
module_param_named(
    fw_version, msm_cap_touch_fw_version, int, S_IRUGO | S_IWUSR | S_IWGRP
);
//Added for show FW version on FQC--
//Added for debug mask definitions--

/************************************************
 * Control structure
 ************************************************/
struct st1332_m32emau {
	struct i2c_client *client;
	struct input_dev  *input;
	struct work_struct wqueue;
	struct completion data_ready;
	struct completion data_complete;
/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#ifdef CONFIG_FIH_FXX
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend st1332_early_suspend_desc;
#endif
#endif
/* } FIH, SimonSSChang, 2009/09/04 */    
} *st1332;

static int st1332_recv(u8 *buf, u32 count)
{
	struct i2c_adapter *adap = st1332->client->adapter;
    struct i2c_msg msgs[]= {
		[0] = {
			.addr = st1332->client->addr,
            .flags = I2C_M_RD,
            .len = count,
            .buf = buf,
		},
	};

	return (i2c_transfer(adap, msgs, 1) < 0) ? -EIO : 0;
}

static int st1332_send(u8 *buf, u32 count)
{
	struct i2c_adapter *adap = st1332->client->adapter;
	struct i2c_msg msgs[]= {
		[0] = {
            .addr = st1332->client->addr,
			.flags = 0,
			.len = count,
			.buf = buf,
        },
    };

	return (i2c_transfer(adap, msgs, 1) < 0) ? -EIO : 0;
}

#define FW_MAJOR(x) ((((int)((x)[0])) & 0xF0) >> 4)
#define FW_MINOR(x) ((((int)((x)[0])) & 0x0F))
static int st1332_get_fw_version(int *version)
{
	int major, minor;

//	printk(KERN_ERR "[DEBUG_MSG] %s\n", __FUNCTION__);

	disable_irq(st1332->client->irq);  //Added for protect st1332 read packet by Stanley

	st1332_buffer[0] = 0x0;		// Set address to 0x0 for Version.
	st1332_send(st1332_buffer, 1);	

	//st1332_recv(st1332_buffer, 1);	// Read Firmware Version.
	//Added for protect st1332 read packet by Stanley++
	if (st1332_recv(st1332_buffer, 1) < 0) {
        st1332_msg(ERR, "st1332_get_fw_version failed");
        enable_irq(st1332->client->irq);
        return -1;
    }
    //Added for protect st1332 read packet by Stanley--

	major = FW_MAJOR(st1332_buffer);
	minor = FW_MINOR(st1332_buffer);
	*version = major * 100 + minor;

	st1332_buffer[0] = 0x10;
	st1332_send(st1332_buffer, 1);		// Set address back to 0x10 for coordinates.

	enable_irq(st1332->client->irq);  //Added for protect st1332 read packet by Stanley

	return 0;
}

static int st1332_get_pw_state(int *state)
{
	printk(KERN_ERR "[DEBUG_MSG] %s\n", __FUNCTION__);
	
	//disable_irq(st1332->client->irq);

	st1332_buffer[0] = 0x02;		// Set address to 0x02 for Device Control Reg.
	//st1332_send(st1332_buffer, 1);
	//Added for new kernel suspend scheme (2010.11.16)++
	if (st1332_send(st1332_buffer, 1) < 0) {
        st1332_msg(ERR, "get_power_mode failed");
        return -1;
    }
    //Added for new kernel suspend scheme (2010.11.16)--

	st1332_recv(st1332_buffer, 1);	// Read Device Control Reg.

	*state = (st1332_buffer[0] & 0x02) >> 1;

	st1332_buffer[0] = 0x10;		// Set address back to 0x10 for coordinates.
	//st1332_send(st1332_buffer, 1);
	//Added for new kernel suspend scheme (2010.11.16)++
	if (st1332_send(st1332_buffer, 1) < 0) {
        st1332_msg(ERR, "get_power_mode failed (Set address back to 0x10 for coordinates)");
        return -1;
    }
    //Added for new kernel suspend scheme (2010.11.16)--

	//enable_irq(st1332->client->irq);

	return 0;
}

static int st1332_set_pw_state(unsigned state)
{

	printk(KERN_ERR "[DEBUG_MSG] %s\n", __FUNCTION__);
		
	//disable_irq(st1332->client->irq);

	st1332_buffer[0] = 0x02;
	st1332_buffer[1] = 0x00 | (state << 1);
	//st1332_send(st1332_buffer, 2);
	//Added for new kernel suspend scheme (2010.11.16)++
	if (st1332_send(st1332_buffer, 2) < 0) {
        st1332_msg(ERR, "set_power_mode failed");
        return -1;
    }
    else
        st1332_msg(ERR, "set_power_mode successful!");
    //Added for new kernel suspend scheme (2010.11.16)--

	st1332_buffer[0] = 0x10;		// Set address back to 0x10 for coordinates.
	//st1332_send(st1332_buffer, 1);
	//Added for new kernel suspend scheme (2010.11.16)++
	if (st1332_send(st1332_buffer, 1) < 0) {
        st1332_msg(ERR, "set_power_mode failed (Set address back to 0x10 for coordinates)");
        return -1;
    }
    //Added for new kernel suspend scheme (2010.11.16)--

	//enable_irq(st1332->client->irq);

	return 0;
}

static int st1332_get_orientation(int *mode)
{
#if 0
	char cmd[4] = { 0x53, 0xB1, 0x00, 0x01 };

	st1332_send(cmd, ARRAY_SIZE(cmd));
	wait_for_completion(&st1332->data_ready);

	if (st1332_buffer[0] != 0x52) {
        complete(&st1332->data_complete);
        return -1;
    }

	*mode = st1332_buffer[2] & 0x03;
	complete(&st1332->data_complete);
#endif //0

	return 0;
}

static int st1332_set_orientation(unsigned mode)
{
#if 0
	char cmd[4] = { 0x54, 0xB1, 0x00, 0x01 };

	if (mode > 3) mode = 3;
	cmd[2] |= (char)mode;

	if (st1332_send(cmd, ARRAY_SIZE(cmd)) < 0) {
		st1332_msg(ERR, "set orientation failed");
		return -1;
	}
	msleep(250);
#endif //0

	return 0;
}

static int st1332_get_resolution(struct st1332_i2c_resolution *res)
{
	printk(KERN_ERR "[DEBUG_MSG] %s\n", __FUNCTION__);

	//disable_irq(st1332->client->irq);

	st1332_buffer[0] = 0x04;
	st1332_send(st1332_buffer, 1);		// Set address to 0x04 for XY Resolution Reg.
	st1332_recv(st1332_buffer, 3);	// Read XY Resolution Reg.


	res->x = ((st1332_buffer[0] & 0xF0) << 4) | st1332_buffer[1];
	res->y = ((st1332_buffer[0] & 0x0F) << 8) | st1332_buffer[2];

	st1332_buffer[0] = 0x10;
	st1332_send(st1332_buffer, 1);		// Set address back to 0x10 for coordinates.

	//enable_irq(st1332->client->irq);

	return 0;
}

#if 0
static int st1332_set_resolution(unsigned mode)
{
	char cmd[4] = { 0x54, 0x82, 0x80, 0x01 };

	if (mode > 2) mode = 2;
	cmd[2] |= ((char)mode << 4);

	if (st1332_send(cmd, ARRAY_SIZE(cmd)) < 0) {
        st1332_msg(ERR, "set resolution type failed");
        return -1;
    }

	return 0;
}

static int st1332_get_deepsleep_mode(int *mode)
{
	char cmd[4] = { 0x53, 0xC0, 0x00, 0x01 };

	st1332_send(cmd, ARRAY_SIZE(cmd));
	wait_for_completion(&st1332->data_ready);

	if (st1332_buffer[0] != 0x52) {
        complete(&st1332->data_complete);
        return -1;
    }

	*mode = (st1332_buffer[1] & 0x08) >> 3;
	complete(&st1332->data_complete);

	return 0;
}

static int st1332_set_deepsleep_mode(unsigned mode)
{
	char cmd[4] = { 0x54, 0xC0, 0x00, 0x01 };

	mode = mode ? 1 : 0;
	cmd[1] |= ((char)mode << 3);

	if (st1332_send(cmd, ARRAY_SIZE(cmd)) < 0) {
        st1332_msg(ERR, "set deepsleep function failed");
        return -1;
    }

    return 0;
}
#endif

#define FWID_HBYTE(x) (((((int)(x)[1]) & 0xF) << 4) + ((((int)(x)[2]) & 0xF0) >> 4))
#define FWID_LBYTE(x) (((((int)(x)[2]) & 0xF) << 4) + ((((int)(x)[3]) & 0xF0) >> 4))
static int st1332_get_fw_id(int *id)
{
#if 0
	char cmd[4] = { 0x53, 0xF0, 0x00, 0x01 };

	st1332_send(cmd, ARRAY_SIZE(cmd));
	wait_for_completion(&st1332->data_ready);

	if (st1332_buffer[0] != 0x52) {
        complete(&st1332->data_complete);
        return -1;
    }

	*id = (FWID_HBYTE(st1332_buffer) << 8) + FWID_LBYTE(st1332_buffer);
	complete(&st1332->data_complete);
#endif //0

	return 0;
}

#if 0
static int st1332_get_report_rate(int *rate)
{
	char cmd[4] = { 0x53, 0xB0, 0x00, 0x01 };

	st1332_send(cmd, ARRAY_SIZE(cmd));
	wait_for_completion(&st1332->data_ready);

	if (res[0] != 0x52) {
        complete(&st1332->data_complete);
        return -1;
    }

	*rate = st1332_buffer[1] & 0xF;
	complete(&st1332->data_complete);

	return 0;
}

static int st1332_set_report_rate(unsigned rate)
{
	char cmd[4] = { 0x54, 0xE0, 0x00, 0x01 };

	if (rate > 5) rate = 5;
	cmd[1] |= (char)rate;

	if (st1332_send(cmd, ARRAY_SIZE(cmd)) < 0) {
        st1332_msg(ERR, "set report rate failed");
        return -1;
    }

    return 0;
}
#endif

static int st1332_get_sensitivity(struct st1332_i2c_sensitivity *sen)
{
#if 0
	char cmd[4] = { 0x53, 0x40, 0x00, 0x01 };

	st1332_send(cmd, ARRAY_SIZE(cmd));
	wait_for_completion(&st1332->data_ready);

	if (st1332_buffer[0] != 0x52) {
		complete(&st1332->data_complete);
		return -1;
	}

	sen->x = st1332_buffer[2] & 0x0F;
	sen->y = (st1332_buffer[2] & 0xF0) >> 4;
	complete(&st1332->data_complete);
#endif //0

	return 0;
}

static int st1332_set_sensitivity(struct st1332_i2c_sensitivity *sen)
{
#if 0
	char cmd[4] = { 0x54, 0x40, 0x00, 0xA1 };  //Modified for new FW V1.1

	if (sen->x > 0x0F) sen->x = 0x0F;
	if (sen->y > 0x0F) sen->y = 0x0F;

	cmd[2] = sen->x | (sen->y << 4);
	if (st1332_send(cmd, ARRAY_SIZE(cmd)) < 0) {
		st1332_msg(ERR, "set sensitivity failed");
		return -1;
	}
#endif //0

	return 0;
}

/* FIH, Henry Juang, 2009/11/20 ++*/
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
int notify_from_proximity(bool bFlag)
{
	bIsNeedSkipTouchEvent = bFlag;
	st1332_msg(ERR, "[TOUCH]notify_from_proximity = %d\r\n", bIsNeedSkipTouchEvent);
	printk("[TOUCH]notify_from_proximity = %d\r\n", bIsNeedSkipTouchEvent);
    
	return 1;
}
EXPORT_SYMBOL(notify_from_proximity);
/* FIH, Henry Juang, 2009/11/20 --*/

#define XCORD1(x) ((((int)((x)[1]) & 0xF0) << 4) + ((int)((x)[2])))
#define YCORD1(y) ((((int)((y)[1]) & 0x0F) << 8) + ((int)((y)[3])))
#define XCORD2(x) ((((int)((x)[4]) & 0xF0) << 4) + ((int)((x)[5])))
#define YCORD2(y) ((((int)((y)[4]) & 0x0F) << 8) + ((int)((y)[6])))

#define KEY_SIMULATE	0
static void st1332_isr_workqueue(struct work_struct *work)
{
	struct input_dev *input = st1332->input;
//	int cnt, virtual_button;  //Modified for new CAP sample by Stanley (2009/05/25)
	stx_report_data_t *pdata;
	static u16 x0=0, y0=0, x1=0, y1=0;
	//static u8 keys_pre_status=0;
	//int i;
	
	disable_irq(st1332->client->irq);
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	pdata = (stx_report_data_t *) st1332_buffer;
	/** For multi-touch event **/
	if (pdata->fingers) {
	    	
		if (pdata->xy_data[0].valid) {
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 255);
			//input_report_key(input, BTN_TOUCH, 1);
			x0 = pdata->xy_data[0].x_h << 8 | pdata->xy_data[0].x_l;
			y0 = pdata->xy_data[0].y_h << 8 | pdata->xy_data[0].y_l;
			/**/
			//map_x=319-y0;
			//map_y=x0;
			//x0=map_x;
			//y0=map_y;
			/* elimate redunt down touch event*/
            if (y0 <= iY_max)  //Added for SK scheme by Stanley, 2010/10/27
            {
			    input_report_abs(input, ABS_MT_POSITION_X, x0);
			    input_report_abs(input, ABS_MT_POSITION_Y, y0);
			
			//input_report_abs(input, ABS_X, x0);
			//input_report_abs(input, ABS_Y, y0);
			    //printk(KERN_ERR "[ST1332] [down]X0: %u, Y0: %u, Key:%u, MAJOR(255)\n", x0, y0, pdata->keys);
			    fih_printk(msm_cap_touch_debug_mask, FIH_DEBUG_ZONE_G1, "[ST1332] [down]X0: %u, Y0: %u, Key:%u, MAJOR(255)\n", x0, y0, pdata->keys);  //Added debug_mask for ST1332
			    input_mt_sync(input);
			}
		}
		
		if (pdata->xy_data[1].valid) {
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 255);
			//input_report_key(input, BTN_2, 1);
			x1 = pdata->xy_data[1].x_h << 8 | pdata->xy_data[1].x_l;
			y1 = pdata->xy_data[1].y_h << 8 | pdata->xy_data[1].y_l;
			/**/
			//map_x=319-y1;
			//map_y=x1;
			//x1=map_x;
			//y1=map_y;
			/**/
            if (y0 <= iY_max)  //Added for SK scheme by Stanley, 2010/10/27
            {
			    input_report_abs(input, ABS_MT_POSITION_X, x1);
			    input_report_abs(input, ABS_MT_POSITION_Y, y1);
			
			    //input_report_abs(input, ABS_X, x1);
			    //input_report_abs(input, ABS_Y, y1);
			    //printk(KERN_ERR "[ST1332] [down]X1: %u, Y1: %u, Key:%u, MAJOR(255)\n", x1, y1, pdata->keys);
			    fih_printk(msm_cap_touch_debug_mask, FIH_DEBUG_ZONE_G1, "[ST1332] [down]X1: %u, Y1: %u, Key:%u, MAJOR(255)\n", x1, y1, pdata->keys);  //Added debug_mask for ST1332
			    input_mt_sync(input);
			}
		}
        //Added for touch behavior (2011/02/21)++
        bST1332_IsPenUp = 0;
        //Added for FW version lower than V5.0++
        if (y0 > (iY_max + 5))
        {
            bST1332_IsPenUp = 1;
        }
        //Added for FW version lower than V5.0--
        //Added for touch behavior (2011/02/21)--
	//}else{
    }
    else if ((pdata->fingers == 0) && !bST1332_Soft1CapKeyPressed && !bST1332_Soft2CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed){

		if(!pdata->xy_data[0].valid){
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(input, ABS_MT_POSITION_X, x0);
			input_report_abs(input, ABS_MT_POSITION_Y, y0);
			//input_report_key(input, BTN_TOUCH, 0);
			//printk(KERN_ERR "[ST1332] [up] X0: %u, Y0: %u, Key:%u, MAJOR(0)\n", x0, y0, pdata->keys);
			fih_printk(msm_cap_touch_debug_mask, FIH_DEBUG_ZONE_G1, "[ST1332] [up] X0: %u, Y0: %u, Key:%u, MAJOR(0)\n", x0, y0, pdata->keys);  //Added debug_mask for ST1332
			input_mt_sync(input);				
		}

		if(!pdata->xy_data[1].valid){
			input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(input, ABS_MT_POSITION_X, x1);
			input_report_abs(input, ABS_MT_POSITION_Y, y1);
			//input_report_key(input, BTN_2, 0);
			//printk(KERN_ERR "[ST1332] [up] X1: %u, Y1: %u, Key:%u, MAJOR(0)\n", x1, y1, pdata->keys);
			fih_printk(msm_cap_touch_debug_mask, FIH_DEBUG_ZONE_G1, "[ST1332] [up] X1: %u, Y1: %u, Key:%u, MAJOR(0)\n", x1, y1, pdata->keys);  //Added debug_mask for ST1332
			input_mt_sync(input);						
		}
//		pdata->keys=0;
        //Added for touch behavior (2011/02/21)++
        bST1332_IsPenUp = 1;
        //Added for touch behavior (2011/02/21)--
	}
	
#if 0
	/** For soft key event **/
	if(keys_pre_status != pdata->keys){
		printk(KERN_INFO "[ST1332] [KEY] (%u)\n", pdata->keys);
		for(i=0; i< 6; i++){
			input_report_key(input, key_mapping[i], pdata->keys & ((0x01)<<i));
		}
	}	
	keys_pre_status=pdata->keys;
#endif

    //FIH, Added for DMQ by Stanley, 2010/10/13++
    //FIH, Added the SK new scheme for PR2 by Stanley, 2010/12/28++
#if 1
    if(msm_cap_touch_fw_version >= 500)
    {
	    if(pdata->keys == 1 && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_KBDILLUMDOWN, 1);
            st1332_msg(INFO, "[ST1332][independent key]virtual button SOFT1 - down!\r\n");
            bST1332_Soft1CapKeyPressed = 1;
        }
        else if(pdata->keys == 2 && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_HOME, 1);
            st1332_msg(INFO, "[ST1332][independent key]virtual button HOME - down!\r\n");
            bST1332_HomeCapKeyPressed = 1;
        }
        else if(pdata->keys == 4 && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_BACK, 1);
            st1332_msg(INFO, "[ST1332][independent key]virtual button BACK - down!\r\n");
            bST1332_BackCapKeyPressed = 1;
        }
        else if(pdata->keys == 8 && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_SEARCH, 1);
            st1332_msg(INFO, "[ST1332][independent key]virtual button SOFT2 - down!\r\n");
            bST1332_Soft2CapKeyPressed = 1;
        }
    
        else if ((pdata->keys == 0) && (bST1332_Soft1CapKeyPressed || bST1332_Soft2CapKeyPressed || bST1332_HomeCapKeyPressed || bST1332_BackCapKeyPressed))
        {
            //st1332_msg(INFO, "[ST1332]X0: %u, Y0: %u - key up!\r\n", x0, y0);
            //Added for touch behavior (2011/02/21)++
            if(!bST1332_IsPenUp)
            {
                //input_report_key(input, BTN_TOUCH, 0);
                //Added for F0XE.B-346++
                input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
                input_mt_sync(input);
                //Added for F0XE.B-346--
                bST1332_IsPenUp = 1;
                st1332_msg(INFO, "[ST1332]Send BTN touch - up!\r\n");
                //bIsKeyLock = 1;  //Added for new behavior (2009/09/27)
            }
            //Added for touch behavior (2011/02/21)--
            if(bST1332_Soft1CapKeyPressed)
            {
                input_report_key(input, KEY_KBDILLUMDOWN, 0);
                st1332_msg(INFO, "[ST1332]virtual button SOFT1 - up!\r\n");
                bST1332_Soft1CapKeyPressed = 0;
                bST1332_Soft2CapKeyPressed = 0;
                bST1332_HomeCapKeyPressed = 0;
                bST1332_BackCapKeyPressed = 0;
            }
            else if(bST1332_HomeCapKeyPressed)
            {
                input_report_key(input, KEY_HOME, 0);
                st1332_msg(INFO, "[ST1332]virtual button HOME - up!\r\n");
                bST1332_Soft1CapKeyPressed = 0;
                bST1332_Soft2CapKeyPressed = 0;
                bST1332_HomeCapKeyPressed = 0;
                bST1332_BackCapKeyPressed = 0;
            }
            else if(bST1332_BackCapKeyPressed)
            {
                input_report_key(input, KEY_BACK, 0);
                st1332_msg(INFO, "[ST1332]virtual button BACK - up!\r\n");
                bST1332_Soft1CapKeyPressed = 0;
                bST1332_Soft2CapKeyPressed = 0;
                bST1332_HomeCapKeyPressed = 0;
                bST1332_BackCapKeyPressed = 0;
            }
            else if(bST1332_Soft2CapKeyPressed)
            {
                input_report_key(input, KEY_SEARCH, 0);
                st1332_msg(INFO, "[ST1332]virtual button SOFT2 - up!\r\n");
                bST1332_Soft1CapKeyPressed = 0;
                bST1332_Soft2CapKeyPressed = 0;
                bST1332_HomeCapKeyPressed = 0;
                bST1332_BackCapKeyPressed = 0;
            }   
        }
    }
#endif
    //FIH, Added the SK new scheme for PR2 by Stanley, 2010/12/28--
    //Added SK new scheme for DMP (2010/10/25)++
    if(msm_cap_touch_fw_version < 500){  //FIH, Added the SK new scheme for PR2 by Stanley, 2010/12/28
    if((y0 > (iY_max + 5)) && !bST1332_Soft1CapKeyPressed && !bST1332_Soft2CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed)  //Modified by Stanley 
    {
        st1332_msg(INFO, "[ST1332]X0: %u, Y0: %u - key down!\r\n", x0, y0);
        if ((x0 < uST1332_VkeySoft1MiddleBoundary) && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_KBDILLUMDOWN, 1);
            st1332_msg(INFO, "[ST1332]virtual button SOFT1 - down!\r\n");
            bST1332_Soft1CapKeyPressed = 1;
        }
        else if ((x0 > uST1332_VkeySoft2MiddleBoundary) && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_SEARCH, 1);
            st1332_msg(INFO, "[ST1332]virtual button SOFT2 - down!\r\n");
            bST1332_Soft2CapKeyPressed = 1;
        }
        else if ((x0 >= uST1332_VkeyHomeLeftBoundary) && (x0 <= uST1332_VkeyHomeRightBoundary) && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_HOME, 1);
            st1332_msg(INFO, "[ST1332]virtual button HOME - down!\r\n");
            bST1332_HomeCapKeyPressed = 1;
        }
        else if ((x0 >= uST1332_VkeyApLeftBoundary) && (x0 <= uST1332_VkeyApRightBoundary) && !bST1332_Soft1CapKeyPressed && !bST1332_HomeCapKeyPressed && !bST1332_BackCapKeyPressed && !bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_BACK, 1);
            st1332_msg(INFO, "[ST1332]virtual button BACK - down!\r\n");
            bST1332_BackCapKeyPressed = 1;            
        }
    }
    //Added SK new scheme for DMP (2010/10/25)--

    else if ((pdata->fingers == 0) && (bST1332_Soft1CapKeyPressed || bST1332_Soft2CapKeyPressed || bST1332_HomeCapKeyPressed || bST1332_BackCapKeyPressed))
    {
        st1332_msg(INFO, "[ST1332]X0: %u, Y0: %u - key up!\r\n", x0, y0);
        //Added for touch behavior (2011/02/21)++
        if(!bST1332_IsPenUp)
        {
            //input_report_key(input, BTN_TOUCH, 0);
            //Added for F0XE.B-346++
            input_report_abs(input, ABS_MT_TOUCH_MAJOR, 0);
            input_mt_sync(input);
            //Added for F0XE.B-346--
            bST1332_IsPenUp = 1;
            st1332_msg(INFO, "[ST1332]Send BTN touch - up!\r\n");
            //bIsKeyLock = 1;  //Added for new behavior (2009/09/27)
        }
        //Added for touch behavior (2011/02/21)--
        if(bST1332_Soft1CapKeyPressed)
        {
            input_report_key(input, KEY_KBDILLUMDOWN, 0);
            st1332_msg(INFO, "[ST1332]virtual button SOFT1 - up!\r\n");
            bST1332_Soft1CapKeyPressed = 0;
            bST1332_Soft2CapKeyPressed = 0;
            bST1332_HomeCapKeyPressed = 0;
            bST1332_BackCapKeyPressed = 0;
        }
        else if(bST1332_HomeCapKeyPressed)
        {
            input_report_key(input, KEY_HOME, 0);
            st1332_msg(INFO, "[ST1332]virtual button HOME - up!\r\n");
            bST1332_Soft1CapKeyPressed = 0;
            bST1332_Soft2CapKeyPressed = 0;
            bST1332_HomeCapKeyPressed = 0;
            bST1332_BackCapKeyPressed = 0;
        }
        else if(bST1332_BackCapKeyPressed)
        {
            input_report_key(input, KEY_BACK, 0);
            st1332_msg(INFO, "[ST1332]virtual button BACK - up!\r\n");
            bST1332_Soft1CapKeyPressed = 0;
            bST1332_Soft2CapKeyPressed = 0;
            bST1332_HomeCapKeyPressed = 0;
            bST1332_BackCapKeyPressed = 0;
        }
        else if(bST1332_Soft2CapKeyPressed)
        {
            input_report_key(input, KEY_SEARCH, 0);
            st1332_msg(INFO, "[ST1332]virtual button SOFT2 - up!\r\n");
            bST1332_Soft1CapKeyPressed = 0;
            bST1332_Soft2CapKeyPressed = 0;
            bST1332_HomeCapKeyPressed = 0;
            bST1332_BackCapKeyPressed = 0;
        }
    }
    }
    //FIH, Added for DMQ by Stanley, 2010/10/13--
		
	input_sync(input);

	gpio_clear_detect_status(st1332->client->irq);  
	enable_irq(st1332->client->irq);
}

static irqreturn_t st1332_isr(int irq, void * dev_id)
{
	//disable_irq(irq);
	schedule_work(&st1332->wqueue);
//	printk(KERN_ERR "[DEBUG_MSG] st1332_isr!\n");

	return IRQ_HANDLED;
}

static int input_open(struct input_dev * idev)
{
	struct i2c_client *client = st1332->client;

    if (request_irq(client->irq, st1332_isr, 2, TOUCH_NAME, st1332)) {
        st1332_msg(ERR, "can not register irq %d", client->irq);
		return -1;
    }

	return 0;
}

static void input_close(struct input_dev *idev)
{
	struct i2c_client *client = st1332->client;
	
	free_irq(client->irq, st1332);
}

static int st1332_misc_open(struct inode *inode, struct file *file)
{
    if ((file->f_flags & O_ACCMODE) == O_WRONLY) {
		st1332_msg(INFO, "device node is readonly");
        return -1;
    }

	return 0;
}

static int st1332_misc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int st1332_misc_ioctl(struct inode *inode, struct file *file,
									unsigned cmd, unsigned long arg)
{
	int value=0;
	int ret = 0;
	struct st1332_i2c_resolution res;
	struct st1332_i2c_sensitivity sen;

	if (_IOC_TYPE(cmd) != ST1332_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > ST1332_IOC_MAXNR) return -ENOTTY;

	switch(cmd) {
	case ST1332_IOC_GFWVERSION:
		if (st1332_get_fw_version(&value) < 0)
			return -EIO;
		ret = put_user(value, (int __user *)arg);
		break;
	case ST1332_IOC_GPWSTATE:
		if (st1332_get_pw_state(&value) < 0)
			return -EIO;
		ret = put_user(value, (int __user *)arg);
		break;
	case ST1332_IOC_GORIENTATION:
		if (st1332_get_orientation(&value) < 0)
			return -EIO;
		ret = put_user(value, (int __user *)arg);
		break;
	case ST1332_IOC_GRESOLUTION:
		if (st1332_get_resolution(&res) < 0)
			return -EIO;
		if(copy_to_user((int __user*)arg, &res,
			sizeof(struct st1332_i2c_resolution)))
			ret = -EFAULT;
		break;
#if 0
	case ST1332_IOC_GDEEPSLEEP:
		if (st1332_get_deepsleep_mode(&value) < 0)
            return -EIO;
        ret = put_user(value, (int __user *)arg);
        break;
#endif
	case ST1332_IOC_GFWID:
		if (st1332_get_fw_id(&value) < 0)
			return -EIO;
		ret = put_user(value, (int __user *)arg);
        break;
#if 0
	case ST1332_IOC_GREPORTRATE:
		if (st1332_get_report_rate(&value) < 0)
			return -EIO;
		ret = put_user(value, (int __user *)arg);
        break;
#endif
	case ST1332_IOC_GSENSITIVITY:
		if (st1332_get_sensitivity(&sen) < 0)
			return -EIO;
		if(copy_to_user((int __user*)arg, &sen,
			sizeof(struct st1332_i2c_sensitivity)))
			ret = -EFAULT;
		break;
	case ST1332_IOC_SPWSTATE:
		ret = get_user(value, (int __user *)arg);
		if (ret == 0)
			ret = st1332_set_pw_state(value);
		break;
	case ST1332_IOC_SORIENTATION:
		ret = get_user(value, (int __user *)arg);
		if (ret == 0)
			ret = st1332_set_orientation(value);
		break;
#if 0
	case ST1332_IOC_SRESOLUTION:
		ret = get_user(value, (int __user *)arg);
		if (ret == 0)
            ret = st1332_set_resolution(value);
		break;
	case ST1332_IOC_SDEEPSLEEP:
		ret = get_user(value, (int __user *)arg);
		if (ret == 0)
			ret = st1332_set_deepsleep_mode(value);
		break;
	case ST1332_IOC_SREPORTRATE:
		ret = get_user(value, (int __user *)arg);
		if (ret == 0)
			ret = st1332_set_report_rate(value);
		break;
#endif
	case ST1332_IOC_SSENSITIVITY:
		if (copy_from_user((int __user *)arg, &sen,
				sizeof(struct st1332_i2c_sensitivity))) {
			ret = -EFAULT;
			break;
		}
		ret = st1332_set_sensitivity(&sen);
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static struct file_operations st1332_misc_fops = {
    .open	= st1332_misc_open,
    .ioctl	= st1332_misc_ioctl,
    .release= st1332_misc_release,
};

static struct miscdevice st1332_misc_dev = {
    .minor= MISC_DYNAMIC_MINOR,
    .name = TOUCH_NAME,
	.fops = &st1332_misc_fops,
};

#ifdef CONFIG_PM
//static int st1332_suspend(struct i2c_client *client, pm_message_t state)
static int st1332_suspend(struct device *client)  //Added for new kernel suspend scheme (2010.11.16)
{
    //struct vreg *vreg_wlan;  //Add for VREG_WLAN power in, 07/08
    struct st1332_i2c_platform_data *pdata = st1332->client->dev.platform_data;  //Added for new kernel suspend scheme (2010.11.16)
    int ret_gpio = 0;  //Add for VREG_WLAN power in, 07/08

	cancel_work_sync(&st1332->wqueue);
    printk(KERN_INFO "st1332_suspend() disable IRQ: %d\n", st1332->client->irq);
    //disable_irq(client->irq);
    disable_irq(st1332->client->irq);  //Added for new kernel suspend scheme (2010.11.16)

	st1332_set_pw_state(1);

	//Setting the configuration of GPIO 23 (2010.11.23)++	
	//ret_gpio = gpio_tlmm_config(GPIO_CFG(pdata->rst_gpio, 0, GPIO_OUTPUT,
						//GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	//gpio_request(pdata->rst_gpio, "touch_rst");
	gpio_direction_output(pdata->rst_gpio, 1);
	
	//if(rc){
		//st1332_msg(INFO, "gpio touch_rst config failed.\n");
	//}

	//if(ret_gpio < 0)
	//{
		//t1332_msg(INFO, "st1332_suspend(): gpio touch_rst config failed.\n");
	//}
	//Setting the configuration of GPIO 23  (2010.11.23)--
	//Setting the configuration of GPIO 89++
	ret_gpio = gpio_tlmm_config(GPIO_CFG(pdata->intr_gpio, 0, GPIO_INPUT,
						GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	if(ret_gpio < 0)
	{
	    msm_cap_touch_gpio_pull_fail = 1;
	    printk(KERN_INFO "st1332_suspend(): GPIO89_PULL_UP failed!\n");
	}
	printk(KERN_INFO "st1332_suspend(): GPIO89_PULL_UP\n");
	//Setting the configuration of GPIO 89--
	return 0; //Remove to set power state by Stanley
}

//static int st1332_resume(struct i2c_client *client)
static int st1332_resume(struct device *client)  //Added for new kernel suspend scheme (2010.11.16)
{
    //struct vreg *vreg_wlan;  //Add for VREG_WLAN power in, 07/08
    //struct st1332_i2c_sensitivity sen;  //Added for modify sensitivity, 0729
    struct st1332_i2c_platform_data *pdata = st1332->client->dev.platform_data;  //Added for new kernel suspend scheme (2010.11.16)
    int ret_gpio = 0;  //Added for modify sensitivity, 0729
    //int retry = 20,			/* retry count of device detecting */	    
		//pkt;				/* packet st1332_buffer */
    
//Remove to set power state by Stanley++
#if 0
	int state = 0, retry = 10;

	//st1332_set_pw_state(0);
	do {
	    st1332_set_pw_state(0);
		st1332_get_pw_state(&state);
		if (--retry == 0) {
			st1332_msg(ERR, "can not wake device up");
			return -1;
		}
	} while (state);
#endif
//Remove to set power state by Stanley--
    //printk(KERN_INFO "st1332_resume() enable IRQ: %d\n", client->irq);
	//enable_irq(client->irq);
    //Setting the configuration of GPIO 23 (2010.11.23)++   
    gpio_direction_output(pdata->rst_gpio, 1);
	msleep(5);  //Modified by Stanley
	gpio_direction_output(pdata->rst_gpio, 0);
	msleep(5);  //Modified by Stanley
	gpio_direction_output(pdata->rst_gpio, 1);
	msleep(145);  //Modified by Stanley
	//if(ret_gpio < 0)
    //{
        //st1332_msg(INFO, "st1332_suspend(): gpio touch_rst config failed.\n");
    //}
    //Setting the configuration of GPIO 23  (2010.11.23)--
    //Setting the configuration of GPIO 89++
	ret_gpio = gpio_tlmm_config(GPIO_CFG(pdata->intr_gpio, 0, GPIO_INPUT,
						GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	if(ret_gpio < 0)
	{
	    msm_cap_touch_gpio_pull_fail = 1;
	    printk(KERN_INFO "st1332_suspend(): GPIO89_PULL_UP failed!\n");
	}
	//Setting the configuration of GPIO 89--
	printk(KERN_INFO "st1332_resume() enable IRQ: %d and GPIO89_PULL_UP\n", st1332->client->irq);
	//enable_irq(client->irq);
	enable_irq(st1332->client->irq);  //Added for new kernel suspend scheme (2010.11.16)
	return 0;
}
#else
#define st1332_suspend	NULL
#define st1332_resume	NULL
#endif



/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#ifdef CONFIG_FIH_FXX
#ifdef CONFIG_HAS_EARLYSUSPEND
void st1332_early_suspend(struct early_suspend *h)
{
    struct st1332_m32emau *pst1332;
	pst1332 = container_of(h, struct st1332_m32emau, st1332_early_suspend_desc);

    printk(KERN_INFO "st1332_early_suspend()\n");
    //st1332_suspend(pst1332->client, PMSG_SUSPEND);
    printk(KERN_INFO "st1332_suspend() disable IRQ: %d\n", pst1332->client->irq);
    disable_irq(pst1332->client->irq);
    printk(KERN_INFO "st1332_early_suspend() exit!\n");
}
void st1332_late_resume(struct early_suspend *h)
{
    struct st1332_m32emau *pst1332;
	//int iValue=0xff;
	pst1332 = container_of(h, struct st1332_m32emau, st1332_early_suspend_desc);

    printk(KERN_INFO "st1332_late_resume()\n");
    //st1332_resume(pst1332->client);
    printk(KERN_INFO "st1332_resume() enable IRQ: %d\n", pst1332->client->irq);
	enable_irq(pst1332->client->irq);

	//if (st1332_get_fw_version(&iValue) >= 0)
        //st1332_msg(INFO, "[TOUCH-CAP]st1332 firmware version = %d", iValue);
	//else
		//st1332_msg(INFO, "[TOUCH-CAP]st1332 get firmware version failed");
	
	printk(KERN_INFO "st1332_late_resume() exit!\n");
}
#endif
#endif
/* } FIH, SimonSSChang, 2009/09/04 */

//Added by Stanley for dump scheme++
static int msm_seq_open(struct inode *inode, struct file *file)
{
	//printk(KERN_INFO "msm_open\n");
  	return single_open(file, NULL, NULL);
}

static ssize_t msm_seq_write(struct file *file, const char *buff, size_t len, loff_t *off)
{
	char str[64];
	int param = -1;
	int param2 = -1;
	int param3 = -1;
	char cmd[32];
	u32 sen_x = 2, sen_y = 3;

	struct st1332_i2c_sensitivity sen;

	printk(KERN_INFO "MSM_TOUCH_Write ~~\n");
	if(copy_from_user(str, buff, sizeof(str)))
		return -EFAULT;	

  	if(sscanf(str, "%s %d %d %d", cmd, &param, &param2, &param3) == -1)
	{
	  	printk("parameter format: <type> <value>\n");

 		return -EINVAL;
	}	  	

	if(!strnicmp(cmd, "sen", 3))
	{	
		sen_x = param;
		sen_y = param2;
		printk(KERN_INFO "sen param = %d\n",sen_x);
	}
	else
	{
		printk(KERN_INFO "Parameter error!\n");
	}

    sen.x = sen_x;
    sen.y = sen_y;
	if (st1332_set_sensitivity(&sen) < 0)
	    {
            st1332_msg(INFO, "[TOUCH-CAP]st1332_set_sensitivity failed!\r\n");
            msleep(100);
        }
	
	return len;
}

static ssize_t msm_seq_read(struct file *file, char __user *data, size_t count, loff_t *ppos)
{
#if 0
    //int bytes, reg;
    int bytes;
    char cmd[4] = { 0x53, 0x20, 0x00, 0x01 };
    char cmd2[4] = { 0x53, 0x30, 0x00, 0x01 };
    char cmd3[4] = { 0x53, 0x40, 0x00, 0x01 };
    char cmd4[4] = { 0x53, 0x60, 0x00, 0x01 };
    char cmd5[4] = { 0x53, 0x63, 0x00, 0x01 };
    char cmd6[4] = { 0x53, 0xB0, 0x00, 0x01 };
    char cmd7[4] = { 0x53, 0xB1, 0x00, 0x01 };
    
    if (*ppos != 0)
        return 0;
    
#if 0
    reg = readl(TSSC_REG(CTL));
    bytes = sprintf(data, "[TOUCH]TSSC_CTL : 0x%x\r\n", reg);
    *ppos += bytes;
    data += bytes;

    reg = readl(TSSC_REG(OPN));
    bytes = sprintf(data, "[TOUCH]TSSC_OPN : 0x%x\r\n", reg);
    *ppos += bytes;
    data += bytes;

    reg = readl(TSSC_REG(SI));
    bytes = sprintf(data, "[TOUCH]TSSC_SAMPLING_INT : 0x%x\r\n", reg);
    *ppos += bytes;
    data += bytes;

    reg = readl(TSSC_REG(STATUS));
    bytes = sprintf(data, "[TOUCH]TSSC_STATUS : 0x%x\r\n", reg);
    *ppos += bytes;
    data += bytes;

    reg = readl(TSSC_REG(AVG12));
    bytes = sprintf(data, "[TOUCH]TSSC_AVG_12 : 0x%x\r\n", reg);
    *ppos += bytes;
    data += bytes;
#endif
    st1332_send(cmd, ARRAY_SIZE(cmd));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]X-axis absolute : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0x30, 0x00, 0x01 };
    st1332_send(cmd2, ARRAY_SIZE(cmd2));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]Y-axis absolute : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0x40, 0x00, 0x01 };
    st1332_send(cmd3, ARRAY_SIZE(cmd3));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]Sensitivity value : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0x60, 0x00, 0x01 };
    st1332_send(cmd4, ARRAY_SIZE(cmd4));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]X-axis resolution : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0x63, 0x00, 0x01 };
    st1332_send(cmd5, ARRAY_SIZE(cmd5));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]Y-axis resolution : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0xB0, 0x00, 0x01 };
    st1332_send(cmd6, ARRAY_SIZE(cmd6));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]Normal report state : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;

    //cmd[4] = { 0x53, 0xB1, 0x00, 0x01 };
    st1332_send(cmd7, ARRAY_SIZE(cmd7));
	st1332_recv(st1332_buffer, ARRAY_SIZE(st1332_buffer));
	bytes = sprintf(data, "[TOUCH]Origin point state : 0x%x, 0x%x, 0x%x, 0x%x\r\n", st1332_buffer[0], st1332_buffer[1], st1332_buffer[2], st1332_buffer[3]);
    *ppos += bytes;
    data += bytes;
#else //0
    *ppos = 0;
#endif //0
    
    return *ppos;
}

static struct file_operations msm_touch_seq_fops =
{
  	.owner 		= THIS_MODULE,
	.open  		= msm_seq_open,
	.write 		= msm_seq_write,
	.read		= msm_seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
//Added by Stanley for dump scheme--

static int st1332_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct input_dev *input;
	struct st1332_i2c_platform_data *pdata;
	struct st1332_i2c_sensitivity sen;  //Added for modify sensitivity, 0729
    
	int i, iValue = 0;  //Added for software reset
	char sensitivity_x = 0xF, sensitivity_y = 0xF;  //Added for modify sensitivity, 0817
//	char reset_cmd[2] = { 0x02, 0x0A };  //Added for software reset
	u8 status;
	int retry = 50;			/* retry count of device detecting */	
	int rc = 0, iST1332_Interval = 0, iST1332_ButtonWidth = 0;  //Added SK new scheme for DMP (2010/10/25)

	
	printk(KERN_ERR "st1332_probe()\n" );
	st1332 = kzalloc(sizeof(struct st1332_m32emau), GFP_KERNEL);
	if (st1332 == NULL) {
		st1332_msg(ERR, "can not allocate memory for st1332");
		return -ENOMEM;
	}

/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#ifdef CONFIG_FIH_FXX
#ifdef CONFIG_HAS_EARLYSUSPEND
        st1332->st1332_early_suspend_desc.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 11;
	    st1332->st1332_early_suspend_desc.suspend = st1332_early_suspend;
	    st1332->st1332_early_suspend_desc.resume = st1332_late_resume;
        printk(KERN_INFO "CAP_Touch register_early_suspend()\n");
	    register_early_suspend(&st1332->st1332_early_suspend_desc);

#endif
#endif
/* } FIH, SimonSSChang, 2009/09/04 */
	
	st1332->client = client;
	dev_set_drvdata(&client->dev, st1332);

    device_init_wakeup(&client->dev, 1);  //Added for FST by Stanley (F0X_2.B-414)

	rc=0;
	retry=50;
	status=0;
	
#define COUNT_TRY 500
#if 0
//Added for software reset++
    for(i = 0 ; i < COUNT_TRY ; i++)
	{
	    if (st1332_send(reset_cmd, ARRAY_SIZE(reset_cmd)) < 0)
	    {
            st1332_msg(INFO, "[TOUCH-CAP]Software reset (%d)failed!\r\n", i);
            msleep(50);
            //Added for check ELAN chip++
            if(i == COUNT_TRY-1)
            {
                st1332_msg(INFO, "%s: return -ENODEV!\r\n", __func__);
                dev_set_drvdata(&client->dev, 0);
                unregister_early_suspend(&st1332->st1332_early_suspend_desc);
                kfree(st1332);
                //st1332_exit();
                
                return -ENODEV;
            }
            //Added for check ELAN chip--    
        }
        else
        {
            st1332_msg(INFO, "[TOUCH-CAP]Software reset successful!\r\n"); 
            break;
        }
	}
	msleep(300);
//Added for software reset--

#endif


#if 0	
	st1332_buffer[0] = 0x01;
	st1332_send(st1332_buffer, 1);	//Set address to Status Register 0x01.
	do {
		status = st1332_recv(st1332_buffer, 1);
		status |= st1332_buffer[0];

		if (--retry == 0) {
			st1332_msg(ERR, "detect timeout");
			break;
			//goto err1;
		}
	} while (status);
#endif

	init_completion(&st1332->data_ready);
	init_completion(&st1332->data_complete);
	INIT_WORK(&st1332->wqueue, st1332_isr_workqueue);

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		st1332_msg(ERR, "can not get platform data");
		goto err1;
	}

	/* hw reset */	
#if 1
	/*  config gpio */
	gpio_tlmm_config(GPIO_CFG(pdata->rst_gpio, 0, GPIO_OUTPUT,
						GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	rc=gpio_request(pdata->rst_gpio, "touch_rst");

	if(rc){
		st1332_msg(ERR, "gpio touch_rst config failed.\n");
	}

	gpio_direction_output(pdata->rst_gpio,1);
	msleep(5);  //Modified by Stanley
	st1332_msg(ERR, "pull %d high (%d)", pdata->rst_gpio, gpio_get_value(pdata->rst_gpio));
#if 1 //reset test
	//mdelay(1000);
	gpio_direction_output(pdata->rst_gpio,0);
	msleep(5);  //Modified by Stanley
	st1332_msg(ERR, "pull %d low (%d)", pdata->rst_gpio, gpio_get_value(pdata->rst_gpio));

	gpio_direction_output(pdata->rst_gpio,1);
	msleep(145);  //Modified by Stanley
	st1332_msg(ERR, "pull %d high (%d)", pdata->rst_gpio, gpio_get_value(pdata->rst_gpio));
#endif	
//	gpio_free(pdata->rst_gpio);	
#endif	


    input = input_allocate_device();
    if (input == NULL) {
		st1332_msg(ERR, "can not allocate memory for input device");
        goto err1;
    }

    input->name = "ST1332 Touchscreen";
    input->phys = "st1332/input0";
    input->open = input_open;
    input->close= input_close;
	
    set_bit(EV_KEY, input->evbit);
    set_bit(EV_ABS, input->evbit);
    set_bit(EV_SYN, input->evbit);
    set_bit(BTN_TOUCH, input->keybit);
    set_bit(BTN_2, input->keybit);  //Added for Multi-touch
    set_bit(KEY_BACK, input->keybit);  //Modified for new CAP sample by Stanley (2009/05/25)
    set_bit(KEY_KBDILLUMDOWN, input->keybit);  //Modified for new CAP sample by Stanley (2009/05/25)
#if 0	//[mq4_poring]
    //Modified for Home and AP key (2009/07/31)++
    if((FIH_READ_HWID_FROM_SMEM() >= CMCS_CTP_PR2) && (FIH_READ_HWID_FROM_SMEM() != CMCS_7627_PR1))
    {
#endif    
        set_bit(KEY_HOME, input->keybit);
        set_bit(KEYCODE_BROWSER, input->keybit);
        set_bit(KEYCODE_SEARCH, input->keybit);
        //set_bit(KEY_SEND, input->keybit);  //Added for FST
        //set_bit(KEY_END, input->keybit);  //Added for FST
        //bIsFxxPR2 = 1; //Added for PR2
#if 0	//[mq4_poring]
	}
#endif	

    //Added the HWID scheme to decide TP resolution (2011.01.20)++
    if((fih_read_product_id_from_orighwid() >= Project_DPD) && (fih_read_product_id_from_orighwid() <= Project_FAD))
    {
        //For 3.5"
        pdata->abs_x_max = 320;
        pdata->abs_y_max = 480;
        st1332_msg(INFO, "[ST1332]Set TP resolution for 3.5 inch.");
    }
    else if(fih_read_product_id_from_orighwid() < Project_DPD)
    {
        //For 2.8"
        pdata->abs_x_max = 240;
        pdata->abs_y_max = 320;
        st1332_msg(INFO, "[ST1332]Set TP resolution for 2.8 inch.");
    }
    //Added the HWID scheme to decide TP resolution for DMO/DMT (2011.02.09)++
    else if((fih_read_product_id_from_orighwid() == Project_DMO) || (fih_read_product_id_from_orighwid() == Project_DMT))
    {
        //For 3.5"
        pdata->abs_x_max = 320;
        pdata->abs_y_max = 480;
        st1332_msg(INFO, "[ST1332]Set TP resolution for 3.5 inch (DMO/DMT).");
    }
    //Added the HWID scheme to decide TP resolution for DMO/DMT (2011.02.09)--
    //Added the HWID scheme to decide TP resolution (2011.01.20)--

    input_set_abs_params(input, ABS_X, pdata->abs_x_min,
								pdata->abs_x_max, 0, 0);
    input_set_abs_params(input, ABS_Y, pdata->abs_y_min,
								pdata->abs_y_max, 0, 0);
    input_set_abs_params(input, ABS_HAT0X, pdata->abs_x_min,
								pdata->abs_x_max, 0, 0);
    input_set_abs_params(input, ABS_HAT0Y, pdata->abs_y_min,
								pdata->abs_y_max, 0, 0);
	
#if 1
    //Added the MT protocol for Eclair by Stanley (2010/03/23)++
    input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0,
								255, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_X, pdata->abs_x_min,
								pdata->abs_x_max, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y, pdata->abs_y_min,
								pdata->abs_y_max, 0, 0);
	printk(KERN_ERR "[ST1332] MAX x,y (%d,%d) \n",pdata->abs_x_max,pdata->abs_y_max);
    //Added the MT protocol for Eclair by Stanley (2010/03/23)--
#endif

	/*** register input device ***/
	st1332->input = input;
	
    if (input_register_device(st1332->input)) {
		st1332_msg(ERR, "can not register input device");
        goto err2;
	}

	if (misc_register(&st1332_misc_dev)) {
		st1332_msg(ERR, "can not add misc device");
		goto err3;
    }

	/*** setup irq pin ***/
	if (MSM_GPIO_TO_INT(pdata->intr_gpio) != client->irq) {
		st1332_msg(ERR, "irq not match, gpio(%d), irq(%d)", pdata->intr_gpio, client->irq);
		goto err4;
	}else{
		st1332_msg(ERR, "irq match, gpio(%d), irq(%d)", pdata->intr_gpio, client->irq);
	}
	
	gpio_tlmm_config(GPIO_CFG(pdata->intr_gpio, 0, GPIO_INPUT,
						GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

#if 0  //for irq test
	gpio_tlmm_config(GPIO_CFG(pdata->intr_gpio, 0, GPIO_OUTPUT,
						GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);

	rc=gpio_request(pdata->intr_gpio, "touch_irq");

	if(rc){
		st1332_msg(ERR, "gpio touch_rst config failed.\n");
	}

	gpio_direction_output(pdata->intr_gpio,0);
	mdelay(5);
	st1332_msg(ERR, "pull %d low (%d)", pdata->intr_gpio, gpio_get_value(pdata->intr_gpio));
#endif

	

	/*** Add for detect firmware version++	***/
	if (st1332_get_fw_version(&iValue) >= 0)
	{
        st1332_msg(INFO, "[TOUCH-CAP]st1332 firmware version = %d", iValue);
        msm_cap_touch_fw_version = iValue;  //Added for show FW version on FQC
        //if(iValue >= 103)
  	}
	else
    {
        st1332_msg(INFO, "[TOUCH-CAP]st1332 firmware version failed!\r\n");
        return -EIO;
    }


    /*** Add for modify sensitivity, 0729++ ***/
    sen.x = sensitivity_x;
    sen.y = sensitivity_y;
    if(!bIsNewFWVer)  //Add for detect firmware version
    {
        for(i = 0 ; i < 10 ; i++)
	    {
	        if (st1332_set_sensitivity(&sen) < 0)
	        {
                st1332_msg(INFO, "[TOUCH-CAP]st1332_set_sensitivity failed!\r\n");
                msleep(50);
            }
            else
            {
                st1332_msg(INFO, "[TOUCH-CAP]st1332_set_sensitivity successful!\r\n"); 
                break;
            }
	    }
	}

    //Added SK new scheme for DMP (2010/10/25)++
    iX_max = pdata->abs_x_max;
    iY_max = pdata->abs_y_max;
    iST1332_Interval = (pdata->abs_x_max - pdata->abs_x_min) / 20;
	iST1332_ButtonWidth = ((pdata->abs_x_max - pdata->abs_x_min) - (iST1332_Interval * 3)) / 4;
	uST1332_VkeySoft1MiddleBoundary = pdata->abs_x_min + iST1332_ButtonWidth;
	uST1332_VkeyHomeLeftBoundary = uST1332_VkeySoft1MiddleBoundary + iST1332_Interval;
	uST1332_VkeyHomeRightBoundary = uST1332_VkeyHomeLeftBoundary + iST1332_ButtonWidth;
	uST1332_VkeyApLeftBoundary = uST1332_VkeyHomeRightBoundary + iST1332_Interval;
	uST1332_VkeyApRightBoundary = uST1332_VkeyApLeftBoundary + iST1332_ButtonWidth;
	uST1332_VkeySoft2MiddleBoundary = uST1332_VkeyApRightBoundary + iST1332_Interval;
    //Added SK new scheme for DMP (2010/10/25)--
	
	//Added by Stanley for dump scheme++
  	msm_touch_proc_file = create_proc_entry("driver/cap_touch", 0666, NULL);
	
	if(!msm_touch_proc_file){
	  	printk(KERN_INFO "create proc file for Msm_touch failed\n");
		return -ENOMEM;
	}

	printk(KERN_INFO "st1332_probe ok\n");
	msm_touch_proc_file->proc_fops = &msm_touch_seq_fops;
	//Added by Stanley for dump scheme--
    return 0;

err4:
	misc_deregister(&st1332_misc_dev);
err3:
	input_unregister_device(st1332->input);
err2:
	input_free_device(input);
err1:
	dev_set_drvdata(&client->dev, 0);
	kfree(st1332);
	return -1;
}

static int st1332_remove(struct i2c_client * client)
{
/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#ifdef CONFIG_FIH_FXX
#ifdef CONFIG_HAS_EARLYSUSPEND
    printk(KERN_INFO "CAP_Touch unregister_early_suspend()\n");
	unregister_early_suspend(&st1332->st1332_early_suspend_desc);
#endif
#endif
/* } FIH, SimonSSChang, 2009/09/04 */

	misc_deregister(&st1332_misc_dev);
	input_unregister_device(st1332->input);
    dev_set_drvdata(&client->dev, 0);
    kfree(st1332);

	return 0;
}

static const struct i2c_device_id st1332_id[] = {
    { TOUCH_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, st1332);

//Added for new kernel suspend scheme (2010.11.16)++
static struct dev_pm_ops st1332_pm_ops = 
{   .suspend = st1332_suspend,
    .resume  = st1332_resume,
};
//Added for new kernel suspend scheme (2010.11.16)--

static struct i2c_driver st1332_i2c_driver = {
	.driver = {
		.name	= TOUCH_NAME,
		.owner	= THIS_MODULE,
//Added for new kernel suspend scheme (2010.11.16)++
#ifdef CONFIG_PM
    .pm = &st1332_pm_ops,
#endif
//Added for new kernel suspend scheme (2010.11.16)--
	},
	.id_table   = st1332_id,
	.probe  	= st1332_probe,
	.remove 	= st1332_remove,
/* FIH, SimonSSChang, 2009/09/04 { */
/* [FXX_CR], change CAP touch suspend/resume function
to earlysuspend */
#ifdef CONFIG_FIH_FXX
	//.suspend	= st1332_suspend,
    //.resume		= st1332_resume,
#else
	//.suspend	= st1332_suspend,
    //.resume		= st1332_resume,
#endif	
/* } FIH, SimonSSChang, 2009/09/04 */
};

static int __init st1332_init( void )
{
//	struct vreg * vreg_rfrx2=0;
    //struct vreg *vreg_wlan;  //Add for VREG_WLAN power in, 09/10
    //int ret;

	printk(KERN_INFO "st1332_init()\n" );
    //Added for Ai1 driver (2011.01.06)++
    if((fih_read_product_id_from_orighwid() != Project_AI1S) && (fih_read_product_id_from_orighwid() != Project_AI1D))
    {
	    msm_cap_touch_debug_mask = *(uint32_t *)TOUCH_DEBUG_MASK_OFFSET;  //Added for debug mask (2010/01/04)
    }
    else
    {
        printk(KERN_INFO "[st1332_init]It was AI1 device and don't need to load it\n" );
        return -ENODEV;
    }
    //Added for Ai1 driver (2011.01.06)--
	 //Neo: Add for increasing VREG_WLAN refcnt and let VREG_WLAN can be closed at first suspend +++
#if 0
	 if((FIH_READ_HWID_FROM_SMEM() != CMCS_7627_PR1) && (FIH_READ_HWID_FROM_SMEM() != CMCS_F913_PR1))  //Don't apply VREG_WLAN power in on PR1++
	 {
	    vreg_wlan = vreg_get(0, "wlan");

	    if (!vreg_wlan) {
	     printk(KERN_ERR "%s: init vreg WLAN get failed\n", __func__);
	    }

	    ret = vreg_enable(vreg_wlan);
	    if (ret) {
	      printk(KERN_ERR "%s: init vreg WLAN enable failed (%d)\n", __func__, ret);
	    }
	 }
#endif
	 //Neo: Add for increasing VREG_WLAN refcnt and let VREG_WLAN can be closed at first suspend ---

	
  return i2c_add_driver(&st1332_i2c_driver);
}

static void __exit st1332_exit( void )
{
	i2c_del_driver(&st1332_i2c_driver);
}

module_init(st1332_init);
module_exit(st1332_exit);

MODULE_DESCRIPTION("ST1332 Touchscreen driver");
MODULE_AUTHOR("Chandler Kang, chandlercckang@fihtdc.com");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
