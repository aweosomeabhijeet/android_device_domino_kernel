
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include "msm_fb.h"
#include <asm/gpio.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include "mach/sc668.h"
#include "mddihosti.h"
/*
    Macro for easily apply LCM vendor's init code
*/
#define WRITE9BITS(data,c) \
do { \
    u16 buf=data; \
    buf|=(c<<8); \
    ret=spi_write_9bits(spi, &buf, 2); \
    if(ret) fih_printk(debug_mask_lcm, FIH_LCM_ERR, KERN_ERR "[lcdc_ili9481] SPI write error  0x%x.\n", data);\
} while(0)

#define Delayms(n) mdelay(n)
#define WriteI(n) WRITE9BITS(n,0)
#define WriteD(n) WRITE9BITS(n,1)
#define LCD_ILI9481_CMD(n) WRITE9BITS(n,0)
#define LCD_ILI9481_INDEX(n) WRITE9BITS(n,1)

#define LCDSPI_PROC_FILE        "driver/lcd_spi"
#define ARRAY_AND_SIZE(x)	(x), ARRAY_SIZE(x)

/* for CMI 3.5 LCM */
#define SPI_send_Command(n) WRITE9BITS(n,0)
#define SPI_send_Data(n) WRITE9BITS(n,1)
#define Wait mdelay

static struct mutex               innolux_dd_lock;
static struct list_head           dd_list;
static struct proc_dir_entry	  *lcdspi_proc_file;
static struct spi_device	  *g_spi;

struct driver_data {
        struct input_dev         *ip_dev;
        struct spi_device        *spi;
        char                      bits_per_transfer;
        struct work_struct        work_data;
        bool                      config;
        struct list_head          next_dd;
};


extern int spi_write_9bits_then_read_8bits(struct spi_device *spi,
		const u16 *txbuf, unsigned n_tx,
		u8 *rxbuf, unsigned n_rx);

/**
 * spi_write - SPI synchronous write with 9 bits per word
 * @spi: device to which data will be written
 * @buf: data buffer
 * @len: data buffer size
 */
static inline int
spi_write_9bits(struct spi_device *spi, const u16 *buf, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= len,
			.bits_per_word =9,
		};
		
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static void reset_lcmic(void)
{
    int gpio_lcd_reset=103;
    int ret=0;
	
    gpio_request(gpio_lcd_reset, "lcdc_ili9481");
	
    /* LCD reset pin: pull high */
    ret = gpio_direction_output(gpio_lcd_reset, 1);
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: reset_lcmic(): gpio_direction_output hight: read(%d)\n", gpio_get_value(gpio_lcd_reset));
    mdelay(1);

    ret = gpio_direction_output(gpio_lcd_reset, 0);
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: reset_lcmic(): gpio_direction_output low: read(%d)\n", gpio_get_value(gpio_lcd_reset));
    mdelay(10);
    
    ret = gpio_direction_output(gpio_lcd_reset, 1);
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: reset_lcmic(): gpio_direction_output hight: read(%d)\n", gpio_get_value(gpio_lcd_reset));
    mdelay(10);

	gpio_free(gpio_lcd_reset);
}


static int ilitek_config(struct driver_data *dd, struct spi_device *spi)
{
    int ret=0;
    reset_lcmic();
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: ilitek_config()\n");
    //************* Start Initial Sequence **********//
    LCD_ILI9481_CMD(0x11);    
    Delayms(20);
    
    //lcdc_ili9481: for polarity of den, hsync, vsync, pclk
    LCD_ILI9481_CMD(0xC6);    
    //LCD_ILI9481_INDEX(0x1B);    
    LCD_ILI9481_INDEX(0x03); //chandler_boot_lcm: hsync, vsync active low  
    
    LCD_ILI9481_CMD(0xD0);   
    LCD_ILI9481_INDEX(0x07);
    LCD_ILI9481_INDEX(0x42);
    LCD_ILI9481_INDEX(0x1C);

    LCD_ILI9481_CMD(0xD1);   
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x02);
    LCD_ILI9481_INDEX(0x0F);

    /* Power_Setting for Normal Mode */
    LCD_ILI9481_CMD(0xD2);   
    LCD_ILI9481_INDEX(0x01);
    //lcdc_ili9481: 
