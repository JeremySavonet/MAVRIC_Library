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
 * \file main.cpp
 *
 * \author MAV'RIC Team
 *
 * \brief Main file
 *
 ******************************************************************************/

#include "sample_projects/LEQuad/lequad.hpp"
#include "sample_projects/LEQuad/hexhog.hpp"

#include "boards/megafly_rev4/megafly_rev4.hpp"

// #include "hal/dummy/file_dummy.hpp"
#include "hal/avr32/file_flash_avr32.hpp"
#include "hal/avr32/serial_usb_avr32.hpp"

// //uncomment to go in simulation
// #include "simulation/dynamic_model_quad_diag.hpp"
// #include "simulation/simulation.hpp"
// #include "hal/dummy/adc_dummy.hpp"
// #include "hal/dummy/pwm_dummy.hpp"
#include "hal/common/time_keeper.hpp"

extern "C"
{
#include "util/print_util.h"
#include "hal/piezo_speaker.h"
#include "libs/asf/avr32/services/delay/delay.h"

#include "sample_projects/LEQuad/proj_avr32/config/conf_imu.hpp"
}

// #include "hal/common/dbg.hpp"
void px4_update(I2c& i2c);

int main(void)
{
    bool init_success = true;

    // -------------------------------------------------------------------------
    // Create board
    // -------------------------------------------------------------------------
    megafly_rev4_conf_t board_config    = megafly_rev4_default_config();

    // Rotate board
    imu_conf_t imu_config = board_config.imu_config;
    uint32_t x_axis = 1;
    float x_sign  = -1.0f;
    uint32_t y_axis = 0;
    float y_sign  = 1.0f;
    uint32_t z_axis = 2;
    float z_sign  = 1.0f;
    board_config.imu_config.accelerometer.sign[0] = x_sign * imu_config.accelerometer.sign[x_axis];
    board_config.imu_config.accelerometer.sign[1] = y_sign * imu_config.accelerometer.sign[y_axis];
    board_config.imu_config.accelerometer.sign[2] = z_sign * imu_config.accelerometer.sign[z_axis];
    board_config.imu_config.accelerometer.axis[0] = imu_config.accelerometer.axis[x_axis];
    board_config.imu_config.accelerometer.axis[1] = imu_config.accelerometer.axis[y_axis];
    board_config.imu_config.accelerometer.axis[2] = imu_config.accelerometer.axis[z_axis];
    board_config.imu_config.gyroscope.sign[0] = x_sign * imu_config.gyroscope.sign[x_axis];
    board_config.imu_config.gyroscope.sign[1] = y_sign * imu_config.gyroscope.sign[y_axis];
    board_config.imu_config.gyroscope.sign[2] = z_sign * imu_config.gyroscope.sign[z_axis];
    board_config.imu_config.gyroscope.axis[0] = imu_config.gyroscope.axis[x_axis];
    board_config.imu_config.gyroscope.axis[1] = imu_config.gyroscope.axis[y_axis];
    board_config.imu_config.gyroscope.axis[2] = imu_config.gyroscope.axis[z_axis];
    board_config.imu_config.magnetometer.sign[0] = x_sign * imu_config.magnetometer.sign[x_axis];
    board_config.imu_config.magnetometer.sign[1] = y_sign * imu_config.magnetometer.sign[y_axis];
    board_config.imu_config.magnetometer.sign[2] = z_sign * imu_config.magnetometer.sign[z_axis];
    board_config.imu_config.magnetometer.axis[0] = imu_config.magnetometer.axis[x_axis];
    board_config.imu_config.magnetometer.axis[1] = imu_config.magnetometer.axis[y_axis];
    board_config.imu_config.magnetometer.axis[2] = imu_config.magnetometer.axis[z_axis];

    Megafly_rev4 board = Megafly_rev4(board_config);

    // Board initialisation
    init_success &= board.init();

    fat_fs_mounting_t fat_fs_mounting;

    fat_fs_mounting_init(&fat_fs_mounting);

    File_fat_fs file_log(true, &fat_fs_mounting); // boolean value = debug mode
    File_fat_fs file_stat(true, &fat_fs_mounting); // boolean value = debug mode

    // -------------------------------------------------------------------------
    // Create MAV
    // -------------------------------------------------------------------------
    // Create MAV using real sensors
    // LEQuad::conf_t mav_config = LEQuad::default_config(MAVLINK_SYS_ID);
    // LEQuad mav = LEQuad(board.imu,
    Hexhog::conf_t mav_config = Hexhog::default_config(MAVLINK_SYS_ID);
    Hexhog mav = Hexhog(board.imu,
                        board.bmp085,
                        board.gps_ublox,
                        board.sonar_i2cxl,      // Warning:
                        board.uart0,
                        board.spektrum_satellite,
                        board.green_led,
                        board.file_flash,
                        board.battery,
                        board.servo_0,
                        board.servo_1,
                        board.servo_2,
                        board.servo_3,
                        board.servo_4,
                        board.servo_5,
                        board.servo_6,
                        board.servo_7,
                        file_log,
                        file_stat,
                        board.i2c1,
                        mav_config );

    // -------------------------------------------------------------------------
    // Create simulation
    // -------------------------------------------------------------------------
    // // Simulated servos
    // Pwm_dummy pwm[4];
    // Servo sim_servo_0(pwm[0], servo_default_config_esc());
    // Servo sim_servo_1(pwm[1], servo_default_config_esc());
    // Servo sim_servo_2(pwm[2], servo_default_config_esc());
    // Servo sim_servo_3(pwm[3], servo_default_config_esc());

    // // Simulated dynamic model
    // Dynamic_model_quad_diag sim_model    = Dynamic_model_quad_diag(sim_servo_0, sim_servo_1, sim_servo_2, sim_servo_3);
    // Simulation sim                       = Simulation(sim_model);

    // // Simulated battery
    // Adc_dummy    sim_adc_battery = Adc_dummy(11.1f);
    // Battery  sim_battery     = Battery(sim_adc_battery);

    // // Simulated IMU
    // Imu      sim_imu         = Imu(  sim.accelerometer(),
    //                                  sim.gyroscope(),
    //                                  sim.magnetometer() );

    // // set the flag to simulation
    // mav_config.state_config.simulation_mode = HIL_ON;
    // LEQuad mav = LEQuad( MAVLINK_SYS_ID,
    //                              sim_imu,
    //                              sim.barometer(),
    //                              sim.gps(),
    //                              sim.sonar(),
    //                              board.uart0,                // mavlink serial
    //                              board.spektrum_satellite,
    //                              board.green_led,
    //                              board.file_flash,
    //                              sim_battery,
    //                              sim_servo_0,
    //                              sim_servo_1,
    //                              sim_servo_2,
    //                              sim_servo_3 ,
    //                              file_log,
    //                              file_stat,
    //                              mav_config );

    if (init_success)
    {
        piezo_speaker_quick_startup();

        // Switch off red LED
        board.red_led.off();
    }
    else
    {
        piezo_speaker_critical_error_melody();
    }

    print_util_dbg_print("[MAIN] OK. Starting up.\r\n");
mav.get_scheduler().add_task(1000000, (Scheduler_task::task_function_t)&px4_update, (Scheduler_task::task_argument_t)&board.i2c1);

    // -------------------------------------------------------------------------
    // Main loop
    // -------------------------------------------------------------------------
    mav.loop();

    return 0;
}


