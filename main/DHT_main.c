/*

	DHT22 sensor reading test

	Jun 2007: Ricardo Timmermann, implemetation


*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "DHT22.h"

#define DHT_PIN 4

void DHT_task(void *pvParameter)
{
	dhtInit(DHT_PIN);
	printf( "Starting DHT Task\n\n");

	while(1) {
		printf("=== Reading DHT ===\n" );
		float temperature;
		float humidity;
		int ret = dhtRead(DHT_PIN, &temperature, &humidity);

		errorHandler(ret);

		printf( "Hum %.1f\n", humidity );
		printf( "Tmp %.1f\n", temperature );

		// -- wait at least 2 sec before reading again ------------
		// The interval of whole process must be beyond 2 seconds !! 
		vTaskDelay( 3000 / portTICK_RATE_MS );
	}
}

void app_main()
{
	nvs_flash_init();
	vTaskDelay( 1000 / portTICK_RATE_MS );
	xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );
}
