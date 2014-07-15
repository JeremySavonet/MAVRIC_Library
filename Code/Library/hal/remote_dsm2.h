/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright © 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */


/**
* \file remote_dsm2.h
*
* This file is the driver for the remote control
*/


#ifndef REMOTE_DSM2_
#define REMOTE_DSM2_

#ifdef __cplusplus
	extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "buffer.h"
#include "stabilisation.h"

#define REMOTE_UART AVR32_USART1						///< Define the microcontroller pin map with the remote UART
#define DSM_RECEIVER_PIN AVR32_PIN_PD12					///< Define the microcontroller pin map with the receiver pin
#define RECEIVER_POWER_ENABLE_PIN AVR32_PIN_PC01		///< Define the microcontroller pin map with the receiver power enable pin
//#define SPEKTRUM_10BIT


/**
 * \brief Structure containing the Spektrum receiver's data
 */
typedef struct 
{
	Buffer_t receiver;				///< Define a buffer for the receiver
	int16_t channels[16];			///< Define an array to contain the 16 remote channels availbale
	uint32_t last_update;			///< Define the last update time 
	uint8_t valid;					///< Define whether a data is valid or not
	uint32_t last_time;				///< Define last time
	uint32_t duration;				///< Define the duration
} Spektrum_Receiver_t;


/**
 * \brief Power-on/off the receiver
 *
 * \param on if true power-on the receiver, false not implemented yet
 */
void remote_dsm2_rc_switch_power(bool on);


/**
 * \brief set slave receiver into bind mode. 
 * has to be called 100ms after power-up
 */
void remote_dsm2_rc_activate_bind_mode(void);


/**
 * \brief Initialize UART receiver for Spektrum/DSM2 slave receivers
 */
void remote_dsm2_rc_init (void);


/**
 * \brief Return a remote channel
 *
 * \param index Specify which channel we are interested in
 *
 * \return the remote channel value
 */
int16_t remote_dsm2_rc_get_channel(uint8_t index);


/**
 * \brief Update the remote channel central position array stored in .c file
 * Warning: you should ensure first that the remote has the stick in their neutral position first
 *
 * \param index Specify which channel we are interested in
 */
void remote_dsm2_rc_center_channel(uint8_t index);


/**
 * \brief Return the neutral position of a remote channel
 * Warning: you should ensure first that the remote has the stick in their neutral position first
 *
 * \param index Specify which channel we are interested in
 *
 * \return the neutral position of this remote channel
 */
int16_t remote_dsm2_rc_get_channel_neutral(uint8_t index);


/**
 * \brief return 1 if enabled receivers works
 *
 * \return the error value of receivers' checks: 0 for no error
 */
int8_t  remote_dsm2_rc_check_receivers(void);


/*
Control_Command_t remote_dsm2_get_command_from_spektrum();
float remote_dsm2_get_roll_from_spektrum();
float remote_dsm2_get_pitch_from_spektrum();
float remote_dsm2_get_yaw_from_spektrum();
float remote_dsm2_get_thrust_from_spektrum();

void remote_dsm2_get_channel_mode_spektrum(uint8_t *chanSwitch);
*/

#ifdef __cplusplus
	}
#endif

#endif /* REMOTE_DSM2_ */
