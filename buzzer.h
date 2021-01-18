/*
 * buzzer.h
 *
 * Created: 2019-10-08 오후 10:01:27
 *  Author: Youngsu Choi
 */ 
#include <stdio.h>

#ifndef BUZZER_H_
#define BUZZER_H_

void init_buzzer(void);
void toggle_game_paused(uint8_t value);
void set_prescalar(uint8_t sound_code);


#endif /* BUZZER_H_ */