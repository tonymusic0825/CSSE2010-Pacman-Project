/*
 * lives.h
 *
 * Created: 2019-10-08 오후 9:25:00
 *  Author: user
 */ 
#include <stdint.h>

#ifndef LIVES_H_
#define LIVES_H_

void init_lives(void);
void decrease_lives(void);
uint8_t get_lives(void);
void set_lives(uint8_t value);


#endif /* LIVES_H_ */