/*******************************************************************************
 * Copyright (c) 2009-2016, MAV'RIC Development Team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

/*******************************************************************************
 * \file mavlink_waypoint_handler.cpp
 *
 * \author MAV'RIC Team
 * \author Nicolas Dousse
 * \author Matthew Douglas
 *
 * \brief The MAVLink waypoint handler
 *
 ******************************************************************************/


#include "communication/mavlink_waypoint_handler.hpp"
#include "mission/mission_handler.hpp"

#include "hal/common/time_keeper.hpp"
#include "util/constants.hpp"
#include "util/print_util.hpp"

extern "C"
{
#include "util/maths.h"
}


//------------------------------------------------------------------------------
// PRIVATE FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

mav_result_t Mavlink_waypoint_handler::set_home(Mavlink_waypoint_handler* waypoint_handler, const mavlink_command_long_t* packet)
{
    if (packet->param1 == 0) // Use indicated location
    {
        waypoint_handler->home_waypoint_ = Waypoint(MAV_FRAME_LOCAL_NED,
                                                    MAV_CMD_NAV_LOITER_UNLIM,
                                                    0,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f);
        print_util_dbg_print("[Mavlink_waypoint_handler] New home location set to (");
        print_util_dbg_putfloat(packet->param5, 3);
        print_util_dbg_print(", ");
        print_util_dbg_putfloat(packet->param6, 3);
        print_util_dbg_print(", ");
        print_util_dbg_putfloat(packet->param7, 3);
        print_util_dbg_print(")\r\n");
    }
    else if (packet->param1 == 1) // Use current position
    {
        waypoint_handler->home_waypoint_ = Waypoint(MAV_FRAME_LOCAL_NED,
                                                    MAV_CMD_NAV_LOITER_UNLIM,
                                                    0,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    0.0f,
                                                    waypoint_handler->ins_.position_lf()[X],
                                                    waypoint_handler->ins_.position_lf()[Y],
                                                    waypoint_handler->ins_.position_lf()[Z]);
        print_util_dbg_print("[Mavlink_waypoint_handler] New home location set to current position: (");
        print_util_dbg_putfloat(waypoint_handler->ins_.position_lf()[X], 3);
        print_util_dbg_print(", ");
        print_util_dbg_putfloat(waypoint_handler->ins_.position_lf()[Y], 3);
        print_util_dbg_print(", ");
        print_util_dbg_putfloat(waypoint_handler->ins_.position_lf()[Z], 3);
        print_util_dbg_print(")\r\n");
    }
    else
    {
        return MAV_RESULT_DENIED;
    }

    waypoint_handler->send_home_waypoint();
    return MAV_RESULT_ACCEPTED;
}

void Mavlink_waypoint_handler::send_home_waypoint()
{
    local_position_t local_pos = home_waypoint_.local_pos();
    global_position_t global_pos;
    coord_conventions_local_to_global_position(local_pos, INS::origin(), global_pos);

    float surface_norm[4];

    mavlink_message_t msg;
    mavlink_msg_home_position_pack( mavlink_stream_.sysid(),
                                    mavlink_stream_.compid(),
                                    &msg,
                                    global_pos.latitude*1e7,
                                    global_pos.longitude*1e7,
                                    global_pos.altitude*1e3,
                                    local_pos[X],
                                    local_pos[Y],
                                    local_pos[Z],
                                    surface_norm,   // q: surface norm
                                    0.0f,           // approach_x
                                    0.0f,           // approach_x
                                    0.0f);          // approach_x
    mavlink_stream_.send(&msg);
}

void Mavlink_waypoint_handler::request_list_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_request_list_t packet;

    mavlink_msg_mission_request_list_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        mavlink_message_t _msg;
        mavlink_msg_mission_count_pack(sysid,
                                       waypoint_handler->mavlink_stream_.compid(),
                                       &_msg,
                                       msg->sysid,
                                       msg->compid,
                                       waypoint_handler->waypoint_count_);
        waypoint_handler->mavlink_stream_.send(&_msg);

        print_util_dbg_print("[Mavlink_waypoint_handler] Will send ");
        print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
        print_util_dbg_print(" waypoints\r\n");
    }
}