//    LCD_ILI9481_INDEX(0x02); 
    LCD_ILI9481_INDEX(0x22); 

    LCD_ILI9481_CMD(0xC0);   
    LCD_ILI9481_INDEX(0x10);
    LCD_ILI9481_INDEX(0x3b);
    LCD_ILI9481_INDEX(0x00);

    LCD_ILI9481_INDEX(0x02);
    LCD_ILI9481_INDEX(0x11);

    /* 
    // lcdc_ili9481: from innolux
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x01);
    LCD_ILI9481_CMD(0x28);   
    */
    // for internal clock
    LCD_ILI9481_CMD(0xC5);   
	WriteD(0x03);  //72Mhz
	//WriteD(0x02);  //85Mhz
	//WriteD(0x01);  //100Mhz
	//WriteD(0x00);  //125Mhz

    //lcdc_ili9481: Display timing (form innolux) 
    WriteI(0xC1);
    WriteD(0x10);  
    WriteD(0x10); 
    WriteD(0x88);
/*
    LCD_ILI9481_CMD(0xC8);   
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x30);
    LCD_ILI9481_INDEX(0x36);
    LCD_ILI9481_INDEX(0x45);
    LCD_ILI9481_INDEX(0x04);
    LCD_ILI9481_INDEX(0x16);
    LCD_ILI9481_INDEX(0x37);
    LCD_ILI9481_INDEX(0x75);
    LCD_ILI9481_INDEX(0x77);
    LCD_ILI9481_INDEX(0x54);
    LCD_ILI9481_INDEX(0x0F);
    LCD_ILI9481_INDEX(0x00);
*/
 //===Gamma setting =====(form innolux)
 
     WriteI(0xC8);
     WriteD(0x00);  
     WriteD(0x77);  
     WriteD(0x06);  
     WriteD(0x42);  
     WriteD(0x0F);  
     WriteD(0x00);  
     WriteD(0x17);  
     WriteD(0x00);  
     WriteD(0x77);  
     WriteD(0x24);  
     WriteD(0x00);  
     WriteD(0x03);       

    LCD_ILI9481_CMD(0xF0);   
    LCD_ILI9481_INDEX(0x00);

    LCD_ILI9481_CMD(0xF6);   
    LCD_ILI9481_INDEX(0x80);

    LCD_ILI9481_CMD(0xF3);   
    LCD_ILI9481_INDEX(0x02);
    LCD_ILI9481_INDEX(0x1A);

    LCD_ILI9481_CMD(0xF4);   //For AC RGB pre-buffer
    LCD_ILI9481_INDEX(0x4D);
    LCD_ILI9481_INDEX(0x2C);

    LCD_ILI9481_CMD(0x3A);   //
    LCD_ILI9481_INDEX(0x55);  //16bit

    LCD_ILI9481_CMD(0xF1);   //External clock division
    LCD_ILI9481_INDEX(0x0B);


    LCD_ILI9481_CMD(0xF7);   
    LCD_ILI9481_INDEX(0x80);

    LCD_ILI9481_CMD(0x36);   
    LCD_ILI9481_INDEX(0xca); //rotate degree 180

    LCD_ILI9481_CMD(0x2A);   
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x01);
    LCD_ILI9481_INDEX(0x3F);

    LCD_ILI9481_CMD(0x2B);
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x00);
    LCD_ILI9481_INDEX(0x01);
    LCD_ILI9481_INDEX(0xDF);


    Delayms(120);
    LCD_ILI9481_CMD(0x29);

    LCD_ILI9481_CMD(0xB4);   //Switch to RGB I/F
    //LCD_ILI9481_INDEX(0x11);  // select PCLK
	LCD_ILI9481_INDEX(0x10);  // internal clock

    Delayms(120);
    LCD_ILI9481_CMD(0x2C); //send 0x2c before write data

    return 0;
}

