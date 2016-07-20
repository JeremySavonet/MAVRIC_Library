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
 * \file ins_kf.hpp
 *
 * \author MAV'RIC Team
 * \author Julien Lecoeur
 * \author Simon Pyroth
 *
 * \brief   Kalman filter for position estimation
 *
 ******************************************************************************/


#ifndef INS_KF_HPP_
#define INS_KF_HPP_


#include "drivers/gps.hpp"
#include "drivers/barometer.hpp"
#include "drivers/sonar.hpp"
#include "drivers/px4flow_i2c.hpp"
#include "sensing/ahrs.hpp"
#include "sensing/ins.hpp"

#include "util/kalman.hpp"
#include "util/constants.hpp"

extern "C"
{
#include "sensing/altitude.h"
}

/**
 * \brief   Altitude estimator
 *
 * \details
 * - state vector X (n=11): X = [ x_ned, y_ned, z_ned,
                                 z_ground_ned,
                                 vx, vy, vz,
                                 bias_accx, bias_accy, bias_accz
                                 bias_baro ]

   - model A = [ 1, 0,  0,  0,  dt, 0,  0,  -(1/2)*alpha_x*(dt^2),  -(1/2)*beta_x*(dt^2),   -(1/2)*gamma_x*(dt^2), 0 ]  // Pos x integrated with vel and inclue acc biases in global ref
               [ 0, 1,  0,  0,  0,  dt, 0,  -(1/2)*alpha_y*(dt^2),  -(1/2)*beta_y*(dt^2),   -(1/2)*gamma_y*(dt^2), 0 ]  // Pos y integrated with vel and inclue acc biases in global ref
               [ 0, 0,  1,  0,  0,  0,  dt, -(1/2)*alpha_z*(dt^2),  -(1/2)*beta_z*(dt^2),   -(1/2)*gamma_z*(dt^2), 0 ]  // Pos z integrated with vel and inclue acc biases in global ref
               [ 0, 0,  0,  1,  0,  0,  0,  0,                      0,                      0,                     0 ]  // Ground alt (cst)
               [ 0, 0,  0,  0,  1,  0,  0,  -alpha_x*dt,            -beta_x*dt,             -gamma_x*dt,           0 ]  // Vel x include acc bias in global ref
               [ 0, 0,  0,  0,  0,  1,  0,  -alpha_y*dt,            -beta_y*dt,             -gamma_y*dt,           0 ]  // Vel y include acc bias in global ref
               [ 0, 0,  0,  0,  0,  0,  1,  -alpha_z*dt,            -beta_z*dt,             -gamma_z*dt,           0 ]  // Vel z include acc bias in global ref
               [ 0, 0,  0,  0,  0,  0,  0,  1,                      0,                      0,                     0 ]  // Acc x bias (cst)
               [ 0, 0,  0,  0,  0,  0,  0,  0,                      1,                      0,                     0 ]  // Acc y bias (cst)
               [ 0, 0,  0,  0,  0,  0,  0,  0,                      0,                      1,                     0 ]  // Acc z bias (cst)
               [ 0, 0,  0,  0,  0,  0,  0,  0,                      0,                      0,                     1 ]  // baro bias (cst)
       with:    alpha_x = q0^2 + q1^2 - q2^2 - q3^2     (using attitude quaternion q = (q0, q1, q2, q3)')
                beta_x  = 2*(-q0*q3 + q1*q2)
                gamma_x = 2*(q0*q2 + q1*q3)
                alpha_y = 2*(q0*q3 + q1*q2)
                beta_y  = q0^2 - q1^2 + q2^2 - q3^2
                gamma_y = 2*(-q0*q1 + q2*q3)
                alpha_z = 2*(-q0*q2 + q1*q3)
                beta_z  = 2*(q0*q1 + q2*q3)
                gamma_z = q0^2 - q1^2 - q2^2 - q3^2

   - input u (p=3):         u = [ ax, ay, az ]  // Accelerations in body frame

   - input model B = [ (1/2)*alpha_x*(dt^2),    (1/2)*beta_x*(dt^2),    (1/2)*gamma_x*(dt^2) ]
                     [ (1/2)*alpha_y*(dt^2),    (1/2)*beta_y*(dt^2),    (1/2)*gamma_y*(dt^2) ]
                     [ (1/2)*alpha_z*(dt^2),    (1/2)*beta_z*(dt^2),    (1/2)*gamma_z*(dt^2) ]
                     [ 0,                       0,                      0                    ]
                     [ alpha_x*dt,              beta_x*dt,              gamma_x*dt           ]
                     [ alpha_y*dt,              beta_y*dt,              gamma_y*dt           ]
                     [ alpha_z*dt,              beta_z*dt,              gamma_z*dt           ]
                     [ 0,                       0,                      0                    ]
                     [ 0,                       0,                      0                    ]
                     [ 0,                       0,                      0                    ]
                     [ 0,                       0,                      0                    ]

   - measurement 1 (gps) : z1 = [ x_ned, y_ned, z_ned ]
                           H1 = [ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
                                [ 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]
                                [ 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 ]

   - measurement 2 (gps) : z2 = [ vx, vy, vz ]
                           H2 = [ 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 ]
                                [ 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 ]
                                [ 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 ]

   - measurement 3 (baro): z3 = [ -z_ned - bias_baro ]
                           H3 = [ 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, -1 ]

   - measurement 4 (sonar): z4 = [ -z_ned + z_ground_ned ]
                            H4 = [ 0, 0, -1, 1, 0, 0, 0, 0, 0, 0, 0 ]
 */
