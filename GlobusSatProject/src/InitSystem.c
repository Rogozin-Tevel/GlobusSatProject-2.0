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

Boolean isFirstActivation()
{
	Boolean* flag = 0;
	int err = FRAM_read(flag, FIRST_ACTIVATION_FLAG_ADDR, FIRST_ACTIVATION_FLAG_SIZE);
	if (err != 0)
	{
		return FALSE;
	}
	return *flag;
}

void firstActivationProcedure()
{
	time_unix secondsSinceDeploy = 0;

	int err = FRAM_read((unsigned char*)secondsSinceDeploy, SECONDS_SINCE_DEPLOY_ADDR, SECONDS_SINCE_DEPLOY_SIZE);  // Check time since deploy
	if(err != 0)
	{
		secondsSinceDeploy = MINUTES_TO_SECONDS(30);
	}

	while (secondsSinceDeploy < MINUTES_TO_SECONDS(30))
	{
		vTaskDelay(SECONDS_TO_TICKS(10));  // Wait until time is up

		err = FRAM_write((unsigned char*)&secondsSinceDeploy, SECONDS_SINCE_DEPLOY_ADDR, SECONDS_SINCE_DEPLOY_SIZE);  // Write new time to memory
		if (err != 0)
		{
			break;
		}
	}

	TelemetryCollectorLogic();

	sat_packet_t cmd = {0};

	GetOnlineCommand(&cmd);
	ActUponCommand(&cmd);

	isis_eps__watchdog__from_t wdResponse;
	isis_eps__watchdog__tm(EPS_I2C_BUS_INDEX,&wdResponse);  // Reset EPS watchdog
}

#ifndef TESTING
	//IsisAntS_autoDeployment(0, isisants_sideA, 10);
	//IsisAntS_autoDeployment(0, isisants_sideB, 10);
#endif
	//TODO: log
}

void WriteDefaultValuesToFRAM()
{
	//TODO: write to FRAM all default values (like threshold voltages...)


}

int StartFRAM()
{
	int error = FRAM_start();
	return error;
}

int StartI2C()
{
	int error = I2C_start(I2c_SPEED_Hz,I2c_Timeout);
	return error;
}

int StartSPI()
{
	int error= SPI_start(bus1_spi,slave1_spi);
	return error;
}

int StartTIME()
{

}

int DeploySystem()
{

}

#define PRINT_IF_ERR(method) if(0 != err)printf("error in '" #method  "' err = %d\n",err);
int InitSubsystems()
{
	//TODO: check for return value errors

}