#define FLOW_STAT_COMMAND 50
#define PX4_ADDRESS 0x42+3
#define SECTOR_COUNT 6


// struct flow_stat_frame{
//     int16_t maxima[SECTOR_COUNT];
//     uint8_t max_pos[SECTOR_COUNT];
//     int16_t minima[SECTOR_COUNT];
//     uint8_t min_pos[SECTOR_COUNT];
//     int16_t stddev[SECTOR_COUNT];
//     int16_t avg[SECTOR_COUNT];
// };

struct flow_stat_frame_t{
    int16_t maxima[SECTOR_COUNT];
    uint8_t max_pos[SECTOR_COUNT];
    int16_t minima[SECTOR_COUNT];
    uint8_t min_pos[SECTOR_COUNT];
    int16_t stddev[SECTOR_COUNT];
    int16_t avg[SECTOR_COUNT];
};

static inline void swap_bytes(uint16_t* buffer, uint8_t size)
{
    uint16_t* end = buffer + size;
    uint8_t* b1 = reinterpret_cast<uint8_t*>(buffer);
    uint8_t* b2 = b1+1;
    for(; buffer < end; buffer++, b1+=2, b2+=2){
        *buffer = ((int16_t)(*b2 << 8 | *b1));
    }
}


#define FLOW_STAT_FRAME_SIZE sizeof(flow_stat_frame_t)

void print_frame(flow_stat_frame_t& f)
{
    print_util_dbg_print("Maxima:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.maxima[i],10);
    print_util_dbg_print("\r\nMax Pos:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.max_pos[i],10);
    print_util_dbg_print("\r\nMinima:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.minima[i],10);
    print_util_dbg_print("\r\nMin Pos:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.min_pos[i],10);
    print_util_dbg_print("\r\nSTDDEV:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.stddev[i],10);
    print_util_dbg_print("\r\nAvg:  ");
    for(uint8_t i = 0; i < SECTOR_COUNT; i++)
        print_util_dbg_print_num(f.avg[i],10);
    print_util_dbg_print("\r\n");
}

void px4_update(I2c& i2c)
{
    uint8_t flow_stat_command = FLOW_STAT_COMMAND;
    i2c.write(&flow_stat_command, 1, PX4_ADDRESS);

    flow_stat_frame_t frame;
    if(i2c.read(reinterpret_cast<uint8_t*>(&frame), FLOW_STAT_FRAME_SIZE, PX4_ADDRESS))
    {
        swap_bytes(reinterpret_cast<uint16_t*>(&(frame.maxima)), SECTOR_COUNT);
        swap_bytes(reinterpret_cast<uint16_t*>(&(frame.minima)), SECTOR_COUNT);
        swap_bytes(reinterpret_cast<uint16_t*>(&(frame.stddev)), SECTOR_COUNT);
        swap_bytes(reinterpret_cast<uint16_t*>(&(frame.avg)), SECTOR_COUNT);
        
        //swap_bytes(reinterpret_cast<uint16_t*>(&(frame.minima)), SECTOR_COUNT);
        print_util_dbg_print("PX4 read\r\n");
        print_frame(frame);
    }
}
