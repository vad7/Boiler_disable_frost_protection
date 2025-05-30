/*
 * DisableAntifreezing.c
 * ver. 1.1
 * Board: Relay1 Heater 220V
 *
 * Created: 30.10.2014
 * Modified: 27.05.2025
 * Author: Vadim Kulakov, vad7@yahoo.com
 *
 * ATTINY13A
 */
#define F_CPU 1200000UL
// Fuses: BODLEVEL = 4V3
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/atomic.h>

#define OneWirePort	PORTB
#define OneWireDDR	DDRB
#define OneWireIN	PINB
#define OneWire		(1<<PORTB0)
#include "onewire.h"

#define RELAYPORT			PORTB
#define RELAY				(1<<PORTB5)

#define SWPIN				PINB
#define SW_SHOW_TEMP		(1<<PORTB2)

#define LED1PORT			PORTB	// LED port
#define LED1				(1<<PORTB1)	// LED pin
#define LED1_ON				LED1PORT |= LED1
#define LED1_OFF			LED1PORT &= ~LED1
#define UNUSED_PINS			((1<<PORTB3) | (1<<PORTB4))

//#define EPROM_OSCCAL		0x00
// Temp = t[-128..127]
#define EEPROM_TempOn		0x00		// 0x08 (8C)
#define EEPROM_TempOff		0x01		// 0x09 (9Ñ)
#define EEPROM_TempFreeze	0x02		// 0xF6 (-10Ñ)
#define EEPROM_ReadPeriod	0x03		// 0x0A (10sec)
#define EEPROM_ReadFailedPeriodMax 0x04 // 0x3C (600sec) *EEPROM_ReadPeriod

register uint8_t TimerSecCnt asm("15");
register uint8_t TimerSec asm("16");
register uint8_t ErrorCnt asm("17");

#if(1)
void Delay100ms(unsigned int ms)
{
	while(ms-- > 0) {	
		_delay_ms(100);
		wdt_reset();
	}
}

void FlashLED(uint8_t num, uint8_t toff, uint8_t ton)
{
	while (num-- > 0) {
		LED1_OFF;
		Delay100ms(toff);
		LED1_ON;
		Delay100ms(ton);
	}
	LED1_OFF;
}

// Flash number: -99..99. Minus - very long pulse, MSD - long pulses, LSD - short pulses
void FlashNumberOnLED(int16_t Number)
{
	if(Number < 0) {
		FlashLED(1, 10, 30); // minus
		Number = -Number;
		Delay100ms(7);
	}
	FlashLED(Number / 10, 7, 11);
	Delay100ms(10);
	FlashLED(Number % 10, 7, 5);
}

uint8_t EEPROM_read(uint8_t ucAddress)
{
	while(EECR & (1<<EEWE)) ;
	EEARL = ucAddress;
	EECR |= (1<<EERE);
	return EEDR;
}

void EEPROM_write(uint8_t ucAddress, uint8_t ucData)
{
	while(EECR & (1<<EEWE)) ;
	cli();
	EECR = (0<<EEPM1)|(0<<EEPM0);
	EEARL = ucAddress;
	EEDR = ucData;
	EECR |= (1<<EEMWE);
	EECR |= (1<<EEWE);
	sei();
}
#endif

ISR(TIM0_OVF_vect) // Timer overflow
{
	if(++TimerSecCnt == 18) // 0.983 sec
	{
		TimerSecCnt = 0;
		if(TimerSec) TimerSec--;
	}
}

int main(void)
{
	CLKPR = (1<<CLKPCE); CLKPR = (0<<CLKPS3) | (0<<CLKPS2) | (1<<CLKPS1) | (1<<CLKPS0); // Clock prescaler division factor: 8
	PRR |= 1<<PRADC;	// ADC power off
	DDRB = RELAY | LED1 | UNUSED_PINS;	// Out
	PORTB = SW_SHOW_TEMP;		// Pull up
	WDTCR = (1<<WDCE) | (1<<WDE); WDTCR = (1<<WDE) | (0<<WDP3) | (1<<WDP2) | (1<<WDP1) | (1<<WDP0);	//  Watchdog Reset 2s
	MCUCR = (1<<SE) | (0<<ISC01) | (0<<ISC00); // Sleep enable
	//TCCR0A = 0;	// Normal mode
	TCCR0B = (1 << CS02) | (0 << CS01) | (0 << CS00); // /256
	TIMSK0 = (1<<TOIE0); // Timer overflow int enable
	//OSCCAL = EEPROM_read(EPROM_OSCCAL);
	sei();
	if(EEPROM_read(EEPROM_ReadPeriod) == 0xFF) {		// Set default EEPROM values
		EEPROM_write(EEPROM_TempOn, 8);
		EEPROM_write(EEPROM_TempOff, 9);
		EEPROM_write(EEPROM_TempFreeze, -10);
		EEPROM_write(EEPROM_ReadPeriod, 10);
		EEPROM_write(EEPROM_ReadFailedPeriodMax, 60);
	}
	while(1) {
		__asm__ volatile ("" ::: "memory"); // Need memory barrier
		wdt_reset();
		if(TimerSec == 0) {
			int16_t T = DS18X20_ReadTempSingle();
			if(T < -32513) { // error if < 0x80FF
				if(ErrorCnt > EEPROM_read(EEPROM_ReadFailedPeriodMax)) RELAYPORT &= ~RELAY; // emergency off
				else ErrorCnt++;
				TimerSec = EEPROM_read(EEPROM_ReadPeriod);
				while(TimerSec) {
					FlashLED(T & 0xFF, 5, 2);
					Delay100ms(15);
				}
			} else {
				T = T / 10 + (T % 10 >= 5 ? 1 : 0);
				ErrorCnt = 0;
				if((SWPIN & SW_SHOW_TEMP) == 0) { // Show temp
					FlashNumberOnLED(T);
					Delay100ms(15);
				}
				if(T <= (int8_t)EEPROM_read(EEPROM_TempFreeze)) {// freezing...
					RELAYPORT &= ~RELAY; // OFF
					FlashLED(10, 2, 2);
				} else if(T >= (int8_t)EEPROM_read(EEPROM_TempOff)) {
					RELAYPORT &= ~RELAY; // OFF
				} else if(T <= (int8_t)EEPROM_read(EEPROM_TempOn)) {
					RELAYPORT |= RELAY; // ON
				}
				if(RELAYPORT & RELAY) {
					LED1_OFF;
				} else {
					LED1_ON;
				}
				TimerSec = EEPROM_read(EEPROM_ReadPeriod);
			}
		}
		sleep_cpu();
	}
}
