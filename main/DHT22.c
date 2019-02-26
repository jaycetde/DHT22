/*------------------------------------------------------------------------------

	DHT22 temperature & humidity sensor AM2302 (DHT22) driver for ESP32

	Jun 2017:	Ricardo Timmermann, new for DHT22  	

	Code Based on Adafruit Industries and Sam Johnston and Coffe & Beer. Please help
	to improve this code. 
	
	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.

	PLEASE KEEP THIS CODE IN LESS THAN 0XFF LINES. EACH LINE MAY CONTAIN ONE BUG !!!

---------------------------------------------------------------------------------*/

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "DHT22.h"

#define DHT_DATA_NUM_BYTES 5 // to complete 40 = 5*8 Bits

// == global defines =============================================

static const char *TAG = "DHT";

// == error handler ===============================================

void dhtErrorHandler(int response)
{
	switch (response)
	{
	case DHT_OK:
		break;

	case DHT_TIMEOUT_ERROR:
		ESP_LOGE(TAG, "Sensor Timeout\n");
		break;

	case DHT_CHECKSUM_ERROR:
		ESP_LOGE(TAG, "CheckSum error\n");
		break;

	default:
		ESP_LOGE(TAG, "Unknown error\n");
	}
}

/*-------------------------------------------------------------------------------
;
;	get next state 
;
;	I don't like this logic. It needs some interrupt blocking / priority
;	to ensure it runs in realtime.
;
;--------------------------------------------------------------------------------*/

int getSignalTiming(int gpioPin, bool state, int usTimeOut)
{

	int uSec = 0;
	while (gpio_get_level(gpioPin) == state)
	{

		if (uSec > usTimeOut)
			return -1;

		++uSec;
		ets_delay_us(1); // uSec delay
	}

	return uSec;
}

/*----------------------------------------------------------------------------
;
;	read DHT22 sensor

copy/paste from AM2302/DHT22 Docu:

DATA: Hum = 16 bits, Temp = 16 Bits, check-sum = 8 Bits

Example: MCU has received 40 bits data from AM2302 as
0000 0010 1000 1100 0000 0001 0101 1111 1110 1110
16 bits RH data + 16 bits T data + check sum

1) we convert 16 bits RH data from binary system to decimal system, 0000 0010 1000 1100 → 652
Binary system Decimal system: RH=652/10=65.2%RH

2) we convert 16 bits T data from binary system to decimal system, 0000 0001 0101 1111 → 351
Binary system Decimal system: T=351/10=35.1°C

When highest bit of temperature is 1, it means the temperature is below 0 degree Celsius. 
Example: 1000 0000 0110 0101, T= minus 10.1°C: 16 bits T data

3) Check Sum=0000 0010+1000 1100+0000 0001+0101 1111=1110 1110 Check-sum=the last 8 bits of Sum=11101110

Signal & Timings:

The interval of whole process must be beyond 2 seconds.

To request data from DHT:

1) Sent low pulse for > 1~10 ms (MILI SEC)
2) Sent high pulse for > 20~40 us (Micros).
3) When DHT detects the start signal, it will pull low the bus 80us as response signal, 
   then the DHT pulls up 80us for preparation to send data.
4) When DHT is sending data to MCU, every bit's transmission begin with low-voltage-level that last 50us, 
   the following high-voltage-level signal's length decide the bit is "1" or "0".
	0: 26~28 us
	1: 70 us

;----------------------------------------------------------------------------*/

void dhtInit(int gpioPin)
{
	gpio_set_direction(gpioPIn, GPIO_MODE_OUTPUT);
	gpio_set_level(gpioPin, 1);
}

int dhtRead(int gpioPin, float *temperature, float *humidity)
{
	int lowUSec = 0;
	int uSec = 0;

	uint8_t dhtData[DHT_DATA_NUM_BYTES];
	uint8_t byteInx = 0;
	uint8_t bitInx = 7;

	// Should already be initialized as 0
	// for (int k = 0; k < MAXdhtData; k++)
	// 	dhtData[k] = 0;

	// == Send start signal to DHT sensor ===========

	gpio_set_direction(gpioPin, GPIO_MODE_OUTPUT);

	// pull down for 5 ms for a smooth and nice wake up
	gpio_set_level(gpioPin, 0);
	ets_delay_us(5000);

	// pull up for 25 us for a gentile asking for data
	gpio_set_level(gpioPin, 1);
	// ets_delay_us(25); // No delay necessary, the data line will stay pulled up

	gpio_set_direction(gpioPin, GPIO_MODE_INPUT); // change to input mode

	// == Wait for DHT to pull the line down ====
	uSec = getSignalLevel(gpioPin, 1, 100);
	if (uSec < 0)
		return DHT_TIMEOUT_ERROR;

	// == DHT will keep the line low for 80 us and then high for 80us ====
	uSec = getSignalLevel(gpioPin, 0, 100);
	if (uSec < 0)
		return DHT_TIMEOUT_ERROR;

	// -- 80us up ------------------------

	uSec = getSignalLevel(gpioPin, 0, 100);
	if (uSec < 0)
		return DHT_TIMEOUT_ERROR;

	// == No errors, read the 40 data bits ================

	for (int k = 0; k < 40; k++)
	{
		// -- starts new data transmission with >50us low signal

		lowUSec = getSignalLevel(gpioPin, 0, 70);
		if (lowUSec < 0)
			return DHT_TIMEOUT_ERROR;

		// -- check to see if after >70us rx data is a 0 or a 1

		uSec = getSignalLevel(gpioPin, 1, 100);
		if (uSec < 0)
			return DHT_TIMEOUT_ERROR;

		// add the current read to the output data
		// since all dhtData array where set to 0 at the start,
		// only look for "1" (>28us us)

		if (uSec > lowUSec)
		{
			dhtData[byteInx] |= (1 << bitInx);
		}

		// index to next byte

		if (bitInx == 0)
		{
			bitInx = 7;
			++byteInx;
		}
		else
			bitInx--;
	}

	// == verify if checksum is ok ===========================================
	// Checksum is the sum of Data 8 bits masked out 0xFF
	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF))
	{
		*humidity = (dhtData[0] * 256 + dhtData[1]) / 10.0f;
		*temperature = ((dhtData[2] & 0x7F) * 256 + dhtData[3]) / 10.0f;

		if (dhtData[2] & 0x80) // negative temp, brrr it's freezing
			*temperature *= -1.0f;

		return DHT_OK;
	}

	return DHT_CHECKSUM_ERROR;
}
