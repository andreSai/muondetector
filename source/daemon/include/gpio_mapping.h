#ifndef GPIO_MAPPING_H
#define GPIO_MAPPING_H
#include <map>
#include "gpio_pin_definitions.h"


#define MAX_HW_VER 3


// define pins on the raspberry pi, UBIAS_EN is the power on/off pin for bias voltage
// PREAMP_1/2 enables the DC voltage to power the preamp through the signal cable
// Attention: the GPIO pin definitions are referring to wiringPi standard except the EVT input pins
// which are in pigpio standard

// HW Ver. 1
/*
//#define UBIAS_EN 4  // pigpio
#define UBIAS_EN 7  // wiringpi
#define PREAMP_1 28  // pigpio 20
#define PREAMP_2 29  // pigpio 21
#define EVT_AND 5
#define EVT_XOR 6
#define GAIN_HL 0	// pigpio 17
#define ADC_READY 23
#define TIMEPULSE 18
#define STATUS1 13 // (23)
#define STATUS2 19 // (24)
#define STATUS3 26 // (25)
*/
// HW Ver 2: pigpio pin numbers (wiring pi numbers)
//#define UBIAS_EN 26 // (25)
//#define PREAMP_1 4  // (7)
//#define PREAMP_2 17 // (0)
//#define EVT_AND 22 // (3)
//#define EVT_XOR 27 // (2)
//#define GAIN_HL 6	 // (22)
//#define ADC_READY 12 // (26)
//#define TIMEPULSE 18 // (1)
//#define TIME_MEAS_OUT 5 // (21)
//#define STATUS1 13 // (23)
//#define STATUS2 19 // (24)
//#define PREAMP_FAULT 23 // (4)

// ATTENTION:
// TO MAKE IT MORE SIMPLE THERE WILL BE ONLY PIGPIO,
// NO WIRING PI FROM NOW

static const std::map<GPIO_PIN, unsigned int> GPIO_PINMAP_VERSIONS[MAX_HW_VER+1] = {
		{
			/* Pin mapping, HW Version 0, proxy to be never used nor initializing something */
		} ,
		{
			/* Pin mapping, HW Version 1 */
            { UBIAS_EN , 4 },
            { PREAMP_1 , 20 },
            { PREAMP_2 , 21 },
			{ EVT_AND , 5 },
			{ EVT_XOR , 6 },
            { GAIN_HL , 17 },
			{ ADC_READY , 23 },
			{ TIMEPULSE , 18 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { STATUS3 , 26 },
            { TDC_INTB, 20 },
            { TDC_STATUS, 21 },
			{ EXT_TRIGGER, 21 }
		} ,
		{
			/* Pin mapping, HW Version 2 */
            { UBIAS_EN , 26 },
            { PREAMP_1 , 4 },
            { PREAMP_2 , 17 },
            { EVT_AND , 22 },
            { EVT_XOR , 27 },
            { GAIN_HL , 6 },
            { ADC_READY , 12 },
			{ TIMEPULSE , 18 },
			{ TIME_MEAS_OUT , 5 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { PREAMP_FAULT , 23 },
            { TDC_INTB, 20 },
            { TDC_STATUS, 21 },
			{ EXT_TRIGGER, 16 }
		} ,
		{
			/* Pin mapping, HW Version 3 */
            { UBIAS_EN , 26 },
            { PREAMP_1 , 4 },
            { PREAMP_2 , 17 },
            { EVT_AND , 22 },
            { EVT_XOR , 27 },
            { GAIN_HL , 6 },
            { ADC_READY , 12 },
			{ TIMEPULSE , 18 },
			{ TIME_MEAS_OUT , 5 },
            { STATUS1 , 13 },
            { STATUS2 , 19 },
            { PREAMP_FAULT , 23 },
            { TDC_INTB, 20 },
            { TDC_STATUS, 21 },
			{ EXT_TRIGGER, 16 },
			{ IN_POL1, 24 },
			{ IN_POL2, 25 }
		}
	};


extern std::map<GPIO_PIN, unsigned int> GPIO_PINMAP;

inline GPIO_PIN bcmToGpioSignal(unsigned int bcmGpioNumber) {
	std::map<GPIO_PIN, unsigned int>::const_iterator i = GPIO_PINMAP.cbegin();
	while (i != GPIO_PINMAP.cend() && i->second!=bcmGpioNumber)	++i;
	if (i==GPIO_PINMAP.cend()) return UNDEFINED_PIN;
	return i->first;
}

#endif //GPIO_MAPPING_H
