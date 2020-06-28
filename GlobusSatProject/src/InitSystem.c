#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <hal/Drivers/I2C.h>
#include <hal/Drivers/SPI.h>
#include <hal/Timing/Time.h>
#include <at91/utility/exithandler.h>
#include <string.h>
#include "GlobalStandards.h"
#include "SubSystemModules/PowerManagment/EPS.h"
#include "SubSystemModules/Communication/TRXVU.h"
#include "SubSystemModules/Communication/ActUponCommand.h"
#include "SubSystemModules/Maintenance/Maintenance.h"
#include "SubSystemModules/Housekepping/TelemetryCollector.h"
#include "InitSystem.h"
#include "TLM_management.h"

#include <satellite-subsystems/isis_eps_driver.h>

#define I2c_SPEED_Hz 100000
#define I2c_Timeout 10

// Done By Ido Golan
Boolean isFirstActivation()
{
	Boolean flag = FALSE;
	int err = FRAM_read((unsigned char*)&flag, FIRST_ACTIVATION_FLAG_ADDR, FIRST_ACTIVATION_FLAG_SIZE);
	if (err != 0)
	{
		return FALSE;
	}
	return flag;
}

// Done By Ido Golan
void firstActivationProcedure()
{
	time_unix secondsSinceDeploy = 0;
	sat_packet_t cmd = {0};

	int err = FRAM_read((unsigned char*)secondsSinceDeploy, SECONDS_SINCE_DEPLOY_ADDR, SECONDS_SINCE_DEPLOY_SIZE);  // Check time since deploy
	if(err != 0)
	{
		secondsSinceDeploy = MINUTES_TO_SECONDS(1);
	}

	muteTRXVU(MINUTES_TO_SECONDS(1) - secondsSinceDeploy);
	printf("~~~~~~~~~~~~~~~~~~~~~~~\nMuted\n~~~~~~~~~~~~~~~~~~~~~~~\n");
	while (secondsSinceDeploy < MINUTES_TO_SECONDS(1))
	{
		vTaskDelay(SECONDS_TO_TICKS(10));  // Wait until time is up
		secondsSinceDeploy -= SECONDS_TO_TICKS(10);
		err = FRAM_write((unsigned char*)&secondsSinceDeploy, SECONDS_SINCE_DEPLOY_ADDR, SECONDS_SINCE_DEPLOY_SIZE);  // Write new time to memory
		if (err != 0)
		{
			break;
		}

		TelemetryCollectorLogic();
		GetOnlineCommand(&cmd);
		ActUponCommand(&cmd);

		isis_eps__watchdog__from_t wdResponse;
		isis_eps__watchdog__tm(EPS_I2C_BUS_INDEX,&wdResponse);  // Reset EPS watchdog
	}
	printf("~~~~~~~~~~~~~~~~~~~~~~~\nUnmuted\n~~~~~~~~~~~~~~~~~~~~~~~\n");
	UnMuteTRXVU();

	Boolean flag = FALSE;
	FRAM_write((unsigned char*)&flag, FIRST_ACTIVATION_FLAG_ADDR, FIRST_ACTIVATION_FLAG_SIZE);

#ifndef TESTING
	//IsisAntS_autoDeployment(0, isisants_sideA, 10);
	//IsisAntS_autoDeployment(0, isisants_sideB, 10);
#endif
	printf("Deployed Ants\n");
}