void Mavlink_waypoint_handler::mission_request_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_request_t packet;

    mavlink_msg_mission_request_decode(msg, &packet);

    print_util_dbg_print("[Mavlink_waypoint_handler] Asking for waypoint number ");
    print_util_dbg_print_num(packet.seq, 10);
    print_util_dbg_print("\r\n");

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER
            || (uint8_t)packet.target_component == 50)) // target_component = 50 is sent by dronekit
    {
        if (packet.seq < waypoint_handler->waypoint_count_)
        {
            uint8_t is_current = 0;
            if (    packet.seq == waypoint_handler->current_waypoint_index_)       // And we are en route
            {
                is_current = 1;
            }

            waypoint_handler->waypoint_list_[packet.seq].send(waypoint_handler->mavlink_stream_, sysid, msg, packet.seq, is_current);

            print_util_dbg_print("[Mavlink_waypoint_handler] Sending waypoint ");
            print_util_dbg_print_num(packet.seq, 10);
            print_util_dbg_print("\r\n");
        }
    }
}

void Mavlink_waypoint_handler::mission_ack_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_ack_t packet;

    mavlink_msg_mission_ack_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        print_util_dbg_print("[Mavlink_waypoint_handler] Acknowledgment received, end of waypoint sending.\r\n");
    }
}

void Mavlink_waypoint_handler::mission_count_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_count_t packet;

    mavlink_msg_mission_count_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        // Erase current waypoint list
        waypoint_handler->waypoint_count_ = 0;

        // Save the number of waypoints GCS is about to send
        if (packet.count > MAX_WAYPOINTS)
        {
            packet.count = MAX_WAYPOINTS;
        }
        waypoint_handler->requested_waypoint_count_ = packet.count;

        print_util_dbg_print("[Mavlink_waypoint_handler] Receiving ");
        print_util_dbg_print_num(packet.count, 10);
        print_util_dbg_print(" new waypoints.");
        print_util_dbg_print("\r\n");

        if (waypoint_handler->requested_waypoint_count_ > 0) // Request first waypoint
        {
            mavlink_message_t _msg;
            mavlink_msg_mission_request_pack(sysid,
                                             waypoint_handler->mavlink_stream_.compid(),
                                             &_msg,
                                             msg->sysid,
                                             msg->compid,
                                             0);
            waypoint_handler->mavlink_stream_.send(&_msg);

            print_util_dbg_print("[Mavlink_waypoint_handler] Asking for waypoint ");
            print_util_dbg_print_num(0, 10);
            print_util_dbg_print("\r\n");
        }
        else // Acknowledge empty waypoint list
        {
            MAV_MISSION_RESULT type = MAV_MISSION_ACCEPTED;

            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         type);
            waypoint_handler->mavlink_stream_.send(&_msg);

            print_util_dbg_print("[Mavlink_waypoint_handler] Flight plan received!\n");
            waypoint_handler->waypoint_received_time_ms_ = time_keeper_get_ms();

            waypoint_handler->send_home_waypoint();
        }
    }
}