class INS_kf: public Kalman<11,3,3>, public INS
{
public:
    /**
     * \brief   Configuration structure
     */
    struct conf_t
    {
        // Sampling time
        float dt;
        // Process covariance
        float sigma_bias_acc;
        float sigma_z_gnd;
        float sigma_bias_baro;
        float sigma_acc;
        // Measurement covariance
        float sigma_gps_xy;
        float sigma_gps_z;
        float sigma_gps_velx;
        float sigma_gps_vely;
        float sigma_gps_velz;
        float sigma_baro;
        float sigma_sonar;
        // Position of the origin
        global_position_t origin;
        // Logic flags
        bool use_ekf;
        bool constant_covar;
    };

    /**
     * \brief Constructor
     */
    INS_kf(const Gps& gps,
              const Barometer& barometer,
              const Sonar& sonar,
              const Px4flow_i2c& flow,
              const ahrs_t& ahrs,
              const conf_t config = default_config() );


    /**
     * \brief   Main update function
     *
     * \return  Success
     */
    bool update(void);


    /**
     * \brief     Last update in seconds
     *
     * \return    time
     */
    float last_update_s(void) const;


    /**
     * \brief     3D Position in meters (NED frame)
     *
     * \return    position
     */
    std::array<float,3> position_lf(void) const;


    /**
     * \brief     Velocity in meters/seconds in NED frame
     *
     * \return    velocity
     */
    std::array<float,3> velocity_lf(void) const;


    /**
     * \brief     Absolute altitude above sea level in meters (>=0)
     *
     * \return    altitude
     */
    float absolute_altitude(void) const;


    /**
     * \brief   Indicates which estimate can be trusted
     *
     * \param   type    Type of estimate
     *
     * \return  boolean
     */
    bool is_healthy(INS::healthy_t type) const;


    /**
     * \brief   Default configuration structure
     */
    static inline INS_kf::conf_t default_config(void);


private:
    const Gps&          gps_;             ///< Gps (input)
    const Barometer&    barometer_;       ///< Barometer (input)
    const Sonar&        sonar_;           ///< Sonar, must be downward facing (input)
    const Px4flow_i2c&  flow_;            ///< Optical flow sensor (input)
    const ahrs_t&       ahrs_;            ///< Attitude and acceleration (input)

    conf_t config_;                      ///< Configuration

    std::array<float,3> pos_;
    std::array<float,3> vel_;
    float absolute_altitude_;

    Mat<3,11> H_gpsvel_;
    Mat<3,3> R_gpsvel_;
    Mat<1,11> H_baro_;
    Mat<1,1> R_baro_;
    Mat<1,11> H_sonar_;
    Mat<1,1> R_sonar_;
    Mat<3,11> H_flow_;
    Mat<3,3> R_flow_;

    float last_accel_update_s_;          ///< Last time we updated the estimate using accelerometer
    float last_sonar_update_s_;          ///< Last time we updated the estimate using sonar
    float last_flow_update_s_;           ///< Last time we updated the estimate using optical flow
    float last_baro_update_s_;           ///< Last time we updated the estimate using barometer
    float last_gps_pos_update_s_;        ///< Last time we updated the estimate using gps position
    float last_gps_vel_update_s_;        ///< Last time we updated the estimate using gps velocity

    /**
     * \brief   Performs the prediction step of the Kalman filter, using linear formulation (KF, non-constant matrices)
     */
    void predict_kf(void);

    /**
     * \brief   Performs the prediction step of the Kalman filter, using extended formulation (EKF)
     */
    void predict_ekf(void);
};


INS_kf::conf_t INS_kf::default_config(void)
{
    INS_kf::conf_t conf = {};

    conf.dt = 0.004f;

    // Process covariance (noise from state and input)
    conf.sigma_z_gnd        = 0.001f;
    conf.sigma_bias_acc     = 0.01f;
    conf.sigma_bias_baro    = 0.001f;
    conf.sigma_acc          = 0.01f;    // Input

    // Measurement covariance   (noise from measurement)
    conf.sigma_gps_xy   = 0.001f;     // Low values for optitrack
    conf.sigma_gps_z    = 0.001f;     // Old: conf.sigma_gps_xy = 2.0f;     conf.sigma_gps_z  = 3.0f;
    conf.sigma_gps_velx = 0.01f;
    conf.sigma_gps_vely = 0.01f;
    conf.sigma_gps_velz = 0.01f;
    conf.sigma_baro     = 10.0f;
    conf.sigma_sonar    = 0.1f;

    //default home location (EFPL Esplanade)
    conf.home = ORIGIN_EPFL;

    // Logic parameters
    conf.use_ekf = false;
    conf.constant_covar = true;

    return conf;
};

static inline bool task_ins_kf_update(INS_kf* ins_kf)
{
    return ins_kf->update();
}

#endif /* INS_KF_HPP_ */
