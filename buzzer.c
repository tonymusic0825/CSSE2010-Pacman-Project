/*
 * buzzer.c
 *
 * Created: 2019-10-08 오후 9:47:33
 *  Author: Youngsu Choi
 */ 

#include "buzzer.h"
#include "game.h"
#include "terminalio.h"
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "timer0.h"

uint8_t game_paused = 0;
volatile uint16_t count = 0;
volatile uint32_t current_sound_time;
volatile uint32_t sound_time_end;
volatile uint8_t partial_death;
volatile uint8_t power_pellet;
volatile uint8_t ghost_eat;
volatile uint8_t mute;

void toggle_game_paused(uint8_t value) {
	game_paused = value;
}

void init_buzzer(void) {
	DDRD |= (1 << 5);
	
	TCCR1A = (0 << COM1A1) | (1 << COM1A0) | (0 << WGM11) | (0 << WGM10); 
	TCCR1B = (0 << WGM13) | (1 << WGM12); 
	
	/* Enable interrupt timer on output compare match 
	*/
	TIMSK1 = (1 << OCIE1A);

	/* Ensure interrupt flag is cleared */
	TIFR1 = (1 << OCF1A);
	
	sei();
}

void set_prescalar(uint8_t sound_code) {
	if ((PIND & (1 << 7)) == 0) {
		if (sound_code == 1) {
			OCR1A = 399;
			power_pellet = 1;
			TCCR1B |= (1 << CS11) | (1 << CS10);
		}
		
		if (sound_code == 2) {
			OCR1A = 399;
			partial_death = 1;
			TCCR1B |= (1 << CS11);
		}
		
		if (sound_code == 3) {
			OCR1A = 399;
			ghost_eat = 1;
			TCCR1B |= (1 << CS11) | (1 << CS10);
		}
		
		sound_time_end = get_sound_time() + 200;
	}
}

ISR(TIMER1_COMPA_vect) {
	
	current_sound_time = get_current_time();
	
	if (ghost_eat == 1) {
		if (current_sound_time >= sound_time_end + 1500) {
			TCCR1B &= ~((1 << CS12) | (1 << CS11) | (1 << CS10));
			ghost_eat = 0;
			DDRD &= ~(1 << 5);
			DDRD |= (1 << 5);
		}
	}

	if (partial_death == 1) {
		if (current_sound_time >= sound_time_end + 1000) {
			OCR1A = 449;
		}
		if (current_sound_time >= sound_time_end + 1500) {
			OCR1A = 499;
		}
		if (current_sound_time >= sound_time_end + 2000) {
			TCCR1B &= ~((1 << CS12));
			TCCR1B &= ~((1 << CS10));
			TCCR1B &= ~((1 << CS11));
			partial_death = 0;
			DDRD &= ~(1 << 5);
			DDRD |= (1 << 5);
		}	
	}
	
	if (power_pellet == 1) {
		if (current_sound_time >= sound_time_end + 500) {
			OCR1A = 349;
		}
		
		if (current_sound_time >= sound_time_end + 800) {
			OCR1A = 299;
		}
		
		if (current_sound_time >= sound_time_end + 1100) {
			TCCR1B &= ~((1 << CS12));
			TCCR1B &= ~((1 << CS10));
			TCCR1B &= ~((1 << CS11));
			DDRD &= ~(1 << 5);
			DDRD |= (1 << 5);
			power_pellet = 0;
		}
	}
}