inline int lcdc_ili9481_config_35v1(struct driver_data *dd, struct spi_device *spi)
{
/*
	ResetDigitalControl(MLCD_RESET);
	Wait(10);
	SetDigitalControl(MLCD_RESET);
	Wait(60);
*/
	int ret;
	//reset_lcmic();
	fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481:%s(): \n", __func__);
	 //Power Setting Sequence
	SPI_send_Command(0x11);     //Exit sleep mode  + Delay > 80ms
	Wait(120);
	SPI_send_Command(0x13);     //Enter normal mode
	SPI_send_Command(0x20);     //Exit invert mode
	SPI_send_Command(0x3A);     
	SPI_send_Data(0x66);	//Set pixel format = 18bit data
//	SPI_send_Data(0x55);	//Set pixel format = 16bit data
	SPI_send_Command(0xB4);     
	SPI_send_Data(0x11);	//Write mode setting   RGB-IF
	SPI_send_Command(0xD0);     //Power Setting
	SPI_send_Data(0x07); 
	SPI_send_Data(0x41);
	SPI_send_Data(0x0D);    //external VCI
	
	SPI_send_Command(0xD1);     //VCOM Control
	SPI_send_Data(0x00); 
	SPI_send_Data(0x11);
	SPI_send_Data(0x19);		//update by Jay
	SPI_send_Command(0xD2);     //Power Setting for Normal mode
	SPI_send_Data(0x01); 
	SPI_send_Data(0x11);

	SPI_send_Command(0xC0);     //Panel Driving Settingl
	SPI_send_Data(0x00); 
	SPI_send_Data(0x3B);        // 8 * ( 3B+1) = 480
	SPI_send_Data(0x00);
	SPI_send_Data(0x02);
	SPI_send_Data(0x11);

	SPI_send_Command(0xC1);     //Display timing for normal
	SPI_send_Data(0x10); 
	SPI_send_Data(0x10);	//update by Jay
	SPI_send_Data(0x88);
	SPI_send_Command(0xC5);     //Frame rate and Inversion Control
	SPI_send_Data(0x03); 

	SPI_send_Command(0xC6);     //INTERFACE SET
	SPI_send_Data(0x01);        //polarity of lcm

	SPI_send_Command(0xC8);     //Gamma
	SPI_send_Data(0x01);
	SPI_send_Data(0x00); 
	SPI_send_Data(0x45);
	SPI_send_Data(0x11); 
	SPI_send_Data(0x70); 
	SPI_send_Data(0x00); 
	SPI_send_Data(0x1F); 
	SPI_send_Data(0x77); 
	SPI_send_Data(0x01);  
	SPI_send_Data(0x77); 
	SPI_send_Data(0x07); 
	SPI_send_Data(0x00); 
	SPI_send_Data(0x00); 
	SPI_send_Command(0xE4);     //Gamma
	SPI_send_Data(0xA0);
	SPI_send_Command(0xF0);             
	SPI_send_Data(0x01);
	SPI_send_Command(0xF3);     
	SPI_send_Data(0x40);
	SPI_send_Data(0x0A);	
	SPI_send_Command(0xF7);             
	SPI_send_Data(0x80);
	SPI_send_Command(0x36);             
	SPI_send_Data(0x0a);       //  Pixel setting //horizontal flip
	Wait(35);
	SPI_send_Command(0x29);  		//Display ON
	SPI_send_Command(0x2C);  		//Display ON
	//Wait(200);

	return ret;
}

