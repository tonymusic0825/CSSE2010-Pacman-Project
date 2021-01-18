/*
 * score.c
 *
 * Written by Peter Sutton
 */

#include "score.h"

uint32_t score;
uint32_t high_score;

void init_score(void) {
	score = 0;
	high_score = 0;
}

void add_to_score(uint16_t value) {
	score += value;
}

void set_score(uint16_t value) {
	score = value;
}

void set_high_score(uint16_t value) {
	high_score = value;
}

uint32_t get_score(void) {
	return score;
}

uint32_t get_high_score(void) {
	return high_score;
}
