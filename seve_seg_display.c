/*
 * seve_seg_display.c
 *
 * Created: 2019-10-09 오전 9:50:28
 *  Author: user
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "timer0.h"
#include "game.h"
#include "terminalio.h"

volatile uint32_t current_time;
uint8_t seven_seg[10] = {63,6,91,79,102,109,125,7,127,111};
uint32_t power_pellet_eaten_time = 0;
uint8_t ssg_cc = 0;
uint8_t next_phase = 0;
uint32_t ssg_count = 0;
uint8_t game_pause = 0;

void init_ssg(void) {
	DDRC = 0xFF;
	DDRD = (1 << DDRD2);

	OCR2A =  78;
	TCCR2A = (1 << COM2A1) | (1 << WGM21);
	TCCR2B = (1 << CS21) | (1 << CS22) | (1 << CS20);
	/* Enable interrupt on timer on output compare match 
	*/
	TIMSK2 = (1 << OCIE2A);

	/* Ensure interrupt flag is cleared */
	TIFR2 = (1 << OCF2A);
	
	next_phase = 0;
	/* Turn on global interrupt */
	sei();
}

void pause_ssg(void) {
	game_pause = 1;
}

void unpause_ssg(void) {
	game_pause = 0;
}

void display_number(uint8_t number, uint8_t digit) {
	if (digit == 0) {
		PORTD &= ~(1UL << 2);
	} else {
		PORTD |= 1 << 2;
	}
	
	PORTC = seven_seg[number];
}

void display_off(void) {
	PORTC = 0;
}

ISR(TIMER2_COMPA_vect) {
	
	if (get_power_pellet_eaten()) {
		// Get current time and set the last time that the power pellet was eaten
		
		if (game_pause) {
			current_time = get_paused_time();
		} else {
			current_time = get_current_time();
		}
		power_pellet_eaten_time = get_power_pellet_time();
		
		uint8_t value = 0;
		// Calculate the number of seconds left
		// Its then incremented by 1 to round up (E.g 14.8 (mod10) = 4 but needs to be 5).
		uint8_t time = (power_pellet_eaten_time + 15000 - current_time) / 1000;
		time++;
		
		if (ssg_cc == 0) {
			value = time % 10;
		} else if (time < 1) {
			value = 0;
		} else {
			value = (time/10) % 10;
		}
		
		// Display the number on ssg
		display_number(value, ssg_cc);
		
		// Swap the display CC to display other side
		ssg_cc = 1 - ssg_cc;
		
		// If time left is less than 10 seconds do not display the left hand side of ssg
		if (time < 10) {
			ssg_cc = 0;
		}
	} else {
		display_off();
	}
}

