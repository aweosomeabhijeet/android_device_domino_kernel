/*
*     ftmgpio.h - GPIO access driver header for FTM use
*
*     Copyright (C) 2009 Clement Hsu <clementhsu@tp.cmcs.com.tw>
*     Copyright (C) 2009 Chi Mei Communication Systems Inc.
*
*     This program is free software; you can redistribute it and/or modify
*     it under the terms of the GNU General Public License as published by
*     the Free Software Foundation; version 2 of the License.
*/

// ioctl operation definition for GPIO device driver
#define IOCTL_GPIO_INIT		1    
#define IOCTL_GPIO_SET		2    
#define IOCTL_GPIO_GET		3    
#define IOCTL_POWER_OFF		4
//FIH, MichaelKao, 2009/7/9++
/* [FXX_CR], Add for get Hardware version*/
#define IOCTL_HV_GET		5
//FIH, MichaelKao, 2009/7/9++
//[+++]FIH, ChiaYuan, 2007/07/10
// [FXX_CR] Put the modem into LPM mode for FTM test
#define IOCTL_MODEM_LPM		6
//[---]FIH, ChiaYuan, 2007/07/10
//FIH, Henry Juang, 2009/08/10++
/* [FXX_CR], Add for FTM PID and testflag*/
#define IOCTL_PID_GET		7
#define IOCTL_PID_SET		8
#define IOCTL_TESTFLAG_GET		9
#define IOCTL_TESTFLAG_SET		10
//FIH, Henry Juang, 2009/08/10--
//FIH, MichaelKao, 2009/08/28++
/* [FXX_CR], Add for Set Mic Bias*/
#define IOCTL_MICBIAS_SET		11
//FIH, MichaelKao, 2009/08/28++

//FIH, Henry Juang, 20091116 ++
/*Add for modifing UART3 source clock*/
#define IOCTL_UART_BAUDRATE_460800		12
//FIH, Henry Juang, 20091116 --

//FIH, Henry Juang, 20100426 ++
/*Add for write NV 8029.*/
#define IOCTL_WRITE_NV_8029		13
//FIH, Henry Juang, 20100426 --
#define IOCTL_CAL_STATUS1		14
#define IOCTL_CAL_STATUS2		15
#define IOCTL_CAL_STATUS3		16
#define IOCTL_CAL_STATUS4		17

//FIH,NeoChen, for read&write FUSE,20100727 ++
#define IOCTL_WRITE_FUSE		18
#define IOCTL_READ_FUSE			19
//FIH,NeoChen, for read&write FUSE,20100727 --
//FIH,Henry Juang, for GRE power key detection,20100816 ++
#define IOCTL_READ_PWR_KEY_DETECTION			20
//FIH,Henry Juang, for GRE power key detection,20100816 --

/* 20101207, SquareCHFang, NVBackup { */
#define IOCTL_BACKUP_UNIQUE_NV      24
#define IOCTL_HW_RESET      25
#define IOCTL_BACKUP_NV_FLAG      27
/* } 20101207, SquareCHFang, NVBackup */

// GPIO direction definition
#define GPIO_IN			0
#define GPIO_OUT		1

// GPIO Output pin High/Low definition
#define GPIO_LOW		0
#define GPIO_HIGH		1


static int ftmgpio_dev_open( struct inode * inode, struct file * file );

static ssize_t ftmgpio_dev_read( struct file * file, char __user * buffer, size_t size, loff_t * f_pos );

static int ftmgpio_dev_write( struct file * filp, const char __user * buffer, size_t length, loff_t * offset );

static int ftmgpio_dev_ioctl( struct inode * inode, struct file * filp, unsigned int cmd, unsigned long arg );

static int __init ftmgpio_init(void);

static void __exit ftmgpio_exit(void);                         


















