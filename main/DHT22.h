/* 
	DHT22 temperature sensor driver
*/

#ifndef DHT22_H_

#define DHT22_H_

#define DHT_OK 0
#define DHT_CHECKSUM_ERROR -1
#define DHT_TIMEOUT_ERROR -2

// == function prototypes =======================================

void dhtInit(int gpioPin);
int dhtRead(int gpioPin, float *temperature, float *humidity);
void dhtErrorHandler(int response);
int getSignalTiming(int gpioPin, bool state, int usTimeOut);

#endif