void Mavlink_waypoint_handler::mission_item_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_item_t packet;

    mavlink_msg_mission_item_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        Waypoint new_waypoint(packet);

        print_util_dbg_print("[Mavlink_waypoint_handler] New waypoint received. requested num :");
        print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
        print_util_dbg_print(" receiving num :");
        print_util_dbg_print_num(packet.seq, 10);
        print_util_dbg_print("\r\n");

        // Check if this is the requested waypoint
        if (packet.seq == waypoint_handler->waypoint_count_)
        {
            // Check if we know what the waypoint is
            if (waypoint_handler->mission_handler_registry_.get_mission_handler(new_waypoint) != NULL)
            {
                print_util_dbg_print("[Mavlink_waypoint_handler] Receiving good waypoint, number ");
                print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
                print_util_dbg_print(" of ");
                print_util_dbg_print_num(waypoint_handler->requested_waypoint_count_, 10);
                print_util_dbg_print("\r\n");

                // Set waypoint and increment
                waypoint_handler->waypoint_list_[waypoint_handler->waypoint_count_] = new_waypoint;
                waypoint_handler->waypoint_count_++;

                if (waypoint_handler->waypoint_count_ == waypoint_handler->requested_waypoint_count_ || // If we have finished the list...
                    waypoint_handler->waypoint_count_ == MAX_WAYPOINTS)                                 // or we have reached max waypoint capacity...
                {                                                                                       // then accept mission
                    MAV_MISSION_RESULT type = MAV_MISSION_ACCEPTED;

                    mavlink_message_t _msg;
                    mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                                 waypoint_handler->mavlink_stream_.compid(),
                                                 &_msg,
                                                 msg->sysid,
                                                 msg->compid,
                                                 type);
                    waypoint_handler->mavlink_stream_.send(&_msg);

                    print_util_dbg_print("[Mavlink_waypoint_handler] Flight plan received!\n");
                    waypoint_handler->waypoint_received_time_ms_ = time_keeper_get_ms();

                    waypoint_handler->send_home_waypoint();
                }
                else
                {
                    mavlink_message_t _msg;
                    mavlink_msg_mission_request_pack(waypoint_handler->mavlink_stream_.sysid(),
                                                     waypoint_handler->mavlink_stream_.compid(),
                                                     &_msg,
                                                     msg->sysid,
                                                     msg->compid,
                                                     waypoint_handler->waypoint_count_);
                    waypoint_handler->mavlink_stream_.send(&_msg);

                    print_util_dbg_print("[Mavlink_waypoint_handler] Asking for waypoint ");
                    print_util_dbg_print_num(waypoint_handler->waypoint_count_, 10);
                    print_util_dbg_print("\n");
                }
            }
            else    // No mission handler registered in registry
            {
                print_util_dbg_print("[Mavlink_waypoint_handler] Waypoint not registered in registry\r\n");
                MAV_MISSION_RESULT type = MAV_MISSION_UNSUPPORTED;

                mavlink_message_t _msg;
                mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                             waypoint_handler->mavlink_stream_.compid(),
                                             &_msg,
                                             msg->sysid,
                                             msg->compid,
                                             type);
                waypoint_handler->mavlink_stream_.send(&_msg);
            }

        } //end of if (packet.seq == waypoint_handler->waypoint_count_)
        else
        {
            MAV_MISSION_RESULT type = MAV_MISSION_INVALID_SEQUENCE;

            mavlink_message_t _msg;
            mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                         waypoint_handler->mavlink_stream_.compid(),
                                         &_msg,
                                         msg->sysid,
                                         msg->compid,
                                         type);
            waypoint_handler->mavlink_stream_.send(&_msg);
        }
    } //end of if this message is for this system and subsystem
}

void Mavlink_waypoint_handler::mission_clear_all_callback(Mavlink_waypoint_handler* waypoint_handler, uint32_t sysid, const mavlink_message_t* msg)
{
    mavlink_mission_clear_all_t packet;

    mavlink_msg_mission_clear_all_decode(msg, &packet);

    // Check if this message is for this system and subsystem
    if (((uint8_t)packet.target_system == (uint8_t)sysid)
            && ((uint8_t)packet.target_component == (uint8_t)MAV_COMP_ID_MISSIONPLANNER))
    {
        waypoint_handler->waypoint_count_ = 0;

        mavlink_message_t _msg;
        mavlink_msg_mission_ack_pack(waypoint_handler->mavlink_stream_.sysid(),
                                     waypoint_handler->mavlink_stream_.compid(),
                                     &_msg,
                                     msg->sysid,
                                     msg->compid,
                                     MAV_CMD_ACK_OK);
        waypoint_handler->mavlink_stream_.send(&_msg);

        print_util_dbg_print("[Mavlink_waypoint_handler] Cleared Waypoint list.\r\n");
    }
}


//------------------------------------------------------------------------------
// PUBLIC FUNCTIONS IMPLEMENTATION
//------------------------------------------------------------------------------

Mavlink_waypoint_handler::Mavlink_waypoint_handler( const INS& ins,
                                                    Mavlink_message_handler& message_handler,
                                                    const Mavlink_stream& mavlink_stream,
                                                    Mission_handler_registry& mission_handler_registry,
                                                    conf_t config):
    waypoint_count_(0),
    current_waypoint_index_(0),
    mavlink_stream_(mavlink_stream),
    ins_(ins),
    message_handler_(message_handler),
    mission_handler_registry_(mission_handler_registry),
    waypoint_received_time_ms_(0),
    requested_waypoint_count_(0),
    timeout_max_waypoint_(10000),
    config_(config)
{
    for (int i = 0; i < MAX_WAYPOINTS; i++)
    {
        waypoint_list_[i] = Waypoint();
    }

    home_waypoint_ = Waypoint(  MAV_FRAME_LOCAL_NED,
                                MAV_CMD_NAV_LOITER_UNLIM,
                                0,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                config_.home_altitude);
}

