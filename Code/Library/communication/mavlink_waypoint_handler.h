/**
 * \page The MAV'RIC License
 *
 * The MAV'RIC Framework
 *
 * Copyright � 2011-2014
 *
 * Laboratory of Intelligent Systems, EPFL
 */


/**
 * \file mavlink_waypoint_handler.h
 * 
 * The mavlink waypoint handler
 */


#ifndef MAVLINK_WAYPOINT_HANDLER__
#define MAVLINK_WAYPOINT_HANDLER__

#ifdef __cplusplus
extern "C" {
#endif

#include "mavlink_stream.h"
#include "stdbool.h"
#include "coord_conventions.h"

#define MAX_WAYPOINTS 10

/*
 * N.B.: Frames, MAV_CMD_NAV are defined in "maveric.h"
 */

typedef struct
{
	uint8_t frame;
	uint16_t waypoint_id;
	uint8_t current;
	uint8_t autocontinue;
	float param1;   // hold time in seconds
	float param2;   // acceptance radius
	float param3;   // loiter radius
	float param4;   // desired heading at waypoint
	double x;
	double y;
	double z;
}waypoint_struct;


typedef enum critical_behavior_enum
{
	CLIMB_TO_SAFE_ALT,
	FLY_TO_HOME_WP,
	CRITICAL_LAND
}critical_behavior_enum;

typedef enum auto_landing_enum_t
{
	DESCENT_TO_SMALL_ALTITUDE,
	DESCENT_TO_GND
}auto_landing_enum_t;

int sending_waypoint_num;
int waypoint_request_number;

uint16_t num_waypoint_onboard;

uint32_t start_timeout;
uint32_t timeout_max_waypoint;


void init_waypoint_handler(void);
void init_waypoint(void);
void init_homing_waypoint(waypoint_struct waypoint_list[],uint16_t* number_of_waypoints);
void init_waypoint_list(waypoint_struct waypoint_list[], uint16_t* number_of_waypoints);

void send_count(Mavlink_Received_t* rec, uint16_t num_of_waypoint, bool* waypoint_receiving, bool * waypoint_sending);
void send_waypoint(Mavlink_Received_t* rec, waypoint_struct waypoint[], uint16_t num_of_waypoint, bool* waypoint_sending);
void receive_ack_msg(Mavlink_Received_t* rec, bool* waypoint_sending);

void receive_count(Mavlink_Received_t* rec, uint16_t* number_of_waypoints, bool* waypoint_receiving, bool* waypoint_sending);
void receive_waypoint(Mavlink_Received_t* rec,  waypoint_struct waypoint_list[], uint16_t number_of_waypoints, bool* waypoint_receiving);
void set_current_waypoint(Mavlink_Received_t* rec,  waypoint_struct waypoint_list[], uint16_t num_of_waypoint);
void set_current_waypoint_from_parameter(waypoint_struct waypoint_list[], uint16_t num_of_waypoint, uint16_t new_current);
void clear_waypoint_list(Mavlink_Received_t* rec,  uint16_t* number_of_waypoints, bool* waypoint_set);
void set_home(Mavlink_Received_t* rec);

void set_mav_mode(Mavlink_Received_t* rec, uint8_t* board_mav_mode, uint8_t* board_mav_state, uint8_t sim_mode);

void control_time_out_waypoint_msg(uint16_t* num_of_waypoint, bool* waypoint_receiving, bool* waypoint_sending);

local_coordinates_t set_waypoint_from_frame(waypoint_struct current_waypoint, global_position_t origin);
void waypoint_hold_init(local_coordinates_t localPos);
void waypoint_take_off(void);

void waypoint_hold_position_handler(void);
void waypoint_navigation_handler(void);
void waypoint_critical_handler(void);

void auto_landing(void);
void continueToNextWaypoint(void);

void set_circle_scenario(waypoint_struct waypoint_list[], uint16_t* number_of_waypoints, float circle_radius, float num_of_vhc);

// TODO: Add code in the function :)
//void set_stream_scenario(waypoint_struct waypoint_list[], uint16_t* number_of_waypoints, float circle_radius, float num_of_vhc);

#ifdef __cplusplus
}
#endif



#endif // MAVLINK_WAYPOINT_HANDLER__