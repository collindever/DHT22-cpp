/*------------------------------------------------------------------------------
	DHT11 CPP temperature & humidity sensor DHT11 driver for ESP32
	Mar 2019:	Collin Dever, new for DHT11

	This example code is in the Public Domain (or CC0 licensed, at your option.)
	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
	PLEASE KEEP THIS CODE IN LESS THAN 0XFF LINES. EACH LINE MAY CONTAIN ONE BUG !!!
---------------------------------------------------------------------------------*/


#include <driver/gpio.h>
#include <esp_err.h>
#include <stdint.h>
#include <sys/types.h>
#include <esp_log.h>

#include "DHT.h"

static char TAG[] = "DHT";

/*-----------------------------------------------------------------------
;
;	Constructor & default settings
;
;------------------------------------------------------------------------*/

DHT::DHT()
{

	DHTgpio = (gpio_num_t) 4;
	humidity = 0.;
	temperature = 0.;

}

/*
DHT::~DHT( void )
{
}
*/

// ----------------------------------------------------------------------

void DHT::setDHTgpio( gpio_num_t gpio )
{
	DHTgpio = gpio;
}


// == get temp & hum =============================================

float DHT::getHumidity() { return humidity; }
float DHT::getTemperature() { return temperature; }

// == error handler ===============================================

void DHT::errorHandler(int response)
{
	switch(response) {

		case DHT_TIMEOUT_ERROR :
			//ESP_LOGI( TAG, "Sensor Timeout\n" );
			printf( "Sensor Timeout\n" );
			break;

		case DHT_CHECKSUM_ERROR:
			//ESP_LOGI( TAG, "CheckSum error\n" );
			printf( "CheckSum error\n" );
			break;

		case DHT_OK:
			break;

		default :
			//ESP_LOGI( TAG, "Unknown error\n" );
			printf( "Unknown error\n" );
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

int DHT::getSignalLevel( int usTimeOut, bool state )
{

	int uSec = 0;
	while( gpio_get_level(DHTgpio)==state ) {

		if( uSec > usTimeOut )
			return -1;

		++uSec;
		ets_delay_us(1);		// uSec delay
	}

	return uSec;
}

/*----------------------------------------------------------------------------
;
;	read DHT11 sensor

	copy/paste from DHT11 Datasheet:
	Data  consists  of decimal  and  integral  parts.  
	A  complete  data  transmission  is 40bit,  and  the sensor sends higher data bitfirst. 
    Data format:8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T data + 8bit check sum
	If the data transmission is right,the check-sum should be the last 8bit of 
	"8bit integral RH data+8bit decimal RHdata+8bit integral T data+8bit decimal T data".

	Signal & Timings:
	The interval of whole process must be beyond 2 seconds.

	To request data from DHT:
	1) Sent low pulse for > 18 ms (MILI SEC)
	2) Sent high pulse for > 20~40 us (Micros).
	3) When DHT detects the start signal, it will pull low the bus 80us as response signal,
	   then the DHT pulls up 80us for preparation to send data.
	4) When DHT is sending data to MCU, every bit's transmission begin with low-voltage-level that last 50us,
	   the following high-voltage-level signal's length decide the bit is "1" or "0".
		0: 26~28 us
		1: 70 us
;----------------------------------------------------------------------------*/

#define MAXdhtData 5	// to complete 40 = 5*8 Bits

int DHT::readDHT()
{
int uSec = 0;

uint8_t dhtData[MAXdhtData];
uint8_t byteInx = 0;
uint8_t bitInx = 7;

	for (int k = 0; k<MAXdhtData; k++)
		dhtData[k] = 0;

	// == Send start signal to DHT sensor ===========

	gpio_set_direction( DHTgpio, GPIO_MODE_OUTPUT );

	// pull down for 3 ms for a smooth and nice wake up
	gpio_set_level( DHTgpio, 0 );
	ets_delay_us( 18000 );

	// pull up for 25 us for a gentile asking for data
	gpio_set_level( DHTgpio, 1 );
	ets_delay_us( 25 );

	gpio_set_direction( DHTgpio, GPIO_MODE_INPUT );		// change to input mode

	// == DHT will keep the line low for 80 us and then high for 80us ====

	uSec = getSignalLevel( 85, 0 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 ) return DHT_TIMEOUT_ERROR;

	// -- 80us up ------------------------

	uSec = getSignalLevel( 85, 1 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 ) return DHT_TIMEOUT_ERROR;

	// == No errors, read the 40 data bits ================

	for( int k = 0; k < 40; k++ ) {

		// -- starts new data transmission with >50us low signal

		uSec = getSignalLevel( 56, 0 );
		if( uSec<0 ) return DHT_TIMEOUT_ERROR;

		// -- check to see if after >70us rx data is a 0 or a 1

		uSec = getSignalLevel( 75, 1 );
		if( uSec<0 ) return DHT_TIMEOUT_ERROR;

		// add the current read to the output data
		// since all dhtData array where set to 0 at the start,
		// only look for "1" (>28us us)

		if (uSec > 40) {
			dhtData[ byteInx ] |= (1 << bitInx);
			}

		// index to next byte

		if (bitInx == 0) { bitInx = 7; ++byteInx; }
		else bitInx--;
	}

	// == get humidity from Data[0] and Data[1] ==========================

	humidity = dhtData[0];
	humidity += (dhtData[1] & 0x0f) * 0.1;

	// == get temp from Data[2] and Data[3]

	temperature = dhtData[2];
	temperature += (dhtData[3] & 0x0f) * 0.1;

	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
		temperature *= -1;

	temperature = this->convertCtoF(temperature);
	// == verify if checksum is ok ===========================================
	// Checksum is the sum of Data 8 bits masked out 0xFF

	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF))
		return DHT_OK;

	else
		return DHT_CHECKSUM_ERROR;
}

float DHT::convertCtoF(float c) {
  return c * 1.8 + 32;
}