inline int lcdc_ili9481_config(struct driver_data *dd, struct spi_device *spi)
{
#if 1
	return lcdc_ili9481_config_35v1(dd,spi);
#else
    int ret=0;
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s()\n", __func__);
    
    //VCI=2.8V;
    WriteI(0x11);   
    Delayms(20);
    
    //=====POWER Setting==//
    WriteI(0xD0);     
    WriteD(0x07);   
    WriteD(0x40);                                        
    WriteD(0x1A);    
         
  //===VCOM Setting=======//
     WriteI(0xD1);
     WriteD(0x00);  
     WriteD(0x05);    
     WriteD(0x10);    

  
  //====POWER Setting For normal mode ===//
     WriteI(0xD2);
     WriteD(0x01); 
     WriteD(0x00); 
    
       
  // ==== Panel driver setting =====//
  
     WriteI(0xC0);
     WriteD(0x10);
     WriteD(0x3B); 
     WriteD(0x00); 
     WriteD(0x02);
     WriteD(0x11);
     
    //==== display off  =====//
     //WriteI(0x28);  
    
  
  //====Frame rate and invertion control===//

     WriteI(0xC5);
     //WriteD(0x03);  //72Mhz
	 WriteD(0x02);  //85Mhz
	 //WriteD(0x01);  //100Mhz
	 //WriteD(0x00);  //125Mhz
    
     WriteI(0xC1);
     WriteD(0x10);  
     WriteD(0x10); 
     WriteD(0x88);
 

  //===Interface Pixel mode=====//?
  
    //lcdc_ili9481
    WriteI(0x3A);
    WriteD(0x55);
 

 //===Gamma setting =====// 
     WriteI(0xC8);
     WriteD(0x00);  
     WriteD(0x37);  
     WriteD(0x11);  
     WriteD(0x21);  
     WriteD(0x04);  
     WriteD(0x02);  
     WriteD(0x66);  
     WriteD(0x04);  
     WriteD(0x77);  
     WriteD(0x12);  
     WriteD(0x05);  
     WriteD(0x00);    

     /* lcdc_ili9481: for polarity of den, hsync, vsync, pclk */
     WriteI(0xC6);    
     //LCD_ILI9481_INDEX(0x82);    
     //WriteD(0x03); //chandler_boot_lcm: hsync, vsync active low  
     WriteD(0x01); //chandler_boot_lcdc


     WriteI(0xB4);
     WriteD(0x11);  //use PCLK


     WriteI(0xF3);
     WriteD(0x20);
     WriteD(0x07);

     WriteI(0x36);
     WriteD(0x09);
     
     WriteI(0xB3);
     WriteD(0x02);
     WriteD(0x00);
     WriteD(0x00);
     WriteD(0x21);
          
     WriteI(0x29);
     Delayms(20);
     WriteI(0x2C);
	
    return ret;	
#endif
}

static int simple_config(struct driver_data *dd, struct spi_device *spi)
{
    int ret=0;
    reset_lcmic();
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: simple_config()\n");
    LCD_ILI9481_CMD(0x11);  //exit sleep mode    
    Delayms(20);
//    LCD_ILI9481_CMD(0x3A);   // 16bits
//    LCD_ILI9481_INDEX(0x55); // 16bits
    Delayms(120);
    LCD_ILI9481_CMD(0x29);  // display on
    
//    LCD_ILI9481_CMD(0xc0);  // panel driving
//    LCD_ILI9481_INDEX(0x10);  // color inversion    
    return 0;    
}

static int get_power_mode(struct driver_data *dd, struct spi_device *spi)
{

    int ret=0;
    reset_lcmic();
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: get_power_mode()\n");
    LCD_ILI9481_CMD(0x11);    
    Delayms(20);
    LCD_ILI9481_CMD(0x0a);  //get power mode  
    LCD_ILI9481_CMD(0x00);  //nop  
    LCD_ILI9481_CMD(0x00);  //nop  

    return 0;    
}

static int get_pixel_mode(struct driver_data *dd, struct spi_device *spi)
{

    int ret=0;    
    u16 txbuf; // just use 9 bits of LSB
    u8 rxbuf[2]={0};
/*
    reset_lcmic();
    printk(KERN_INFO "lcdc_ili9481: get_pixel_mode()\n");

    LCD_ILI9481_CMD(0x11);    // exit sleep mode
    Delayms(20);

    LCD_ILI9481_CMD(0x3A);   // 15bits
    LCD_ILI9481_INDEX(0x55); // 15bits
*/
    txbuf=0x0c;
    spi_write_9bits_then_read_8bits(spi, &txbuf, sizeof(txbuf), rxbuf, 2);
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: get_pixel_mode(), tx(0x%x), rxbuf[]=[0x%x,0x%x]\n", txbuf, rxbuf[0], rxbuf[1] );

    return ret;    
}

static int sleep_config(struct driver_data *dd, struct spi_device *spi)
{

    int ret=0;    
    u16 txbuf; // just use 9 bits of LSB
    u8 rxbuf[2]={0};

    reset_lcmic();
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: get_pixel_mode()\n");

    LCD_ILI9481_CMD(0x11);    // exit sleep mode
    Delayms(20);

    LCD_ILI9481_CMD(0x3A);   // 15bits
    LCD_ILI9481_INDEX(0x55); // 15bits

    txbuf=0x0c;
    spi_write_9bits_then_read_8bits(spi, &txbuf, sizeof(txbuf), rxbuf, 2);
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: get_pixel_mode(), tx(0x%x), rxbuf[]=[0x%x,0x%x]\n", txbuf, rxbuf[0], rxbuf[1] );

    return 0;    
}

