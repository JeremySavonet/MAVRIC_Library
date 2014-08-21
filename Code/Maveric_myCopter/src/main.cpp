/**
 * \file
 *
 * \brief Empty user application template
 *
 */

/*
 * Include header files for all drivers that have been imported from
 * AVR Software Framework (ASF).
 */

extern "C" {
	#include "led.h"
	#include "delay.h"
	#include "print_util.h"
	#include "central_data.h"
	#include "boardsupport.h"
	#include "tasks.h"
	#include "mavlink_telemetry.h"
	#include "piezo_speaker.h"
	
	#include "gpio.h"
	
	#include "FatFs/diskio.h"
}
 
central_data_t *central_data;

void initialisation() 
{	
	central_data = central_data_get_pointer_to_struct();
	boardsupport_init(central_data);
	central_data_init();

	mavlink_telemetry_init();
	
	//onboard_parameters_write_parameters_to_flashc(&central_data->mavlink_communication.onboard_parameters);
	onboard_parameters_read_parameters_from_flashc(&central_data->mavlink_communication.onboard_parameters);

	LED_On(LED1);

	//piezo_speaker_startup_melody();
	piezo_speaker_mario_melody();

	print_util_dbg_print("OK. Starting up.\r");

	//sd_spi_test();
	
	//diskio_test_low_layer();
	
	//diskio_test_fatfs();

	central_data->state.mav_state = MAV_STATE_STANDBY;

	central_data->imu.calibration_level = OFF;
}

int main (void)
{
	initialisation();
	tasks_create_tasks();
	
	while (1 == 1) 
	{
		scheduler_update(&central_data->scheduler);
	}

	return 0;
}