bool Mavlink_waypoint_handler::init()
{
    bool init_success = true;

    // Add message callbacks for waypoint handler messages requests
    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_ITEM, // 39
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &mission_item_callback,
                                                        this );

    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_REQUEST, // 40
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &mission_request_callback,
                                                        this );

    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_REQUEST_LIST, // 43
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &request_list_callback,
                                                        this );

    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_COUNT, // 44
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &mission_count_callback,
                                                        this );

    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_CLEAR_ALL, // 45
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &mission_clear_all_callback,
                                                        this );

    init_success &= message_handler_.add_msg_callback(  MAVLINK_MSG_ID_MISSION_ACK, // 47
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        &mission_ack_callback,
                                                        this );

    // Add command callbacks for waypoint handler messages requests
    init_success &= message_handler_.add_cmd_callback(  MAV_CMD_DO_SET_HOME, // 179
                                                        MAVLINK_BASE_STATION_ID,
                                                        MAV_COMP_ID_ALL,
                                                        MAV_COMP_ID_ALL, // 0
                                                        &set_home,
                                                        this );

    send_home_waypoint();

    if(!init_success)
    {
        print_util_dbg_print("[MAVLINK_WAYPOINT_HANDLER] constructor: ERROR\r\n");
    }

    return init_success;
}

const Waypoint& Mavlink_waypoint_handler::current_waypoint() const
{
    // If there are no waypoints set, go to home
    if (waypoint_count_ == 0)
    {
        return home_waypoint();
    }

    // If it is a good index
    if (current_waypoint_index_ >= 0 && current_waypoint_index_ < waypoint_count_)
    {
        return waypoint_list_[current_waypoint_index_];
    }
    else
    {
        // For now, set to home
        return home_waypoint();
    }
}

const Waypoint& Mavlink_waypoint_handler::next_waypoint() const
{
    // If there are no waypoints set, go to home
    if (waypoint_count_ == 0)
    {
        return home_waypoint();
    }

    // If it is a good index
    if (current_waypoint_index_ >= 0 && current_waypoint_index_ < waypoint_count_)
    {
        // Check if the next waypoint exists
        if ((current_waypoint_index_ + 1) != waypoint_count_)
        {
            return waypoint_list_[current_waypoint_index_ + 1];
        }
        else // No next waypoint, set to first
        {
            return waypoint_list_[0];
        }
    }
    else
    {
        // For now, set to home
        return home_waypoint();
    }
}

const Waypoint& Mavlink_waypoint_handler::waypoint_from_index(int i) const
{
    // If there are no waypoints set, go to home
    if (waypoint_count_ == 0)
    {
        return home_waypoint();
    }

    if (i >= 0 && i < waypoint_count_)
    {
        return waypoint_list_[i];
    }
    else
    {
        return home_waypoint();
    }
}

const Waypoint& Mavlink_waypoint_handler::home_waypoint() const
{
    return home_waypoint_;
}

void Mavlink_waypoint_handler::advance_to_next_waypoint()
{
    // If the current waypoint index is the last waypoint, go to first waypoint
    if (current_waypoint_index_ == (waypoint_count_-1))
    {
        set_current_waypoint_index(0);
    }
    else // Update current in both waypoints
    {
        set_current_waypoint_index(current_waypoint_index_+1);
    }
}

uint16_t Mavlink_waypoint_handler::current_waypoint_index() const
{
    return current_waypoint_index_;
}

bool Mavlink_waypoint_handler::set_current_waypoint_index(int index)
{
    if (index >= 0 && index < waypoint_count_)
    {
        current_waypoint_index_ = index;

        print_util_dbg_print("[Mavlink_waypoint_handler] Set current waypoint to number");
        print_util_dbg_print_num(index, 10);
        print_util_dbg_print("\r\n");

        return true;
    }
    else
    {
        return false;
    }
}
