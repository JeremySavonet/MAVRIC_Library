/*
 * qfilter.c
 * Quaternion complementary attitude filter
 * 
 *  Created on: Apr 13, 2010
 *      Author: Felix Schill
 */

#ifndef QFILTER_H_
#define QFILTER_H_
#include "compiler.h"

#define GYRO_LPF 0.1
#define ACC_LPF 0.05
#define MAG_LPF 0.1


#define GRAVITY 9.81

#include "coord_conventions.h"

enum calibration_mode {OFF, LEVELING, LEVEL_PLUS_ACCEL};

typedef struct {
	UQuat_t qe;
	UQuat_t up_vec, north_vec;
	
	float be[9], sf[9];
	float om[3], a[3], mag[3];
	float kp;
	float ki;
	float kp_mag;
	float ki_mag;
	float raw_mag_mean[3];
	uint8_t calibration_level;
	float heading;
	float acc_bf[3];
} Quat_Attitude_t;	

//float dt;

void qfInit(Quat_Attitude_t *attitude, float *scalefactor, float *bias);

void qfilter(Quat_Attitude_t *attitude, float *rates, float dt, bool simu_mode);

#endif /* QFILTER_H_ */
