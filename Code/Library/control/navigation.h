/*
 * navigation.h
 *
 *  Created: 13.08.2013 11:56:46
 *  Author: ndousse
 */ 


#ifndef NAVIGATION_H_
#define NAVIGATION_H_

#include "qfilter.h"
#include "mavlink_waypoint_handler.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_nav(void);

void run_navigation(local_coordinates_t waypoint_input);

float set_rel_pos_n_dist2wp(float waypointPos[], float rel_pos[]);

void set_speed_command(float rel_pos[], float dist2wpSqr);

#ifdef __cplusplus
}
#endif

#endif // NAVIGATION_H_