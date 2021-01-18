/*
 * joystick.h
 *
 * Created: 2019-10-09 오후 12:07:30
 *  Author: user
 */ 


#ifndef JOYSTICK_H_
#define JOYSTICK_H_

#include <stdint.h>

#define CENTRE (0)
#define NORTH (1)
#define NORTH_EAST (2)
#define EAST (3)
#define SOUTH_EAST (4)
#define SOUTH (5)
#define SOUTH_WEST (6)
#define WEST (7)
#define NORTH_WEST (8)

uint8_t has_joystick_moved(void);
void init_joystick(void);
uint8_t get_current_joystick_dirn(void);
uint8_t get_last_joystick_dirn(void);
uint32_t get_joystick_x(void);
uint32_t get_joystick_y(void);

#endif /* JOYSTICK_H_ */