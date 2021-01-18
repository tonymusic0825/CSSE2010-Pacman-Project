/*
 * lives.c
 *
 * Created: 2019-10-08 오후 9:24:13
 *  Author: user
 */ 

#include "lives.h"
#include <stdlib.h>

uint8_t lives;

void init_lives(void) {
	lives = 3;
}

void decrease_lives(void) {
	lives--;
}

uint8_t get_lives(void) {
	return lives;
}

void set_lives(uint8_t value) {
	lives = value;
}

