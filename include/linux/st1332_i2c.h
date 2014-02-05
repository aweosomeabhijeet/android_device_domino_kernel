#ifndef ST1332_I2C_H
#define ST1332_I2C_H

#include <linux/ioctl.h>

struct st1332_i2c_platform_data {
	uint16_t version;
	int abs_x_min;
	int abs_x_max;
	int abs_y_min;
	int abs_y_max;
	int intr_gpio;
	int rst_gpio;
	int (*power)(int on);
};

typedef struct {
	u8	y_h: 3,
		reserved: 1,
		x_h: 3,
		valid: 1;
	u8	x_l;
	u8	y_l;
} xy_data_t;

typedef struct {
	u8		fingers: 3,
			gesture: 5;
	u8		keys;
	xy_data_t	xy_data[2];
} stx_report_data_t;

#if 1

struct st1332_i2c_sensitivity {
	int x;
	int y;
};

struct st1332_i2c_resolution {
	int x;
	int y;
};

#define ST1332_IOC_MAGIC    'E'
#define ST1332_IOC_GFWVERSION   _IOR(ST1332_IOC_MAGIC, 0, int)  /* get firmware version */
#define ST1332_IOC_GPWSTATE     _IOR(ST1332_IOC_MAGIC, 1, int)  /* get power state */
#define ST1332_IOC_GORIENTATION _IOR(ST1332_IOC_MAGIC, 2, int)  /* get orientation */
#define ST1332_IOC_GRESOLUTION  _IOR(ST1332_IOC_MAGIC, 3, int)  /* get resolution */
#define ST1332_IOC_GDEEPSLEEP   _IOR(ST1332_IOC_MAGIC, 4, int)  /* get deep sleep function status */
#define ST1332_IOC_GFWID        _IOR(ST1332_IOC_MAGIC, 5, int)  /* get firmware id */
#define ST1332_IOC_GREPORTRATE  _IOR(ST1332_IOC_MAGIC, 6, int)  /* get report rate */
#define ST1332_IOC_GSENSITIVITY _IOR(ST1332_IOC_MAGIC, 7, int)  /* get sensitivity setting */
#define ST1332_IOC_SPWSTATE     _IOW(ST1332_IOC_MAGIC, 8, int)  /* change power state */
#define ST1332_IOC_SORIENTATION _IOW(ST1332_IOC_MAGIC, 9, int)  /* change orientation */
#define ST1332_IOC_SRESOLUTION  _IOW(ST1332_IOC_MAGIC,10, int)  /* change resolution */
#define ST1332_IOC_SDEEPSLEEP   _IOW(ST1332_IOC_MAGIC,11, int)  /* enable or disable deep sleep function */
#define ST1332_IOC_SREPORTRATE  _IOW(ST1332_IOC_MAGIC,12, int)  /* change device report frequency */
#define ST1332_IOC_SSENSITIVITY _IOR(ST1332_IOC_MAGIC,13, int)  /* change sensitivity setting */
#define ST1332_IOC_MAXNR    15
#endif
/* FIH, Henry Juang, 2009/11/20 ++*/
/* [FXX_CR], Add for proximity driver to turn on/off BL and TP. */
int notify_from_proximity(bool bFlag);  //Added for test
/* FIH, Henry Juang, 2009/11/20 --*/
#endif