// Done By Shachar Sanger
void WriteDefaultValuesToFRAM()
{
	time_unix default_no_comm_thresh = DEFAULT_NO_COMM_WDT_KICK_TIME;
	FRAM_write((unsigned char*)&default_no_comm_thresh,//value
		NO_COMM_WDT_KICK_TIME_ADDR,//address
		NO_COMM_WDT_KICK_TIME_SIZE);//size

	EpsThreshVolt_t def_thresh_volt = { .raw = DEFAULT_EPS_THRESHOLD_VOLTAGES };
	FRAM_write((unsigned char*)def_thresh_volt.raw,//value
		EPS_THRESH_VOLTAGES_ADDR,//address
		EPS_THRESH_VOLTAGES_SIZE);//size


	float def_alpha = DEFAULT_ALPHA_VALUE;
	FRAM_write((unsigned char*)&def_alpha, EPS_ALPHA_FILTER_VALUE_ADDR,
		EPS_ALPHA_FILTER_VALUE_SIZE);


	time_unix tlm_save_period = DEFAULT_EPS_SAVE_TLM_TIME;
	FRAM_write((unsigned char*)&tlm_save_period, EPS_SAVE_TLM_PERIOD_ADDR,
		sizeof(tlm_save_period));

	tlm_save_period = DEFAULT_SOLAR_SAVE_TLM_TIME;
	FRAM_write((unsigned char*)&tlm_save_period, SOLAR_SAVE_TLM_PERIOD_ADDR,
		sizeof(tlm_save_period));
	//

	//
	tlm_save_period = DEFAULT_WOD_SAVE_TLM_TIME;
	FRAM_write((unsigned char*)&tlm_save_period, WOD_SAVE_TLM_PERIOD_ADDR,
		sizeof(tlm_save_period));
	//

	//
	tlm_save_period = DEFAULT_TRXVU_SAVE_TLM_TIME;
	FRAM_write((unsigned char*)&tlm_save_period, TRXVU_SAVE_TLM_PERIOD_ADDR,
		sizeof(tlm_save_period));
	//

	//
	tlm_save_period = DEFAULT_ANT_SAVE_TLM_TIME;
	FRAM_write((unsigned char*)&tlm_save_period, ANT_SAVE_TLM_PERIOD_ADDR,
		sizeof(tlm_save_period));
	//

	time_unix beacon_interval = DEFAULT_BEACON_INTERVAL_TIME;
	FRAM_write((unsigned char*)&beacon_interval, BEACON_INTERVAL_TIME_ADDR,
		BEACON_INTERVAL_TIME_SIZE);
}

// Done By Omri Halifa
int StartFRAM()
{
	int error = FRAM_start();
	return error;
}
// Done By Omri Halifa
int StartI2C()
{
	int error = I2C_start(I2c_SPEED_Hz,I2c_Timeout);
	return error;
}
// Done By Omri Halifa
int StartSPI()
{
	int error= SPI_start(bus1_spi,slave1_spi);
	return error;
}
// Done By Shachar Tzarfati & Ido Golan
int StartTIME()
{
	Time deployTime = UNIX_DEPLOY_DATE_JAN_D1_Y2020;
	int err = Time_start(&deployTime, 0);
	if(err != 0)
	{
		return err;
	}
	time_unix timeUni = 0;
	if(!isFirstActivation())
	{
		err = FRAM_read((unsigned char *)&timeUni, MOST_UPDATED_SAT_TIME_ADDR,
							MOST_UPDATED_SAT_TIME_SIZE);
		if(err != 0)
		{
			return err;
		}
		err = Time_setUnixEpoch((unsigned int)timeUni);
		if(err != 0)
		{
			return err;
		}
	}
}
// Done By Ido Golan
int DeploySystem()
{
	Boolean8bit tlms_created[NUMBER_OF_TELEMETRIES];
	if(isFirstActivation())
	{
		TelemetryCreateFiles(tlms_created);
		WriteDefaultValuesToFRAM();
		firstActivationProcedure();
	}
}

#define PRINT_IF_ERR(method) if(0 != err)printf("error in '" #method  "' err = %d\n",err);
// Done By Ido Golan
int InitSubsystems()
{
	if(StartI2C() != 0)
	{
		printf("Error in the initialization of I2C\n");
	}
	else
	{
		printf("I2C Init - [OK]\n");
	}

	if(StartSPI() != 0)
	{
		printf("Error in the initialization of SPI\n");
	}
	else
	{
		printf("SPI Init - [OK]\n");
	}

	if(StartFRAM() != 0)
	{
		printf("Error in the initialization of FRAM\n");
	}
	else
	{
		printf("FRAM Init - [OK]\n");
	}

	if(StartTIME() != 0)
	{
		printf("Error in the initialization of TIME\n");
	}
	else
	{
		printf("TIME Init - [OK]\n");
	}

	if(InitTrxvu() != 0)
	{
		printf("Error in the initialization of Trxvu\n");
	}
	else
	{
		printf("TRXVU Init - [OK]\n");
	}

	if(EPS_Init() != 0)
	{
		printf("Error in the initialization of EPS\n");
	}
	else
	{
		printf("EPS Init - [OK]\n");
	}

	if(InitializeFS(isFirstActivation()) != 0)
	{
		printf("Error in the initialization of FS\n");
	}
	else
	{
		printf("FS Init - [OK]\n");
	}

	if(InitTelemetryCollrctor() != 0)
	{
		printf("Error in the initialization of TelemetryCollector\n");
	}
	else
	{
		printf("Telemetry Collector Init - [OK]\n");
	}

	if(DeploySystem() != 0)
	{
		printf("Error in DeploySystem\n");
	}
	else
	{
		printf("Deploy Systems - [OK]\n");
	}

	return 0;
}
