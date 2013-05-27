/*
 * estimator.c
 *
 *  Created on: April 30, 2013
 *      Author: Philippe
 * 
 */

#include "imu.h"
#include "boardsupport.h"
#include "gps_ublox.h"
#include "bmp085.h"
#include "estimator.h"
#include "qfilter.h"

#define INIT_X_P	10.
#define INIT_Y_P	10.
#define INIT_Z_P	10.

#define Q_X0	0.001
#define Q_X1	0.001
#define Q_X2	0.00001
#define Q_Y0	0.001
#define Q_Y1	0.001
#define Q_Y2	0.00001
#define Q_Z0	0.001
#define Q_Z1	0.001
#define Q_Z2	0.00001

#define R_X		2.
#define R_Y		2.
#define R_Z		2.

#define COS_PI_4 0.7071 // cos(pi/4), for taylor approx for latitude of around 45degre
#define EARTH_RADIUS 0.6731 //in E-7 meter
#define DEGREE_TO_RADIAN 0.0175 //pi/180
#define PI_4 0.7854  //pi/4

//Cross product
#define CP(u,v,out)\ 
	out[0]=u[1]*v[2]-u[2]*v[1];\
	out[1]=u[2]*v[0]-u[0]*v[2];\
	out[2]=u[0]*v[1]-u[1]*v[0];
	
#define MUL_V_SCA(u,a)\
	u[0]=u[0]*a;\
	u[1]=u[1]*a;\
	u[2]=u[2]*a;
	

pressure_data *baro;
board_hardware_t *board;
float P[3][3][3]; // Covariance matrice for Z,X and Y
float Q[3][3];
float R[3];
long init_lat,init_long;
float init_alt;
float previous_P[3][3][3]; //Remember the state and covariance 200ms ago (so 10 predictions ago) to apply propagation
float previous_state[3][3];//for delayed GPS measure (so we can get a max 200ms delay)
float previous_acc[3][10];// We must stock the acc, dt and quaternion so that we can do the predictions until actual time
float previous_dt[3][10];



//----------------------------INITIALISATION------------------------
void e_init()
{
	board=get_board_hardware();
	e_kalman_init(X,INIT_X_P); //e stands for estimator not extended
	e_kalman_init(Y,INIT_Y_P);
	e_kalman_init(Z,INIT_Z_P);
	Q[0][0]=Q_X0;
	Q[0][1]=Q_X1;
	Q[0][2]=Q_X2;
	Q[1][0]=Q_Y0;
	Q[1][1]=Q_Y1;
	Q[1][2]=Q_Y2;
	Q[2][0]=Q_Z0;
	Q[2][1]=Q_Z1;
	Q[2][2]=Q_Z2;
	R[0]=R_X;
	R[1]=R_Y;
	R[2]=R_Z;
}

void e_kalman_init (int axis,float init_p) // axis = Z, X or Y
{
	int i, j;
	
	board->estimation.state[axis][POSITION] = 0; // Differential par rapport au point de depart
	board->estimation.state[axis][SPEED] = 0; // Differential par rapport au point de depart
	
	if (axis==X)
		board->estimation.state[axis][BIAIS] = board->imu1.raw_bias[3];
	else if (axis==Y)
		board->estimation.state[axis][BIAIS] = board->imu1.raw_bias[4];
	else if (axis==Z)
		board->estimation.state[axis][BIAIS] = board->imu1.raw_bias[5];
		
	for (i=0; i<3; i++) {
		for (j=0; j<3; j++)
		P[axis][i][j] = 0.;
		P[axis][i][i] = init_p;
	}
}

//------------------------------PREDICTION--------------------------
void e_predict (UQuat_t *qe, float *a, float dt)
{
	
	UQuat_t inv_qe,q_temp;
	float x[3],y[3],z[3];
	float temp1[3], temp2[3];
	// Calculation of acceleration on x,y,z in NED 
	x[0]=1;x[1]=0;x[2]=0; //definition of x,y,z in NED
	y[0]=0;y[1]=1;y[2]=0;
	z[0]=0;z[1]=0;z[2]=1;
	quat_rot(qe,x); // get the x vector of the quad in NED
	quat_rot(qe,y);
	quat_rot(qe,z);
	MUL_V_SCA(x,a[0]) // get the right norm
	MUL_V_SCA(y,a[1])
	MUL_V_SCA(z,a[2])

	e_kalman_predict(X,(x[0]*x[0]+y[0]*y[0]+z[0]*z[0]),dt);//final x (in NED) acc 
	e_kalman_predict(Y,(x[1]*x[1]+y[1]*y[1]+z[1]*z[1]),dt);
	e_kalman_predict(Z,(x[2]*x[2]+y[2]*y[2]+z[2]*z[2]),dt);
}

