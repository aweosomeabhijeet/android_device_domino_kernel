/*
 *     mt9p111.h - Camera Sensor Config
 *
 *     Copyright (C) 2010 Kent Kwan <kentkwan@fihspec.com>
 *     Copyright (C) 2008 FIH CO., Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; version 2 of the License.
 */

#ifndef MT9P111_H
#define MT9P111_H

#include <linux/types.h>
#include <mach/camera.h>

#include <mach/vreg.h>
#include <mach/gpio.h>

int mt9p111_power_on(void);
int mt9p111_power_off(void);

extern struct mt9p111_reg mt9p111_regs;

enum mt9p111_width {
	WORD_LEN,
	BYTE_LEN,
	BYTE_POLL,
	BIT_POLL
};

struct mt9p111_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum mt9p111_width width;
	unsigned short mdelay_time;
};

struct mt9p111_i2c_reg_burst_mode_conf {
	unsigned short waddr;
	unsigned short wdata1;
	unsigned short wdata2;
	unsigned short wdata3;
	unsigned short wdata4;
	unsigned short wdata5;
	unsigned short wdata6;
	unsigned short wdata7;
	unsigned short wdata8;
	enum mt9p111_width width;
	unsigned short mdelay_time;
};

struct mt9p111_reg {
	const struct mt9p111_i2c_reg_conf *prev_snap_reg_settings;
	uint16_t prev_snap_reg_settings_size;
	const struct mt9p111_i2c_reg_conf *noise_reduction_reg_settings;
	uint16_t noise_reduction_reg_settings_size;
	const struct mt9p111_i2c_reg_conf *plltbl;
	uint16_t plltbl_size;
	const struct mt9p111_i2c_reg_conf *stbl;
	uint16_t stbl_size;
	const struct mt9p111_i2c_reg_conf *rftbl;
	uint16_t rftbl_size;
	//FIHLX-D6P2-MM-KK-Aptina5M-0++
	const struct mt9p111_i2c_reg_conf *aftbl;
	uint16_t aftbl_size;
	//FIHLX-D6P2-MM-KK-Aptina5M-0--
	//Div2-SW6-MM-KK-NewTable-0++
	//const struct mt9p111_i2c_reg_conf *patchtbl;
	const struct mt9p111_i2c_reg_burst_mode_conf *patchtbl;
	uint16_t patchtbl_size;
	const struct mt9p111_i2c_reg_conf *chartbl;
	uint16_t chartbl_size;
	const struct mt9p111_i2c_reg_conf *iqtbl;
	uint16_t iqtbl_size;
	//Div2-SW6-MM-KK-NewTable-0--
    //Div6-PT2-MM-CH-CameraSetting-00+[
	const struct mt9p111_i2c_reg_conf *ab_off_tbl;
	uint16_t ab_off_tbl_size;
	const struct mt9p111_i2c_reg_conf *ab_60mhz_tbl;
	uint16_t ab_60mhz_tbl_size;	
	const struct mt9p111_i2c_reg_conf *ab_50mhz_tbl;
	uint16_t ab_50mhz_tbl_size;	
	const struct mt9p111_i2c_reg_conf *ab_auto_tbl;
	uint16_t ab_auto_tbl_size;	
	const struct mt9p111_i2c_reg_conf *br0_tbl;
	uint16_t br0_tbl_size;
	const struct mt9p111_i2c_reg_conf *br1_tbl;
	uint16_t br1_tbl_size;
	const struct mt9p111_i2c_reg_conf *br2_tbl;
	uint16_t br2_tbl_size;
	const struct mt9p111_i2c_reg_conf *br3_tbl;
	uint16_t br3_tbl_size;
	const struct mt9p111_i2c_reg_conf *br4_tbl;
	uint16_t br4_tbl_size;
	const struct mt9p111_i2c_reg_conf *br5_tbl;
	uint16_t br5_tbl_size;
	const struct mt9p111_i2c_reg_conf *br6_tbl;
	uint16_t br6_tbl_size;	
	const struct mt9p111_i2c_reg_conf *const_m2_tbl;
	uint16_t const_m2_tbl_size;
	const struct mt9p111_i2c_reg_conf *const_m1_tbl;
	uint16_t const_m1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *const_zero_tbl;
	uint16_t const_zero_tbl_size;	
	const struct mt9p111_i2c_reg_conf *const_p1_tbl;
	uint16_t const_p1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *const_p2_tbl;
	uint16_t const_p2_tbl_size;	
	const struct mt9p111_i2c_reg_conf *effect_none_tbl;
	uint16_t effect_none_tbl_size;
	const struct mt9p111_i2c_reg_conf *effect_mono_tbl;
	uint16_t effect_mono_tbl_size;	
	const struct mt9p111_i2c_reg_conf *effect_sepia_tbl;
	uint16_t effect_sepia_tbl_size;	
	const struct mt9p111_i2c_reg_conf *effect_negative_tbl;
	uint16_t effect_negative_tbl_size;	
	const struct mt9p111_i2c_reg_conf *effect_solarize_tbl;
	uint16_t effect_solarize_tbl_size;	
	const struct mt9p111_i2c_reg_conf *exp_normal_tbl;
	uint16_t exp_normal_tbl_size;	
	const struct mt9p111_i2c_reg_conf *exp_spot_tbl;
	uint16_t exp_spot_tbl_size;	
	const struct mt9p111_i2c_reg_conf *exp_average_tbl;
	uint16_t exp_average_tbl_size;	
	const struct mt9p111_i2c_reg_conf *satu_m2_tbl;
	uint16_t satu_m2_tbl_size;
	const struct mt9p111_i2c_reg_conf *satu_m1_tbl;
	uint16_t satu_m1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *satu_zero_tbl;
	uint16_t satu_zero_tbl_size;	
	const struct mt9p111_i2c_reg_conf *satu_p1_tbl;
	uint16_t satu_p1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *satu_p2_tbl;
	uint16_t satu_p2_tbl_size;	
	const struct mt9p111_i2c_reg_conf *sharp_m2_tbl;
	uint16_t sharp_m2_tbl_size;
	const struct mt9p111_i2c_reg_conf *sharp_m1_tbl;
	uint16_t sharp_m1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *sharp_zero_tbl;
	uint16_t sharp_zero_tbl_size;	
	const struct mt9p111_i2c_reg_conf *sharp_p1_tbl;
	uint16_t sharp_p1_tbl_size;	
	const struct mt9p111_i2c_reg_conf *sharp_p2_tbl;
	uint16_t sharp_p2_tbl_size;	
	const struct mt9p111_i2c_reg_conf *wb_auto_tbl;
	uint16_t wb_auto_tbl_size;
	const struct mt9p111_i2c_reg_conf *wb_daylight_tbl;
	uint16_t wb_daylight_tbl_size;	
	const struct mt9p111_i2c_reg_conf *wb_cloudy_daylight_tbl;
	uint16_t wb_cloudy_daylight_tbl_size;	
	const struct mt9p111_i2c_reg_conf *wb_incandescent_tbl;
	uint16_t wb_incandescent_tbl_size;	
	const struct mt9p111_i2c_reg_conf *wb_fluorescent_tbl;
	uint16_t wb_fluorescent_tbl_size;		
	const struct mt9p111_i2c_reg_conf *hd_inittbl;
	uint16_t hd_inittbl_size;
	const struct mt9p111_i2c_reg_conf *hd_to_vga_tbl;
	uint16_t hd_to_vga_tbl_size;	
	const struct mt9p111_i2c_reg_conf *iso_auto_tbl;
	uint16_t iso_auto_tbl_size;
	const struct mt9p111_i2c_reg_conf *iso_100_tbl;
	uint16_t iso_100_tbl_size;	
	const struct mt9p111_i2c_reg_conf *iso_200_tbl;
	uint16_t iso_200_tbl_size;	
	const struct mt9p111_i2c_reg_conf *iso_400_tbl;
	uint16_t iso_400_tbl_size;	
	const struct mt9p111_i2c_reg_conf *iso_800_tbl;
	uint16_t iso_800_tbl_size;		
    //Div6-PT2-MM-CH-CameraSetting-00+]
};

#endif /* MT9P111_H */
