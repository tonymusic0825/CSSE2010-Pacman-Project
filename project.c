/*
 * project.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by Youngsu Choi
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"
#include "lives.h"
#include "buzzer.h"
#include "seve_seg_display.h"
#include "joystick.h"
#include "ledmatrix.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_level_complete(void);
void handle_game_over(void);

// ASCII code for Escape character
#define ESCAPE_CHAR 27
uint8_t seven_seg_data[10] = {63,6,91,79,102,109,125,7,127,111};
/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	// Initialize lives 
	init_lives();
	
	set_prescalar(2);
	
	while(1) {
		new_game();
		//Led Matrix
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	// For seven segment display
	init_ssg();
	
	// LEDs for lives
	DDRA |= (1 << DDRA7) | (1 << DDRA6) | (1 << DDRA5);
	PORTA |= (1 << PORTA7) | (1 << PORTA6) | (1 << PORTA5);
	DDRD &= ~(1 << 7);
	
	ledmatrix_setup();
	init_button_interrupts();
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	// Buzzer
	init_buzzer();
	
	// Joystick
	init_joystick();
	
	init_timer0();
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a messages
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Pac-Man"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010/7201 project by Youngsu Choi - 45182822"));
	if (signature_check()) {
		move_cursor(10, 14);
		printf_P(PSTR("Saved Game Exists"));
	}
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("45182822", COLOUR_YELLOW);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				ledmatrix_clear();
				reset_timer0();
				return;
			}
		}
	}
}

void completely_new_game(void) {
	set_prescalar(2);
	new_game();
	reset_power_pellet_eaten();
	reset_dead_ghosts();
	set_alive_pellet_ghosts();
	PORTA |= (1 << PORTA7) | (1 << PORTA6) | (1 << PORTA5);
	init_lives();
	play_game();
	handle_game_over();
}

void new_game(void) {
	// Initialise the game and display
	initialise_game();
	
	// Initialise the score
	init_score();
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time;
	uint32_t pacman_last_move_time;
	uint32_t ghost_last_move_time;
	uint32_t ghost_last_move_time2;
	uint32_t ghost_last_move_time3;
	uint32_t ghost_last_move_time4;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	uint32_t power_pellet_eaten_time = 0;
	
	// Get the current time and remember this as the last time the projectiles
    // were moved.
	current_time = get_current_time();
	pacman_last_move_time = current_time;
	ghost_last_move_time = current_time;
	ghost_last_move_time2 = current_time;
	ghost_last_move_time3 = current_time;
	ghost_last_move_time4 = current_time;

	// We play the game until it's over
	while(!is_game_over()) {	
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		power_pellet_eaten_time = get_power_pellet_time();
		
		// Ledmatrix
		draw_ledmatrix_game();	
		
		if (get_power_pellet_eaten() && current_time >= power_pellet_eaten_time + 15000) {
			reset_power_pellet_eaten();
			reset_dead_ghosts();
			set_alive_pellet_ghosts();
			reset_last_ghost_score();
			PORTC = 0;
		}
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
										// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
				} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process 
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		
		// Process the input. 
		if(button==3 || escape_sequence_char=='D') {
			// Button 3 pressed OR left cursor key escape sequence completed 
			// Attempt to move left
			change_pacman_direction(DIRN_LEFT);
		} else if(button==2 || escape_sequence_char=='A') {
			// Button 2 pressed or up cursor key escape sequence completed
			change_pacman_direction(DIRN_UP);
		} else if(button==1 || escape_sequence_char=='B') {
			// Button 1 pressed OR down cursor key escape sequence completed
			change_pacman_direction(DIRN_DOWN);
		} else if(button==0 || escape_sequence_char=='C') {
			// Button 0 pressed OR right cursor key escape sequence completed 
			// Attempt to move right
			change_pacman_direction(DIRN_RIGHT);
		} else if(serial_input == 'p' || serial_input == 'P') {
			pause_time();
			pause_ssg();
			change_game_paused();
			while(1) {
				if (button_pushed()){
					// do nothing
				}
				if(serial_input_available()) {
					serial_input = fgetc(stdin);
					if(serial_input == 'p' || serial_input =='P') {
						unpause_time();
						unpause_ssg();
						break;
					} else if (serial_input == 'n' || serial_input == 'N') {
						completely_new_game();
					} else if (serial_input == 's' || serial_input == 'S') {
						save_game();
					} else if (serial_input == 'o' || serial_input == 'O') {
						if (signature_check()) {
							load_game();
							break;
						}
					}
				}
			}
			change_game_paused();
		} else if (serial_input == 'n' || serial_input == 'N') {
			completely_new_game();
		// else - invalid input or we're part way through an escape sequence -
		// do nothing
		} else if (serial_input == 's' || serial_input == 'S') {
			save_game();
		} else if (serial_input == 'o' || serial_input == 'O') {
			if (signature_check()) {
				pause_ssg();
				load_game();
				ghost_last_move_time = get_current_time();
				ghost_last_move_time2 = get_current_time();
				ghost_last_move_time3 = get_current_time();
				ghost_last_move_time4 = get_current_time();
				pacman_last_move_time = get_current_time();
				unpause_ssg();	
			}
		} else {
			if (get_current_joystick_dirn() == 1) {
				change_pacman_direction(DIRN_UP);
			} else if (get_current_joystick_dirn() == 2) {
				if (!(change_pacman_direction(DIRN_UP))) {
					change_pacman_direction(DIRN_RIGHT);
				} else {
					change_pacman_direction(DIRN_UP);
				}
			} else if (get_current_joystick_dirn() == 3) {
				change_pacman_direction(DIRN_RIGHT);
			} else if (get_current_joystick_dirn() == 4) {
				if (!(change_pacman_direction(DIRN_DOWN))) {
					change_pacman_direction(DIRN_RIGHT);
				} else {
					change_pacman_direction(DIRN_DOWN);
				}
			} else if (get_current_joystick_dirn() == 5) {
				change_pacman_direction(DIRN_DOWN);
			} else if (get_current_joystick_dirn() == 6) {
				if (!(change_pacman_direction(DIRN_DOWN))) {
					change_pacman_direction(DIRN_LEFT);
				} else {
					change_pacman_direction(DIRN_DOWN);
				}
			} else if (get_current_joystick_dirn() == 7) {
				change_pacman_direction(DIRN_LEFT);
			} else if (get_current_joystick_dirn() == 8) {
				if (!(change_pacman_direction(DIRN_UP))) {
					change_pacman_direction(DIRN_LEFT);
				} else {
					change_pacman_direction(DIRN_UP);
				}
			}
		}
		
		
		current_time = get_current_time();
		
		if(!is_game_over() && current_time >= pacman_last_move_time + 400) {
			// 400ms (0.4 second) has passed since the last time we moved 
			// the pac-man - move it.
			move_pacman();
			pacman_last_move_time = current_time;
			
			// Check if the move finished the level - and restart if so
			if(is_level_complete()) {
				handle_level_complete();	// This will pause until a button is pushed
				initialise_game_level();
				// Update our timers since we have a pause above
				pacman_last_move_time = ghost_last_move_time4 = ghost_last_move_time2 =
				ghost_last_move_time3 = ghost_last_move_time = get_current_time();
			}
		}
		if(!is_game_over() && current_time >= ghost_last_move_time + 500 &&
			get_dead_ghost(0)) {
			// 500ms (0.5 second) has passed since the last time we moved the
			// ghosts - move them
			move_ghost(0);
			ghost_last_move_time = current_time;
		}
		if (!is_game_over() && current_time >= ghost_last_move_time2 + 525 &&
			get_dead_ghost(1)) {
			move_ghost(1);
			ghost_last_move_time2 = current_time;
		}
		if (!is_game_over() && current_time >= ghost_last_move_time3 + 550 &&
			get_dead_ghost(2)) {
			move_ghost(2);
			ghost_last_move_time3 = current_time;
		}
		if (!is_game_over() && current_time >= ghost_last_move_time4 + 600 &&
			get_dead_ghost(3)) {
			move_ghost(3);
			ghost_last_move_time4 = current_time;
		}
	}
	// We get here if the game is over.
}

void handle_level_complete(void) {
	move_cursor(35,10);
	printf_P(PSTR("Level complete"));
	move_cursor(35,11);
	printf_P(PSTR("Push a button or key to continue"));
	// Clear any characters in the serial input buffer - to make
	// sure we only use key presses from now on.
	clear_serial_input_buffer();
	while(button_pushed() == NO_BUTTON_PUSHED && !serial_input_available()) {
		; // wait
	}
	// Throw away any characters in the serial input buffer
	clear_serial_input_buffer();

}

void handle_game_over(void) {
	PORTA = (0 << PORTA7) | (0 << PORTA6) | (0 << PORTA5);
		
	move_cursor(35,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(35,16);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		int serial_input = -1;
		if (serial_input_available()) {
			serial_input = fgetc(stdin);
			if (serial_input == 'N' || serial_input == 'n') {
				break;
				; // wait
			}
		}
	} 
	completely_new_game();		
}