//Rotation of vector vect with the quaternion quat
void quat_rot(UQuat_t *quat,float *vect)
{
	float temp1[3],temp2[3];
	CP((*quat).v,vect,temp1);
	temp1[0]=temp1[0]+(*quat).s*vect[0];
	temp1[1]=temp1[1]+(*quat).s*vect[1];
	temp1[2]=temp1[2]+(*quat).s*vect[2];
	CP((*quat).v,temp1,temp2);
	vect[0]= vect[0]+temp2[0]+temp2[0];
	vect[1]= vect[1]+temp2[1]+temp2[1];
	vect[2]= vect[2]+temp2[2]+temp2[2];
}

/*

 state vector : X = [ x x_speed x_biais ]; 

 F = [ 1 dt -dt^2/2
       0  1 -dt
       0  0   1     ];

 B = [ dt^2/2 dt 0]';

 Q = [ 0.01  0     0
       0     0.01  0
       0     0     0.001 ];

 Xk1 = F * Xk0 + B * accel;

 Pk1 = F * Pk0 * F' + Q;

*/
void e_kalman_predict (int axis,float accel_meas, float dt)
{
  float accel_corr;
  float FPF00,FPF01,FPF02,FPF10,FPF11,FPF12,FPF20,FPF21,FPF22;
  /* update state */
  
  if (axis == Z)
	accel_corr = accel_meas - 1; // 1 for gravity
  else
	accel_corr = accel_meas;
	
  board->estimation.state[axis][POSITION] = board->estimation.state[axis][POSITION] + dt * board->estimation.state[axis][SPEED]; // not exactly the function F defined above
  board->estimation.state[axis][SPEED] = board->estimation.state[axis][SPEED] + dt * ( accel_corr - board->estimation.state[axis][BIAIS]);
  /* update covariance */
  // F*P*F' calculation
  FPF00 = P[axis][0][0] + dt * ( P[axis][1][0] + P[axis][0][1] + dt * P[axis][1][1] );
  FPF01 = P[axis][0][1] + dt * ( P[axis][1][1] - P[axis][0][2] - dt * P[axis][1][2] );
  FPF02 = P[axis][0][2] + dt * ( P[axis][1][2] );
  FPF10 = P[axis][1][0] + dt * (-P[axis][2][0] + P[axis][1][1] - dt * P[axis][2][1] );
  FPF11 = P[axis][1][1] + dt * (-P[axis][2][1] - P[axis][1][2] + dt * P[axis][2][2] );
  FPF12 = P[axis][1][2] + dt * (-P[axis][2][2] );
  FPF20 = P[axis][2][0] + dt * ( P[axis][2][1] );
  FPF21 = P[axis][2][1] + dt * (-P[axis][2][2] );
  FPF22 = P[axis][2][2];
  // P = F*P*F' + Q
  P[axis][0][0] = FPF00 + Q[axis][POSITION];
  P[axis][0][1] = FPF01;
  P[axis][0][2] = FPF02;
  P[axis][1][0] = FPF10;
  P[axis][1][1] = FPF11 + Q[axis][SPEED];
  P[axis][1][2] = FPF12;
  P[axis][2][0] = FPF20;
  P[axis][2][1] = FPF21;
  P[axis][2][2] = FPF22 + Q[axis][BIAIS];	
}


//--------------------------------UPDATE----------------------------

/*
  H = [1 0 0];
  R = 0.1;
  // state residual
  y = rangemeter - H * Xm;
  // covariance residual
  S = H*Pm*H' + R;
  // kalman gain
  K = Pm*H'*inv(S);
  // update state
  Xp = Xm + K*y;
  // update covariance
  Pp = Pm - K*H*Pm;
*/
void e_kalman_update_position (int axis,float position_meas, uint32_t dt) 
{
	  float y,S,K1,K2,K3; 
	  float P11,P12,P13,P21,P22,P23,P31,P32,P33;

	  y = position_meas - board->estimation.state[axis][POSITION];
	  S = P[axis][0][0] + R[axis];
	  K1 = P[axis][0][0] * 1/S;
	  K2 = P[axis][1][0] * 1/S;
	  K3 = P[axis][2][0] * 1/S;

	  board->estimation.state[axis][POSITION]    = board->estimation.state[axis][POSITION]    + K1 * y;
	  board->estimation.state[axis][SPEED] = board->estimation.state[axis][SPEED] + K2 * y;
	  board->estimation.state[axis][BIAIS] = board->estimation.state[axis][BIAIS] + K3 * y;

	  P11 = (1. - K1) * P[axis][0][0];
	  P12 = (1. - K1) * P[axis][0][1];
	  P13 = (1. - K1) * P[axis][0][2];
	  P21 = -K2 * P[axis][0][0] + P[axis][1][0];
	  P22 = -K2 * P[axis][0][1] + P[axis][1][1];
	  P23 = -K2 * P[axis][0][2] + P[axis][1][2];
	  P31 = -K3 * P[axis][0][0] + P[axis][2][0];
	  P32 = -K3 * P[axis][0][1] + P[axis][2][1];
	  P33 = -K3 * P[axis][0][2] + P[axis][2][2];

	  P[axis][0][0] = P11;
	  P[axis][0][1] = P12;
	  P[axis][0][2] = P13;
	  P[axis][1][0] = P21;
	  P[axis][1][1] = P22;
	  P[axis][1][2] = P23;
	  P[axis][2][0] = P31;
	  P[axis][2][1] = P32;
	  P[axis][2][2] = P33;
}