void lcdc_ili9481_panel_off(void)
{
    int ret=0;
    struct spi_device *spi=g_spi;
    
    if (!spi){
        fih_printk(debug_mask_lcm, FIH_LCM_ERR, KERN_INFO "lcdc_ili9481: %s(), spi NULL! do nothing.\n", __func__);    
        return;
    }

    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s()\n", __func__);    
    
	WriteI(0x28);	// display off
	WriteI(0x10); // enter sleep

}
EXPORT_SYMBOL(lcdc_ili9481_panel_off);

/*  Add this flag to skip 1st lcd on, 
    because bootload has init before. 
*/
//chandler_boot_lcdc
static int panel_first_on=1; 

void lcdc_ili9481_panel_on(void)
{

    struct spi_device *spi=g_spi;
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s()\n", __func__);
    //chandler_boot_lcdc
	if(panel_first_on==1) {
	    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s(): skip first panel on\n", __func__);    
    	panel_first_on=0;
        return;
    }
    
    if (!spi){
        fih_printk(debug_mask_lcm, FIH_LCM_ERR, KERN_INFO "lcdc_ili9481: %s(), spi NULL! do nothing.\n", __func__);    
        return;
    }


    
    //chandler_boot_lcm
    
    reset_lcmic();
    lcdc_ili9481_config(0,spi);
    
    //Delayms(100);
}


EXPORT_SYMBOL(lcdc_ili9481_panel_on);


static int lcdc_spi_probe(struct spi_device *spi)
{
	struct driver_data *dd;
    int                 rc=0;

	fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481:%s(): \n", __func__);
	dd = kzalloc(sizeof(struct driver_data), GFP_KERNEL);
    if (!dd) {
        rc = -ENOMEM;
        goto probe_exit;
    }
	
	mutex_lock(&innolux_dd_lock);
    list_add_tail(&dd->next_dd, &dd_list);
    mutex_unlock(&innolux_dd_lock);
    dd->spi = spi;
	g_spi= spi;
	
	//chandler_porting: should skip the following config because it's already enable in bootloader.
	/*
	reset_lcmic();
	rc = lcdc_ili9481_config(dd,spi);
	if (rc)
        goto probe_err_cfg;
	*/
	return 0;
	
probe_err_cfg:
    mutex_lock(&innolux_dd_lock);
    list_del(&dd->next_dd);
    mutex_unlock(&innolux_dd_lock);
probe_exit:
    return rc;	
}

static struct spi_driver lcdc_spi_drv = {
    .driver = {
            .name  = "lcdc_spi",
    },
    .probe         = lcdc_spi_probe,
};

static ssize_t lcdspi_proc_read(struct file *filp,
                char *buffer, size_t length, loff_t *offset)
{
        
    printk( KERN_INFO "do nothing\n");

    return 0;
}

static ssize_t lcdspi_proc_write(struct file *filp,
                const char *buff, size_t len, loff_t *off)
{
    char msg[ 512];

    if( len > 512)
            len = 512;

    if( copy_from_user( msg, buff, len))
            return -EFAULT;

	if( strncmp("innolux", msg, strlen("innolux")) == 0)
	{
		lcdc_ili9481_config( NULL, g_spi);
	}else if(strncmp("ilitek", msg, strlen("ilitek")) == 0){
        ilitek_config( NULL, g_spi);
	}else if(strncmp("simple", msg, strlen("simple")) == 0){
        simple_config( NULL, g_spi);
	}else if(strncmp("reset", msg, strlen("reset")) == 0){
        reset_lcmic();
	}else if(strncmp("power", msg, strlen("power")) == 0){
        get_power_mode( NULL, g_spi);
	}else if(strncmp("pixel", msg, strlen("pixel")) == 0){
        get_pixel_mode( NULL, g_spi);
	}else if(strncmp("sleep", msg, strlen("sleep")) == 0){
        sleep_config( NULL, g_spi);
	}else if(strstr( msg, "pixel") ){

	}

    return len;
}


