/*
 * joystick.c
 *
 * Created: 2019-10-09 오전 11:47:53
 *  Author: user
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "joystick.h"
#include "timer0.h"

// X/Y coordinate of joystick positions
static volatile uint16_t x;
static volatile uint16_t y;

// 0 if X value was converted lastly. Otherwise it's Y value
static volatile uint8_t x_or_y;

// The last time the joystick moved
static uint32_t joystick_time = 0; // Originally hasn't moved

// The last/current joystick direction
static uint8_t joystick_dirn;
static uint8_t last_joystick_dirn = CENTRE; // Originally at the centre

void init_joystick(void) {
	
	ADMUX = (1<<REFS0);
	
	// Turn on the ADC (but don't start a conversion yet).
	// Set up the conversion complete interrupt.
	// Choose a clock divider of 64.
	// Enable interrupt.
	ADCSRA = (1<<ADEN)|(1<<ADIE)|(1<<ADPS2)|(1<<ADPS1);
	
	x = 512;
	y = 512;
	x_or_y = 0;
	
	// Start the ADC conversion
	ADCSRA |= (1<<ADSC);
}

uint8_t has_joystick_moved(void) {
	
	// Save whether interrupts were enabled and turn them off
	int8_t interrupts_were_enabled = bit_is_set(SREG, SREG_I);
	cli();
	
	// Assume that the joystick hasn't changed direction since last conversion
	uint8_t return_value = 0;
	
	// Set current joystick direction
	joystick_dirn = get_current_joystick_dirn();
	
	if (joystick_time == 0 && joystick_dirn != CENTRE) {
		// Set the last time joystick moved and return current joystick direction
		joystick_time = get_current_time();
		// Set the last joystick direction to current joystick direction
		last_joystick_dirn = joystick_dirn;
		return_value = 1;
	} else if (joystick_time != 0 && (get_current_time() > joystick_time + 300)) {
		if (joystick_dirn == last_joystick_dirn) {
			return 0; // Joystick hasn't changed directions
		} else {
			// Set the last time joystick moved and return current joystick direction
			joystick_time = get_current_time();
			// Set the last joystick direction to current joystick direction
			last_joystick_dirn = joystick_dirn;
			return_value = 1;	
		}
	}
	
	if(interrupts_were_enabled) {
		sei();
	}
	
	return return_value;
}

uint8_t get_last_joystick_dirn(void) {
	return last_joystick_dirn;
}

uint32_t get_joystick_x(void) {
	return x;
}

uint32_t get_joystick_y(void) {
	return y;
}

uint8_t get_current_joystick_dirn(void) {

	if (x < 550 && x > 450 && y < 550 && y > 450) {
		return CENTRE;
	} 
	if (x > 570 && y < 570 && y > 430) {
		return NORTH;
	} 
	if (x > 600 && y < 400) {
		return NORTH_EAST;
	} 
	if (x < 550 && x > 450 && y < 450) {
		return EAST;
	}
	if (x < 450 && y < 450) {
		return SOUTH_EAST;
	}
	if (x < 450 && y < 550 && y > 450) {
		return SOUTH;
	}
	if (x < 450 && y > 550) {
		return SOUTH_WEST;
	}
	if (x < 550 && x > 450 && y > 550) {
		return WEST;
	}
	if (x > 550 && y > 550) {
		return NORTH_WEST;
	}
	
	return CENTRE;
}

// Interrupt for conversion of value X & Y when joystick is moved
ISR(ADC_vect) {

	uint16_t value = ADC;
	
	// Calculate values for either X or Y
	if (x_or_y == 0) {
		x = value;
	} else {
		y = value;
	}
	
	// Change x_or_y to 1 if last valued calculated was x and vice versa for y
	x_or_y = 1 - x_or_y;
	if (x_or_y == 0) {
		ADMUX &= ~1;
	} else { 
		ADMUX |= 1;
	}
	
	//Start the ADC conversion again
	ADCSRA |= (1<<ADSC);
}