/*
  H = [0 1 0];
  R = 0.1;
  // state residual
  yd = vz - H * Xm;
  // covariance residual
  S = H*Pm*H' + R;
  // kalman gain
  K = Pm*H'*inv(S);
  // update state
  Xp = Xm + K*yd;
  // update covariance
  Pp = Pm - K*H*Pm;
*/
void e_kalman_update_speed(int axis,float speed_meas, uint32_t dt)
{
  
	float yd,S,K1,K2,K3;
  	float P11,P12,P13,P21,P22,P23,P31,P32,P33;
  
	yd = speed_meas - board->estimation.state[axis][SPEED];
	S = P[axis][1][1] + R[axis];
	K1 = P[axis][0][1] * 1/S;
	K2 = P[axis][1][1] * 1/S;
	K3 = P[axis][2][1] * 1/S;

	board->estimation.state[axis][POSITION] = board->estimation.state[axis][POSITION] + K1 * yd;
	board->estimation.state[axis][SPEED] = board->estimation.state[axis][SPEED] + K2 * yd;
	board->estimation.state[axis][BIAIS] = board->estimation.state[axis][BIAIS] + K3 * yd;

	P11 = -K1 * P[axis][1][0] + P[axis][0][0];
	P12 = -K1 * P[axis][1][1] + P[axis][0][1];
	P13 = -K1 * P[axis][1][2] + P[axis][0][2];
	P21 = (1. - K2) * P[axis][1][0];
	P22 = (1. - K2) * P[axis][1][1];
	P23 = (1. - K2) * P[axis][1][2];
	P31 = -K3 * P[axis][1][0] + P[axis][2][0];
	P32 = -K3 * P[axis][1][1] + P[axis][2][1];
	P33 = -K3 * P[axis][1][2] + P[axis][2][2];

  P[axis][0][0] = P11;
  P[axis][0][1] = P12;
  P[axis][0][2] = P13;
  P[axis][1][0] = P21;
  P[axis][1][1] = P22;
  P[axis][1][2] = P23;
  P[axis][2][0] = P31;
  P[axis][2][1] = P32;
  P[axis][2][2] = P33;

}



//--------------------------------GLOBAL--------------------------
void estimator_loop()
{
	float pos_x,pos_y,pos_z;
	double	latitude_rad;
	float time_before_baro;
	static bool init_gps_position = 0;
	
	//static uint32_t dt_baro,time_before_baro;

	e_predict(&(board->imu1.attitude.qe),board->imu1.attitude.a,board->imu1.dt);
	
	//Check new values from GPS/Baro, if yes, update
	if (newValidGpsMsg())
	{
		if (!init_gps_position)
		{
			init_long=board->GPS_data.longitude;
			init_lat=board->GPS_data.latitude;
			init_alt=board->GPS_data.altitude;
		}
		init_gps_position = 1;
		
		//longitude latitude to x,y position
		latitude_rad= ((double) (board->GPS_data.latitude-init_lat))*DEGREE_TO_RADIAN; //in rad E+7
		pos_x= (float) (((double) (board->GPS_data.longitude-init_long)*EARTH_RADIUS)*DEGREE_TO_RADIAN*(COS_PI_4-COS_PI_4*(latitude_rad*0.0000001-PI_4)-COS_PI_4*0.5*(latitude_rad*0.0000001-PI_4)*(latitude_rad*0.0000001-PI_4)));//Taylor 2nd order cos() approx
		pos_y= (float) (latitude_rad*EARTH_RADIUS); 
		pos_z= -board->GPS_data.altitude+init_alt;
		//get delay of GPS measure
		//do prediction up to the corresponding delay
		e_kalman_update_position(X,pos_x,(uint32_t) board->GPS_data.timeLastMsg);
		e_kalman_update_position(Y,pos_y,(uint32_t) board->GPS_data.timeLastMsg);
		e_kalman_update_position(Z,pos_z,(uint32_t) board->GPS_data.timeLastMsg);
		e_kalman_update_speed(X,board->GPS_data.northSpeed,(uint32_t) board->GPS_data.timeLastMsg); 
		e_kalman_update_speed(Y,board->GPS_data.eastSpeed,(uint32_t) board->GPS_data.timeLastMsg);
		e_kalman_update_speed(Z,board->GPS_data.verticalSpeed,(uint32_t) board->GPS_data.timeLastMsg); 
		//Continue the prediction until actual time
	}	
/*	if (newBaroValue())
	{
		dt_baro=get_millis()-time_before_baro;
		baro=get_pressure_data_slow();
		e_kalman_update_position(Z,baro->altitude,dt_baro);
		time_before_baro=get_millis();
	}	*/
}