static struct file_operations lcdspi_proc_fops = {
    .owner          = THIS_MODULE,
    .read           = lcdspi_proc_read,
    .write          = lcdspi_proc_write,
};

static int create_lcdspi_proc_file(void)
{
    lcdspi_proc_file = create_proc_entry( LCDSPI_PROC_FILE, 0644, NULL);
    if (!lcdspi_proc_file) {
        printk(KERN_INFO "Create proc file for LCD SPI failed\n");
        return -ENOMEM;
    }

    printk( KERN_INFO "LCD SPI proc OK\n" );
    lcdspi_proc_file->proc_fops = &lcdspi_proc_fops;
    return 0;
}

extern void lcdc_clk_off(void);

#if defined(ILI9481)
static int fih_lcm_is_mddi_type()
{
	return 0;
}
#endif


int fih_lcm_is_ili9481()
{
	if(Project_DPD <=fih_read_product_id_from_orighwid() && fih_read_product_id_from_orighwid() <=Project_FAD){
		return TRUE;
	}else{
		return FALSE;
	}
}

static void disable_mddi_clock(void)
{

	if (mddi_get_client_id() != 0) {
		
    }else{

    }
    printk(KERN_INFO "lcdc_ili9481: disable_mddi_clock()\n");
}

static int __init lcdc_ili9481_init(void)
{
	int ret;
	struct msm_panel_info pinfo;
	
    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: +%s()\n",__func__);

	//open ldo
//	sc668_LDO_setting( 1, LDO_VOLTAGE_2_8, MODULE_NAME);
//	mdelay(10);
    
    if(fih_lcm_is_mddi_type()){
        fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s(): LCM isn't RGB i/f\n",__func__);
        lcdc_clk_off();        
        ret=-ENODEV;
        goto err;
    }
    
	if(!fih_lcm_is_ili9481()){
		goto err;
	}

	/* chandler: disable mddi clock */
	disable_mddi_clock();
	pinfo.xres = 320;
	pinfo.yres = 480;
	pinfo.type = LCDC_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 18;
	pinfo.fb_num = 2;
#if 0    
    if( (FIH_READ_HWID_FROM_SMEM() >= CMCS_RTP_PR2) ) {
        //pinfo.clk_rate = 11360000;
        pinfo.clk_rate = 8192000;
    }else{
        pinfo.clk_rate = 8192000;
    }
#endif	
	//pinfo.clk_rate = 11360000;
	pinfo.clk_rate = 9000000;
	//pinfo.clk_rate = 9000000;

    fih_printk(debug_mask_lcm, FIH_LCM_DBG, KERN_INFO "lcdc_ili9481: %s(): pinfo.clk_rate(%d)\n", __func__, pinfo.clk_rate);
    
/* FIH_ADQ added by Guorui*/
	pinfo.bl_max=100;
	pinfo.bl_min=0;
/* FIH_ADQ */

	pinfo.lcdc.h_back_porch = 3;
	pinfo.lcdc.h_front_porch = 3;
	pinfo.lcdc.h_pulse_width = 2;
	
	pinfo.lcdc.v_back_porch = 2;
	pinfo.lcdc.v_front_porch = 4;
	pinfo.lcdc.v_pulse_width = 2;

	pinfo.lcdc.border_clr = 0;	/* blk */
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
	pinfo.lcdc.hsync_skew = 0;	

	ret = lcdc_device_register(&pinfo);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

    INIT_LIST_HEAD(&dd_list);
    mutex_init(&innolux_dd_lock);

    create_lcdspi_proc_file();

//    ret = spi_register_driver(&lcdc_spi_drv);
err:    return ret;
}

void lcdc_lil9481_trigger_lcm(void)
{
	printk(KERN_ERR "%s()\n", __func__);
	spi_register_driver(&lcdc_spi_drv);
}

EXPORT_SYMBOL(lcdc_lil9481_trigger_lcm);


static void __exit lcdc_ili9481_exit(void)
{
    spi_unregister_driver(&lcdc_spi_drv);
}

module_init(lcdc_ili9481_init);
module_exit(lcdc_ili9481_exit);
