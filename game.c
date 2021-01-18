/*
** game.c
**
** Author: Peter Sutton, Modified by Youngsu Choi
**
*/

#include "game.h"
#include <stdio.h>
#include "ledmatrix.h"
#include "terminalio.h"
#include "line_drawing_characters.h"
#include "pixel_colour.h"
#include <avr/pgmspace.h>
#include <stdlib.h>
#include "timer0.h"
#include "score.h"
#include "buzzer.h"
#include "lives.h"
#include <avr/eeprom.h>
/* Stdlib needed for random() - random number generator */

///////////////////////////////////////////////////////////
// Initial game field
// The string below has 31 elements for each of the 31 rows. The index into 
// the string is row_number * 31 + column_number.
// Each location is one of the following values:
// (space) - nothing at this location
// - - horizontal wall at this location - uses LINE_HORIZONTAL
// | - vertical wall at this location - uses LINE_VERTICAL
// F - wall is down and to the right - uses LINE_DOWN_AND_RIGHT
// 7 - wall is down and to the left - uses LINE_DOWN_AND_LEFT
// L - wall is up and to the right - uses LINE_UP_AND_RIGHT
// J - wall is up and to the left - uses LINE_UP_AND_LEFT
// > - wall is vertical and to the right - uses LINE_VERTICAL_AND_RIGHT
// < - wall is vertical and to the left - uses LINE_VERTICAL_AND_LEFT
// ^ - wall is horizontal and up - uses LINE_HORIZONTAL_AND_UP
// v - wall is horizontal and down - uses LINE_HORIZONTAL_AND_DOWN
// + - wall is in all directions - uses LINE_VERTICAL_AND_HORIZONTAL
// . - pacdot initially at this location
// P - power pellet initial location (initially implemented just as a pac-dot)
//
// This array is stored in program memory to preserve RAM. (1 is added to 
// size to allow for null character at end of string.)
// (Note that string constants with whitespace between them are concatenated.)

static const char init_game_field[FIELD_HEIGHT*FIELD_WIDTH + 1] PROGMEM =
	"F-------------v-v-------------7"
	"|.............| |.............|"
	"|.F---7.F---7.| |.F---7.F---7.|"
	"|.|   |.L---J.L-J.L---J.|   |.|"
	"|.|   |.................|   |.|"
	"|.|   |.F---7.F-7.F---7.|   |.|"
	"|PL---J.L---J.L-J.L---J.L---JP|"
	"|.............................|"
	"|.F---7.F7.F-------7.F7.F---7.|"
	"|.L---J.||.L--7 F--J.||.L---J.|"
	"|.......||....| |....||.......|"
	"L-----7.|L--7 | | F--J|.F-----J"
	"      |.|F--J L-J L--7|.|      "
	"      |.||           ||.|      "
	"------J.LJ F--   --7 LJ.L------"
	"       .   |       |   .       "
	"------7.F7 L-------J F7.F------"
	"      |.||           ||.|      "
	"      |.|| F-------7 ||.|      "
	"F-----J.LJ L--7 F--J LJ.L-----7"
	"|.............| |.............|"
	"|.F---7.F---7.| |.F---7.F---7.|"
	"|.L-7 |.L---J.L-J.L---J.| F-J.|"
	"|P..| |........ ........| |..P|"
	">-7.| |.F7.F-------7.F7.| |.F-<"
	">-J.L-J.||.L--7 F--J.||.L-J.L-<"
	"|.......||....| |....||.......|"
	"|.F-----JL--7.| |.F--JL-----7.|"
	"|.L---------J.L-J.L---------J.|"
	"|.............................|"
	"L-----------------------------J";

// Array to store the game dots (pacdots) - each element in the array is a 32 bit integer, 
// representing the absence/presence of pacdots in each row. The first element in 
// the array is for row 0 (top), the last for row 30 (bottom).
// Within the 32-bit integer, the least significant 31 bits are used to store
// the data for each column in that row. The least significant bit (bit 0) is the value
// for column 0 (left hand column), the second most significant bit (bit 30) is for
// column 30 (right hand column). The most significant bit (bit 31) is unused. A value
// of 1 in a bit represents the presence of a pacdot, 0 is the absence.
//
// This array will be initially set from the data in init_game_field above and
// will be updated as pacdots are eaten.
static uint32_t pacdots[FIELD_HEIGHT];
// We also keep a count of the number of pac-dots remaining on the game field
static uint16_t num_pacdots;

// Initial pacman location and direction
#define INIT_PACMAN_X 15
#define INIT_PACMAN_Y 23
#define INIT_PACMAN_DIRN DIRN_RIGHT

// Location of the ghost's home - ghosts will be every 2 cells
// starting from the left most position (12,15) to (18,15)
#define GHOST_HOME_Y 15
#define GHOST_HOME_X_LEFT 12
#define GHOST_HOME_X_RIGHT 18
#define GHOST_HOME_ENTRY_Y 14
#define GHOST_HOME_ENTRY_X_LEFT 14
#define GHOST_HOME_ENTRY_X_RIGHT 16
#define INIT_GHOST_DIRN DIRN_RIGHT

// Values to represent the contents of a cell (x,y)
// Non-negative values are used for ghost numbers (0 to 3)
#define CELL_IS_GHOST_HOME (-1)
#define CELL_IS_WALL (-2)
#define CELL_CONTAINS_PACMAN (-3)
#define CELL_CONTAINS_PACDOT (-4)
#define CELL_EMPTY (-5)
#define CELL_CONTAINS_POWER_PELLET (-6)

// Terminal colours to be used
static uint8_t ghost_colours[NUM_GHOSTS] = {
	BG_RED, BG_GREEN, BG_CYAN, BG_MAGENTA
};
#define PACMAN_COLOUR (FG_YELLOW)

// EEPROM save location/sizes
static const char signature[8] = "PacmanYC";

// Signature
#define SIGNATURE 0
#define SIGNATURE_SIZE 8

// Scores
#define SCORE 8
#define SCORE_SIZE 4
#define HIGH_SCORE 12
#define HIGH_SCORE_SIZE 4

// Pacdots
#define NUM_PACDOTS 16
#define NUM_PACDOTS_SIZE 2
#define PACDOTS 18
#define PACDOTS_SIZE 124

// Power Pellet
#define POWER_PELLET_STATUS 142
#define POWER_PELLET_STATUS_SIZE 1
#define POWER_PELLET_EATEN_TIME 143
#define POWER_PELLET_EATEN_TIME_SIZE 4
#define POWER_PELLET_GHOSTS	147
#define POWER_PELLET_GHOSTS_SIZE 4
#define ALIVE_GHOSTS 151
#define ALIVE_GHOSTS_SIZE 1

// Time
#define TIME 152
#define TIME_SIZE 4

// Lives
#define LIVES 156
#define LIVES_SIZE 1

// Pacman
#define PACMAN_X 157
#define PACMAN_X_SIZE 1
#define PACMAN_Y 158
#define PACMAN_Y_SIZE 1
#define PACMAN_DIRECTION 159
#define PACMAN_DIRECTION_SIZE 1

// Ghosts
#define GHOSTS_X 160
#define GHOSTS_X_SIZE 4
#define GHOSTS_Y 164
#define GHOSTS_Y_SIZE 4
#define GHOSTS_DIRN 168
#define GHOSTS_DIRN_SIZE 4

// Last ghost score
#define LAST_GHOST_SCORE 180
#define LAST_GHOST_SCORE_SIZE 2

// Unicode characters used to represent the pacman in each direction
static const char* pacman_characters[NUM_DIRECTION_VALUES] = {
	"\u15E4", "\u15E2", "\u15E7", "\u15E3"
};

// Location of pacman (values will be in the range 0 to FIELD_WIDTH - 1 or FIELD_HEIGHT - 1)
static uint8_t pacman_x;
static uint8_t pacman_y;

// Direction of pacman movement (one of the direction values in game.h)
static uint8_t pacman_direction;

// Locations and directions of the ghosts
static uint8_t ghost_x[NUM_GHOSTS];
static uint8_t ghost_y[NUM_GHOSTS];
static uint8_t ghost_direction[NUM_GHOSTS];

// Indicate whether the game is running or not - 1 indicates yes,
// 0 indicates game over
static uint8_t game_running;

// Power pellet variables
static uint8_t power_pellet_eaten = 0;
static uint32_t power_pellet_eaten_time;
static uint8_t pellet_ghosts[NUM_GHOSTS] = {1, 1, 1, 1};
static uint8_t alive_pellet_ghosts = 4;
static uint16_t last_ghost_score;

// Pause
static uint8_t is_game_paused = 0;

// Sounds variables
static uint32_t sound_enable_time = 0;

// Load Variables
uint16_t load_score[1];
uint16_t load_highscore[1];

uint16_t load_num_pacdots[1];
uint32_t load_pacdots[FIELD_HEIGHT];

uint8_t load_pellet_status[1];
uint32_t load_eaten_pellet_time[1];
uint8_t load_pellet_ghosts[NUM_GHOSTS];
uint8_t load_alive_pellet_ghosts[1];
uint16_t load_last_ghost_score[1];

uint32_t load_time[1];

uint8_t load_lives[1];

uint8_t load_pacman_x[1];
uint8_t load_pacman_y[1];
uint8_t load_pacman_dirn[1];

uint8_t load_ghost_x[NUM_GHOSTS];
uint8_t load_ghost_y[NUM_GHOSTS];
uint8_t load_ghost_dirn[NUM_GHOSTS];

///////////////////////////////////////////////////////////
// Private Functions
//
// is_wall_at() returns true (1) if there is a wall at the given 
// game location, 0 otherwise
static int8_t is_wall_at (uint8_t x, uint8_t y) {
	// Get information about any wall in that position
	char wall_character = pgm_read_byte(& init_game_field[y * FIELD_WIDTH + x]);
	return (wall_character != ' ' && wall_character != '.'
			&& wall_character != 'P');
}

// is_pacman_at() returns true(1) if the pacman is at the given 
// game location (x,y), 0 otherwise
static int8_t is_pacman_at(uint8_t x, uint8_t y) {
	return (x == pacman_x && y == pacman_y);
}

// is_pacdot_at() returns true (1) if there is a pacdot at the given
// game location, 0 otherwise
static int8_t is_pacdot_at (uint8_t x, uint8_t y) {
	// Get the details for the row
	uint32_t dots_on_row = pacdots[y];
	// Extract the value for the column x (which is in bit x)
	if(dots_on_row & (1UL<< x)) {
		return 1;
	} else {
		return 0;
	}
}

static int8_t is_power_pellet_at(uint8_t x, uint8_t y) {
	if ((y == 6 || y == 23) && (x == 1 || x == 29)) {
		uint32_t dots_on_row = pacdots[y];
		
		if(dots_on_row & (1UL << x)) {
			return 1;
		} else {
			return 0;
		}
	}
	return 0;
}

// Returns true (1) if the given location is the home of the ghosts
// (this includes the entry to the home of the ghosts)
static int8_t is_ghost_home(uint8_t x, uint8_t y) {
	if(y == GHOST_HOME_Y && x >= GHOST_HOME_X_LEFT && x <= GHOST_HOME_X_RIGHT) {
		return 1;
	} else if(y == GHOST_HOME_ENTRY_Y && x >= GHOST_HOME_ENTRY_X_LEFT
			&& x <= GHOST_HOME_ENTRY_X_RIGHT) {
		return 1;
	} else {
		return 0;
	}
}

void update_score(uint8_t value) {

	// Add score and reprint scores
	add_to_score(value);
	move_cursor(33, 4);
	printf("%10ld", get_score());
	
	// Update high score if needed and reprint high score
	if (get_score() > get_high_score()) {
		set_high_score(get_score());
		move_cursor(33, 6);
		printf("%10ld", get_high_score());
	}
	
}

// The pac-man has just arrived in a location occupied by a pac-dot. Update
// our array which keeps track of remaining pacdots. Update and output the
// count of remaining pac-dots.
// See initialise_pacdots() below for information on how the pacdots array
// is initialised.
static void eat_pacdot(uint8_t value) {

	pacdots[pacman_y] ^= 1UL << pacman_x;
	
	// Decrement number of pacdots
	num_pacdots -= 1;
	move_cursor(33, 1);
	printf("%s", "Remaining number of pac-dots: ");
	
	if (num_pacdots >= 100) {
		printf("%d", num_pacdots);
		} else {
		printf("%s", " ");
		printf("%d", num_pacdots);
	}
	
	// Update score
	update_score(value);
}

// what_is_at(x,y) returns
//		CELL_EMPTY, CELL_CONTAINS_PACDOT, CELL_CONTAINS_PACMAN, CELL_IS_WALL,
//		CELL_IS_GHOST_HOME or the ghost number if the cell contains a ghost
static int8_t what_is_at(uint8_t x, uint8_t y) {
	if(is_pacman_at(x,y)) {
		return CELL_CONTAINS_PACMAN;
	} else { // Check for ghosts next - these take priority over dots
		// BUT note that there may be a pacdot at the same location
		for(int8_t i = 0; i < NUM_GHOSTS; i++) {
			if(x == ghost_x[i] && y == ghost_y[i]) {
				return i;	// ghost number
			}
		}
	}
	if (is_power_pellet_at(x, y)) {
		return CELL_CONTAINS_POWER_PELLET;
	} else if (is_pacdot_at(x,y)) {
		return CELL_CONTAINS_PACDOT;
	} else if (is_wall_at(x,y)) {
		return CELL_IS_WALL;
	} else if(is_ghost_home(x,y)) {
		return CELL_IS_GHOST_HOME;
	}
	// If we get here, we haven't found anything else - cell is empty
	return CELL_EMPTY;
}

// what_is_in_dirn(x,y,direction) returns what is in the cell one from 
// the cell at (x,y) in the given direction - provided that is not off
// the game field. (If it is, we just indicate that a wall is there.)
// We check for a wall first because this also checks that we're not at the edge
static int8_t what_is_in_dirn(uint8_t x, uint8_t y, uint8_t direction) {
	// delta_x and delta_y keep track of the change to the current x,y
	// - we set these based on the direction we're checking in. One of these
	// will end up as -1 or +1, the other will stay at 0.
	int8_t delta_x = 0;
	int8_t delta_y = 0;
	switch(direction) {
		case DIRN_LEFT:
			if(x == 0) {
				// We can't move left since we're at the edge
				return CELL_IS_WALL;
			}
			delta_x = -1;
			break;
		case DIRN_RIGHT:
			if(x == FIELD_WIDTH-1) {
				// We can't move right since we're at the edge
				return CELL_IS_WALL;
			}
			delta_x = 1;
			break;
		case DIRN_UP:
			if(y == 0) {
				// We can't move up since we're at the edge
				return CELL_IS_WALL;
			}
			delta_y = -1;
			break;
		case DIRN_DOWN:
			if(y == FIELD_HEIGHT-1) {
				// We can't move down since we're at the edge
				return CELL_IS_WALL;
			}
			delta_y = 1;
			break;
		default:	// Shouldn't happen - we just return CELL_IS_WALL if
			// the direction given is invalid
			return CELL_IS_WALL;
	}	
	return what_is_at(x + delta_x, y + delta_y);
}

// determine_dirns_ghost_can_move_in()
// Returns a number that indicates whether a ghost at the given x,y location
// can move in each direction. The lower 4 bits of the return value will each
// be 0 or 1 - 0 means can't move in the direction, 1 means can move. The bit
// positions are
// - 0 (DIRN_LEFT) (least significant) - left
// - 1 (DIRN_RIGHT) - right
// - 2 (DIRN_UP) - up
// - 3 (DIRN_DOWN) - down
// Movement in the given direction can only happen if the cell is one of
// - the pacman
// - a pacdot
// - empty
// It can not move there if the cell is a ghost or a wall.
// If we're in the ghost home we can move to another cell in the ghost home.
// If we're outside the ghost home we can't move into it.
static int8_t determine_dirns_ghost_can_move_in(uint8_t x, uint8_t y) {
	int8_t return_value = 0;
	int8_t posn_is_in_ghost_home = is_ghost_home(x,y);
	for(int8_t dirn = DIRN_LEFT; dirn <= DIRN_DOWN; dirn++) {
		int8_t adjacent_cell_contents = what_is_in_dirn(x,y,dirn);
		
		if(adjacent_cell_contents < CELL_IS_WALL) {
			// cell is empty or pacdot or pacman
			return_value |= (1 << dirn);
		} else if(posn_is_in_ghost_home && adjacent_cell_contents == CELL_IS_GHOST_HOME) {
			// we're in the ghost home and can move to an empty cell in the ghost home
			return_value |= (1 << dirn);
		} 
	}
	return return_value;
}

// direction_to_pacman() is called for a ghost position and we return a direction
// to move in that will take us closer to the pacman (from DIRN_LEFT to DIRN_DOWN)
// or -1 if we can't move at all. (Note we can only move into cells that are empty
// OR contain a pacdot OR contain the pacman. We can't move into walls or cells 
// that contain ghosts.)
static int8_t direction_to_pacman(uint8_t x, uint8_t y) {
	int8_t delta_x = pacman_x - x;
	int8_t delta_y = pacman_y - y;
	// Work out which direction options are possible
	int8_t dirn_options = determine_dirns_ghost_can_move_in(x, y);
	if(dirn_options == 0) {
		// Can't move
		return -1;
	}
	
	if(abs(delta_x) < abs(delta_y)) {
		// Pacman is further away in y direction - try this direction (up/down) first
		if(delta_y < 0) {
			if(dirn_options & (1 << DIRN_UP)) {
				return DIRN_UP;
			}
			// Can't move up - move on to checking left/right
		} else if(delta_y > 0) {
			if(dirn_options & (1 << DIRN_DOWN)) {
				return DIRN_DOWN;
			}
			// Can't move down - move on to checking left/right
		} // else delta_y is 0 - so try left/right
	}
	// Try the x direction 
	if(delta_x < 0) {
		if(dirn_options & (1 << DIRN_LEFT)) {
			return DIRN_LEFT;
		}
		// Pacman is left but we can't move left - try up or down
		if(delta_y < 0) {
			if(dirn_options & (1 << DIRN_UP)) {
				return DIRN_UP;
			}
		} else if(dirn_options & (1 << DIRN_DOWN)) {
			return DIRN_DOWN;
		}
	} else {
		if(dirn_options & (1 << DIRN_RIGHT)) {
			return DIRN_RIGHT;
		}
		// Pacman is right (or directly above/below) but we can't move right - try up or down
		if(delta_y < 0) {
			if(dirn_options & (1 << DIRN_UP)) {
				return DIRN_UP;
			} 
		} else if(dirn_options & (1 << DIRN_DOWN)) {
			return DIRN_DOWN;
		}
	}
	// Just move whichever way we can - try until we find one that works
	for(int8_t dirn = DIRN_LEFT; dirn <= DIRN_DOWN; dirn++) {
		if(dirn_options & (1 << dirn)) {
			return dirn;
		}
	}
	// We can't move in any direction
	return -1;
}

// determine_ghost_direction_to_move()
// 
// Determine the direction the given ghost (0 to 3) should move in.
// (Each ghost uses a different approach to moving.)
// Return -1 if the ghost can't move (e.g. surrounded by walls and other
// ghosts).
static int8_t determine_ghost_direction_to_move(uint8_t ghostnum) {
	uint8_t x = ghost_x[ghostnum];
	uint8_t y = ghost_y[ghostnum];
	uint8_t curdirn = ghost_direction[ghostnum];

	int8_t dirn_options = determine_dirns_ghost_can_move_in(x,y);
	if(dirn_options == 0) {
		// ghost has no options - indicate that the ghost can't move
		return -1;
	}
	
	if(is_ghost_home(x,y)) {
		// Attempt to move ghost out of home - try UP
		if(dirn_options & (1 << DIRN_UP)) {
			return DIRN_UP;
		}
		// If this doesn't work, we'll try the usual algorithm
	}
	switch(ghostnum) {
		case 0:
			// Ghost 0 will always try to move towards the pacman
			return direction_to_pacman(x, y);
			break;
		case 1:
		case 3:
			// Ghosts 1 and 3 will always try to keep moving in their current
			// direction if possible
			if(dirn_options & (1<<curdirn)) {
				// Current direction is valid - just keep going
				return curdirn;
			} else {
				// Can't move in current direction - try right angles
				int8_t new_dirn = (curdirn + ghostnum)%4;
				if(dirn_options & (1 << new_dirn)) {
					return new_dirn;
				} else {
					// Try the other direction at right angles
					new_dirn = (new_dirn + 2)%4;
					if(dirn_options & (1 << new_dirn)) {
						return new_dirn;
					} else {
						// Neither of the right angles directions worked
						// - just go back in the opposite direction 
						return (curdirn + 2)%4;
					}
				}
			}
			break;	
		case 2:
			// Ghost 2 will try to move in the same direction as the pacman is moving
			if(dirn_options & (1 << pacman_direction)) {
				// That direction is one of the valid options
				return pacman_direction;
			} else {
				// Otherwise, start from a random direction and try each in turn
				int8_t first_direction_to_check = random()%4;
				for(int8_t i = 0; i < 4; i++) {
					int8_t direction_to_check = (first_direction_to_check + i)%4;
					if(dirn_options & (1 << direction_to_check)) {
						return direction_to_check;
					
					}
				}
				// Should never get here because one of the directions should be valid
				// - just indicate that we can't move
				return -1;
			}
	}
	// Should never get here either - just indicate that we can't move
	return -1;	
}

uint8_t game_paused_status(void) {
	return is_game_paused;
}

void change_game_paused(void) {
	is_game_paused = ~is_game_paused;
}

// draw_initial_game_field()
static void draw_initial_game_field(void) {
	clear_terminal();
	normal_display_mode();
	hide_cursor();
	move_cursor(1,1);	// Start at top left
	uint16_t wall_array_index = 0;  // row_number * 31 + column_number, i.e. 31*x+y
	for(uint8_t y = 0; y < FIELD_HEIGHT; y++) {
		for(uint8_t x = 0; x < FIELD_WIDTH; x++) {
			char wall_character = pgm_read_byte(&init_game_field[wall_array_index]);
			switch(wall_character) {
				case '-':	printf("%s", LINE_HORIZONTAL); break;
				case '|':	printf("%s", LINE_VERTICAL); break;
				case 'F':	printf("%s", LINE_DOWN_AND_RIGHT); break;
				case '7':	printf("%s", LINE_DOWN_AND_LEFT); break;
				case 'L':	printf("%s", LINE_UP_AND_RIGHT); break;
				case 'J':	printf("%s", LINE_UP_AND_LEFT); break;
				case '>':	printf("%s", LINE_VERTICAL_AND_RIGHT); break;
				case '<':	printf("%s", LINE_VERTICAL_AND_LEFT); break;
				case '^':	printf("%s", LINE_HORIZONTAL_AND_UP); break;
				case 'v':	printf("%s", LINE_HORIZONTAL_AND_DOWN); break;
				case '+':	printf("%s", LINE_VERTICAL_AND_HORIZONTAL); break;
				case ' ':	printf(" "); break;
				case 'P':	
					set_display_attribute(FG_GREEN);
					printf("O");
					set_display_attribute(FG_WHITE);
					break;
				case '.':	printf("."); break;	// pac-dot
				default:	printf("x"); break;	// shouldn't happen but we show an x in case it does
			}
			wall_array_index++;
		}
		printf("\n");
	}
}

static void initialise_pacdots(void) {
	num_pacdots = 0;
	uint16_t wall_array_index = 0;  // row_number * 31 + column_number, i.e. 31*x+y
	for(uint8_t y = 0; y < FIELD_HEIGHT; y++) {
		pacdots[y] = 0;
		for(uint8_t x = 0; x < FIELD_WIDTH; x++) {
			char wall_character = pgm_read_byte(&init_game_field[wall_array_index]);
			if(wall_character == '.' || wall_character == 'P') {
				pacdots[y] |= (1UL<<x);
				num_pacdots++;
			}
			wall_array_index++;
		}
	}	
}

// Erase the pixel at the given location - presumably because the 
// ghost or the pac-man has moved out of this space. If there is 
// still a pac-dot at this space, we output a dot, otherwise we
// output a space. It is assumed that we are in normal video mode.
static void erase_pixel_at(uint8_t x, uint8_t y) {
	move_cursor(x+1, y+1);
	
	if (is_power_pellet_at(x, y)) {
		set_display_attribute(FG_GREEN);
		printf("O");
	} else if(is_pacdot_at(x,y)) {
		normal_display_mode();
		printf(".");
 	} else {
		printf(" ");
	 }
}

// We draw the pac-man at the given location. The character used
// to draw the pac-man is based on the direction it is currently
// facing.
static void draw_pacman_at(uint8_t x, uint8_t y) {
	move_cursor(x+1,y+1);
	set_display_attribute(PACMAN_COLOUR);
	printf("%s", pacman_characters[pacman_direction]);
	normal_display_mode();
}

// ghostnum is assumed to be in the range 0..NUM_GHOSTS-1
// x and y values are assumed to be valid
static void draw_ghost_at(uint8_t ghostnum, uint8_t x, uint8_t y) {
	move_cursor(x+1,y+1);
	// change the background colour to the colour of the given ghost
	set_display_attribute(ghost_colours[ghostnum]);
	// If there is a pac-dot at this location we output a "." otherwise
	// we output a space (which will be shown as a block in reverse video)
	if (is_power_pellet_at(x,y)) {
		set_display_attribute(FG_BLACK);
		printf("O");
	} else if(is_pacdot_at(x,y)) {
		printf(".");
	} else {
		printf(" ");
	}
	// Return to normal display mode to ensure we don't use this
	// background colour for any other printing
	normal_display_mode();
}

static void draw_power_pellet_ghost_at(uint8_t ghostnum, uint8_t x, uint8_t y) {
		move_cursor(x+1,y+1);
		// change the background colour to the colour of the given ghost
		set_display_attribute(BG_BLUE);
		// If there is a pac-dot at this location we output a "." otherwise
		// we output a space (which will be shown as a block in reverse video)
		if(is_pacdot_at(x,y)) {
			printf(".");
		} else {
			printf(" ");
		}
		// Return to normal display mode to ensure we don't use this
		// background colour for any other printing
		normal_display_mode();
}

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
// Public Functions
void initialise_game_level(void) {
	draw_initial_game_field();
	initialise_pacdots();
	pacman_x = INIT_PACMAN_X;
	pacman_y = INIT_PACMAN_Y;
	pacman_direction = INIT_PACMAN_DIRN;
	draw_pacman_at(pacman_x, pacman_y);
	for(int8_t i = 0; i < NUM_GHOSTS; i++) {
		ghost_x[i] = GHOST_HOME_X_LEFT + 2*i;
		ghost_y[i] = GHOST_HOME_Y;
		ghost_direction[i] = INIT_GHOST_DIRN;
		draw_ghost_at(i, ghost_x[i], ghost_y[i]);
	}
	
	// Display Initial Scores/Remaining pacdots
	move_cursor(33, 1);
	printf("%s", "Remaining number of pac-dots: ");
	printf("%d", num_pacdots);
	move_cursor(33, 2);
	printf("%s", "Lives: ");
	printf("%d", 3);
	move_cursor(33, 3);
	printf("%10s", "Score");
	move_cursor(33, 4);
	printf("%10ld", get_score());
	move_cursor(33, 5);
	printf("%10s", "High Score");
	move_cursor(33, 6);
	printf("%10ld", get_high_score());
	move_cursor(33, 7);
	if (signature_check()) {
		printf("%s", "Saved Game: Yes");
	} else {
		printf("%s", "Saved Game: No");
	}
}

void initialise_game(void) {
	initialise_game_level();
	game_running = 1;
}

void kill_ghost(uint8_t cell_contents) {
	
	// Erase all pixel
	erase_pixel_at(pacman_x, pacman_y);
	
	// Decrement the amount of ghosts that are alive and redraw pacman at the spot
	alive_pellet_ghosts -= 1;
	draw_pacman_at(ghost_x[cell_contents], ghost_y[cell_contents]);
	
	// Change the ghosts position to its home and redraw them there
	ghost_x[cell_contents] = GHOST_HOME_X_LEFT + 2*cell_contents;
	ghost_y[cell_contents] = GHOST_HOME_Y;
	draw_power_pellet_ghost_at(cell_contents, ghost_x[cell_contents], ghost_y[cell_contents]);
	pellet_ghosts[cell_contents] = 0;
	
	// Update score
	uint32_t ghost_score;
	
	if (last_ghost_score == 0) {
		ghost_score = 200;
		last_ghost_score = 200;
	} else {
		ghost_score = last_ghost_score * 2;
		last_ghost_score = ghost_score;
	}
	
	add_to_score(ghost_score);
	move_cursor(33, 4);
	printf("%10ld", get_score());

	if (get_score() > get_high_score()) {
		set_high_score(get_score());
		move_cursor(33, 6);
		printf("%10ld", get_high_score());
	}
	
	// Play sound
	set_prescalar(3);
}

void reset_last_ghost_score(void) {
	last_ghost_score = 0;
}

int8_t move_pacman(void) {
	if(!game_running) {
		// Game is over - do nothing
		return 0;
	}
	
	if (pacman_x == 0 && pacman_y == 15 && pacman_direction == DIRN_LEFT) {
		erase_pixel_at(pacman_x, pacman_y);
		pacman_x = 30;
		draw_pacman_at(pacman_x, pacman_y);
	} else if (pacman_x == 30 && pacman_y == 15 && pacman_direction == DIRN_RIGHT) {
		erase_pixel_at(pacman_x, pacman_y);
		pacman_x = 0;
		draw_pacman_at(pacman_x, pacman_y);
	}
	// If the pac-man is about to exit through the end of a passage-way
	// then wrap its location around to the other side of the game field
	// YOUR CODE HERE - you may need to alter the code below also
	// Work out what is in the direction we want to move
	int8_t cell_contents = what_is_in_dirn(pacman_x, pacman_y, pacman_direction);
	if(cell_contents == CELL_IS_WALL) {
		return 0;	// We can't move - wall is straight ahead
	}
	// We can move - erase the pac-man in the current location
	erase_pixel_at(pacman_x, pacman_y);
	// Update the pac-man location
	if(pacman_direction == DIRN_LEFT) {
		pacman_x--;
	} else if(pacman_direction == DIRN_RIGHT) {
		pacman_x++;
	} else if(pacman_direction == DIRN_UP) {
		pacman_y--;
	} else {
		pacman_y++;
	}
	
	if (cell_contents >= 0 && power_pellet_eaten == 0 && get_lives() - 1 > 0) {
		play_again();
	} else if (cell_contents >= 0 && power_pellet_eaten == 0) {
		// We've encountered a ghost - draw both at the location
		// Set the background colour to that of the ghost
		// before we print out the pac-man
		// Note that the variable cell_contents contains the ghost number
		set_display_attribute(ghost_colours[cell_contents]);
		draw_pacman_at(pacman_x, pacman_y);
		// Game is over 
		game_running = 0;
	} else if (cell_contents >= 0 && power_pellet_eaten == 1) {
		kill_ghost(cell_contents);
	} else {
		if(cell_contents == CELL_CONTAINS_PACDOT) {
			eat_pacdot(10);
			sound_enable_time = get_current_time();
		} else if(cell_contents == CELL_CONTAINS_POWER_PELLET) {
			eat_pacdot(50);
			last_ghost_score = 0;
			power_pellet_eaten = 1;
			power_pellet_eaten_time = get_current_time();
			reset_dead_ghosts();
			set_alive_pellet_ghosts();
			set_prescalar(1);
		}
		draw_pacman_at(pacman_x, pacman_y);
	}
	return 1;
}

int8_t change_pacman_direction(int8_t direction) {
	if(!game_running) {
		// Game is over - do nothing
		return 0;
	}
	// Work out what is in the direction we want to move
	int8_t cell_contents = what_is_in_dirn(pacman_x, pacman_y, direction);
	if(cell_contents == CELL_IS_WALL) {
		// Can't move
		return 0;
	} else {
		pacman_direction = direction;
		// Redraw the pacman so it is facing in the right direction
		draw_pacman_at(pacman_x, pacman_y);
		return 1;
	}
}

void play_again(void) {
	
	decrease_lives();
	
	for (uint8_t i = 0; i < 4; i++) {
		erase_pixel_at(ghost_x[i], ghost_y[i]);
		ghost_x[i] = GHOST_HOME_X_LEFT + 2*i;
		ghost_y[i] = GHOST_HOME_Y;
		draw_ghost_at(i, ghost_x[i], ghost_y[i]);
	}
	
	move_cursor(33, 2);
	printf("%s", "Lives: ");
	printf("%d", get_lives());
	
	if (get_lives() == 2) {
		PORTA = (0 << PORTA7) | (1 << PORTA6) | (1 << PORTA5);
		} else if (get_lives() == 1) {
		PORTA = (0 << PORTA7) | (0 << PORTA6) | (1 << PORTA5);
	}
	
	erase_pixel_at(pacman_x, pacman_y);
	pacman_x = INIT_PACMAN_X;
	pacman_y = INIT_PACMAN_Y;
	draw_pacman_at(pacman_x, pacman_y);
}

void move_ghost(int8_t ghostnum) {
	if(!game_running) {
		// Game is over - do nothing
		return;
	}
	int8_t dirn_to_move = determine_ghost_direction_to_move(ghostnum);
	if(dirn_to_move < 0) {
		// Ghost can't move (e.g. boxed in) - do nothing
		return;
	}
	
	// Erase the ghost from the current location
	erase_pixel_at(ghost_x[ghostnum], ghost_y[ghostnum]);
	
	// Update the ghost's direction (it's possible this may be the same value)
	ghost_direction[ghostnum] = dirn_to_move;
	// Update the ghost's location
	switch(dirn_to_move) {
		case DIRN_LEFT:
			ghost_x[ghostnum]--;
			break;
		case DIRN_RIGHT:
			ghost_x[ghostnum]++;
			break;
		case DIRN_UP:
			ghost_y[ghostnum]--;
			break;
		case DIRN_DOWN:
			ghost_y[ghostnum]++;
			break;
	}
	
	// Check if the pac-man is at this ghost location. 
	if (is_pacman_at(ghost_x[ghostnum], ghost_y[ghostnum]) && power_pellet_eaten == 0 && get_lives() - 1 > 0) {
		play_again();
	} else if (is_pacman_at(ghost_x[ghostnum], ghost_y[ghostnum]) && power_pellet_eaten == 0) {
		// Ghost has just moved into the pac-man. Game is over
		game_running = 0;
		// We draw the background colour for the
		// ghost and output the pac-man over the top of it.
		set_display_attribute(ghost_colours[ghostnum]);
		draw_pacman_at(ghost_x[ghostnum], ghost_y[ghostnum]);
	} else if (is_pacman_at(ghost_x[ghostnum], ghost_y[ghostnum]) && power_pellet_eaten == 1) {
		kill_ghost(ghostnum);
	} else if (power_pellet_eaten == 1) {
		draw_power_pellet_ghost_at(ghostnum, ghost_x[ghostnum], ghost_y[ghostnum]);
	} else {
		draw_ghost_at(ghostnum, ghost_x[ghostnum], ghost_y[ghostnum]);
	}
	normal_display_mode();
}

int8_t is_game_over(void) {
	return !game_running;
}

int8_t is_level_complete(void) {
	return (num_pacdots == 0);
}

int8_t get_power_pellet_eaten(void) {
	return power_pellet_eaten;
}

void reset_power_pellet_eaten(void) {
	power_pellet_eaten = 0;
}

uint32_t get_power_pellet_time(void) {
	return power_pellet_eaten_time;
}

uint8_t get_dead_ghost(uint8_t value) {
	return pellet_ghosts[value];
}

void reset_dead_ghosts(void) {
	for (int8_t i=0; i<4; i++) {
		pellet_ghosts[i] = 1;
	}
}

void set_alive_pellet_ghosts(void) {
	alive_pellet_ghosts = 4;
}

uint32_t get_sound_time(void) {
	return sound_enable_time;
}

uint16_t get_num_pacdots(void) {
	return num_pacdots;
}

// EEPROM functions

void save_game(void) {
		
	uint16_t save_score = get_score();
	uint16_t save_high_score = get_high_score();
	uint32_t save_time;
	
	move_cursor(33, 7);
	printf("%s", "Saved Game: Yes");
	
	if (game_paused_status()) {
		save_time = get_paused_time();
	} else {
		save_time = get_current_time();
	}
	uint8_t save_lives = get_lives();
	
	// Save Signature
	eeprom_update_block((const void*)&signature, (void*)SIGNATURE, SIGNATURE_SIZE); 
	
	// Save Scores
	eeprom_update_block((const void*)&save_score, (void*)SCORE, SCORE_SIZE); 
	eeprom_update_block((const void*)&save_high_score, (void*)HIGH_SCORE, HIGH_SCORE_SIZE);
	
	// Save Pacdots
	eeprom_update_block((const void*)&num_pacdots, (void*)NUM_PACDOTS, NUM_PACDOTS_SIZE);
	eeprom_update_block((const void*)&pacdots, (void*)PACDOTS, PACDOTS_SIZE);
	
	// Save Power Pellet Info
	eeprom_update_block((const void*)&power_pellet_eaten, (void*)POWER_PELLET_STATUS, POWER_PELLET_STATUS_SIZE);
	eeprom_update_block((const void*)&power_pellet_eaten_time, (void*)POWER_PELLET_EATEN_TIME, POWER_PELLET_EATEN_TIME_SIZE);
	eeprom_update_block((const void*)&pellet_ghosts, (void*)POWER_PELLET_GHOSTS, POWER_PELLET_GHOSTS_SIZE);
	eeprom_update_block((const void*)&alive_pellet_ghosts, (void*)ALIVE_GHOSTS, ALIVE_GHOSTS_SIZE);
	eeprom_update_block((const void*)&last_ghost_score, (void*)LAST_GHOST_SCORE, LAST_GHOST_SCORE_SIZE);
	
	// Save Time
	eeprom_update_block((const void*)&save_time, (void*)TIME, TIME_SIZE);
	
	// Save Lives
	eeprom_update_block((const void*)&save_lives, (void*)LIVES, LIVES_SIZE);
	
	// Save Pacman Info
	eeprom_update_block((const void*)&pacman_x, (void*)PACMAN_X, PACMAN_X_SIZE);
	eeprom_update_block((const void*)&pacman_y, (void*)PACMAN_Y, PACMAN_Y_SIZE);
	eeprom_update_block((const void*)&pacman_direction, (void*)PACMAN_DIRECTION, PACMAN_DIRECTION_SIZE);
	
	// Save Ghosts
	eeprom_update_block((const void*)&ghost_x, (void*)GHOSTS_X, GHOSTS_X_SIZE);
	eeprom_update_block((const void*)&ghost_y, (void*)GHOSTS_Y, GHOSTS_Y_SIZE);
	eeprom_update_block((const void*)&ghost_direction, (void*)GHOSTS_DIRN, GHOSTS_DIRN_SIZE);
	
	move_cursor(40, 40);
	printf("%s", "Game Saved.");
}

uint8_t signature_check(void) {
	char eeprom_signature[8];
	int x;
	
	eeprom_read_block((void*)&eeprom_signature, (const void*)SIGNATURE, SIGNATURE_SIZE);
	for (x = 0; x < 8; x++) {
		if (eeprom_signature[x] != signature[x]) {
			return 0;
		}
	}
	
	return 1;
}

void load_game_state(void) {

	// Lives
	set_lives(load_lives[0]);
	move_cursor(33, 2);
	printf("%s", "Lives: ");
	printf("%d", get_lives());
	
	// Score
	set_score(load_score[0]);
	set_high_score(load_highscore[0]);
	move_cursor(33, 4);
	printf("%s", "        ");
	move_cursor(33, 4);
	printf("%10ld", get_score());
	move_cursor(33, 6);
	printf("%s", "        ");
	move_cursor(33, 6);
	printf("%10ld", get_high_score());
		
	// Num_pacdot
	uint8_t p;
	uint8_t board_width;
	uint8_t board_height;
	uint32_t wall_array_index = 0;
	num_pacdots = load_num_pacdots[0];
	move_cursor(33, 1);
	printf("%s", "Remaining number of pac-dots: ");
	printf("%d", num_pacdots);
	
	// Pacdots
	for (p = 0; p < FIELD_HEIGHT; p++) {
		pacdots[p] = load_pacdots[p];
	}

	for (board_height = 0; board_height < FIELD_HEIGHT; board_height++) {
		for (board_width = 0; board_width < FIELD_WIDTH; board_width++) {
			erase_pixel_at(board_height, board_width);
		}
	}
	
	normal_display_mode();
	hide_cursor();
	move_cursor(1,1);
	for(uint8_t y = 0; y < FIELD_HEIGHT; y++) {
		for(uint8_t x = 0; x < FIELD_WIDTH; x++) {
			char wall_character = pgm_read_byte(&init_game_field[wall_array_index]);
			switch(wall_character) {
				case '-':	printf("%s", LINE_HORIZONTAL); break;
				case '|':	printf("%s", LINE_VERTICAL); break;
				case 'F':	printf("%s", LINE_DOWN_AND_RIGHT); break;
				case '7':	printf("%s", LINE_DOWN_AND_LEFT); break;
				case 'L':	printf("%s", LINE_UP_AND_RIGHT); break;
				case 'J':	printf("%s", LINE_UP_AND_LEFT); break;
				case '>':	printf("%s", LINE_VERTICAL_AND_RIGHT); break;
				case '<':	printf("%s", LINE_VERTICAL_AND_LEFT); break;
				case '^':	printf("%s", LINE_HORIZONTAL_AND_UP); break;
				case 'v':	printf("%s", LINE_HORIZONTAL_AND_DOWN); break;
				case '+':	printf("%s", LINE_VERTICAL_AND_HORIZONTAL); break;
				case ' ':	printf(" "); break;
				default:	move_cursor_right();
			}
			wall_array_index++;
		}
		printf("\n");
	}
	
	// Pacman
	erase_pixel_at(pacman_x, pacman_y);
	pacman_x = load_pacman_x[0];
	pacman_y = load_pacman_y[0];
	pacman_direction = load_pacman_dirn[0];
	draw_pacman_at(pacman_x, pacman_y);
	
	// Ghosts
	uint8_t g;
	
	for (g = 0; g < NUM_GHOSTS; g++) {
		erase_pixel_at(ghost_x[g], ghost_y[g]);
		ghost_x[g] = load_ghost_x[g];
		ghost_y[g] = load_ghost_y[g];
		
		if (power_pellet_eaten) {
			draw_power_pellet_ghost_at(g, ghost_x[g], ghost_y[g]);
			} else {
			draw_ghost_at(g, ghost_x[g], ghost_y[g]);
		}
	}
	
	// Time
	set_time(load_time[0]);
	
	// Power Pellet
	if (load_pellet_status[0] == 1) {
		uint8_t i;
		for (i = 0; i < NUM_GHOSTS; i++) {
			pellet_ghosts[i] = load_pellet_ghosts[i];
		}
		last_ghost_score = load_last_ghost_score[0];
		power_pellet_eaten = load_pellet_status[0];
		alive_pellet_ghosts = load_alive_pellet_ghosts[0];
		power_pellet_eaten_time = load_eaten_pellet_time[0];
	}
}

void load_game(void) {
	
	// Load Scores
	eeprom_read_block((void*)&load_score, (const void*)SCORE, SCORE_SIZE);
	eeprom_read_block((void*)&load_highscore, (const void*)HIGH_SCORE, HIGH_SCORE_SIZE);
	
	// Load Pacdots
	eeprom_read_block((void*)&load_num_pacdots, (const void*)NUM_PACDOTS, NUM_PACDOTS_SIZE);
	eeprom_read_block((void*)&load_pacdots, (const void*)PACDOTS, PACDOTS_SIZE);
	
	// Load Power Pellet Info
	eeprom_read_block((void*)&load_pellet_status, (const void*)POWER_PELLET_STATUS, POWER_PELLET_STATUS_SIZE);
	eeprom_read_block((void*)&load_eaten_pellet_time, (const void*)POWER_PELLET_EATEN_TIME, POWER_PELLET_EATEN_TIME_SIZE);
	eeprom_read_block((void*)&load_pellet_ghosts, (const void*)POWER_PELLET_GHOSTS, POWER_PELLET_GHOSTS_SIZE);
	eeprom_read_block((void*)&load_alive_pellet_ghosts, (const void*)ALIVE_GHOSTS, ALIVE_GHOSTS_SIZE);
	eeprom_read_block((void*)&load_last_ghost_score, (const void*)LAST_GHOST_SCORE, LAST_GHOST_SCORE_SIZE);
	
	// Load Time
	eeprom_read_block((void*)&load_time, (const void*)TIME, TIME_SIZE);
	
	// Load Lives
	eeprom_read_block((void*)&load_lives, (const void*)LIVES, LIVES_SIZE);
	
	// Load Pacman Info
	eeprom_read_block((void*)&load_pacman_x, (const void*)PACMAN_X, PACMAN_X_SIZE);
	eeprom_read_block((void*)&load_pacman_y, (const void*)PACMAN_Y, PACMAN_Y_SIZE);
	eeprom_read_block((void*)&load_pacman_dirn, (const void*)PACMAN_DIRECTION, PACMAN_DIRECTION_SIZE);
	
	// Load Ghosts
	eeprom_read_block((void*)&load_ghost_x, (const void*)GHOSTS_X, GHOSTS_X_SIZE);
	eeprom_read_block((void*)&load_ghost_y, (const void*)GHOSTS_Y, GHOSTS_Y_SIZE);
	eeprom_read_block((void*)&load_ghost_dirn, (const void*)GHOSTS_DIRN, GHOSTS_DIRN_SIZE);
	
	load_game_state();
}

void draw_ledmatrix_game(void) {
	int8_t x;
	int8_t y;
	int8_t matrix_x = -1;
	uint8_t matrix_y = 8;
	uint8_t display_count = 0;
	
	for (x = pacman_x - 7; x < pacman_x + 9; x++) {
		matrix_x++;
		for (y = pacman_y - 4; y < pacman_y + 4; y++) {
			matrix_y--;					
			if (x < 0 || y < 0 || y > FIELD_HEIGHT - 1 || x > FIELD_WIDTH - 1) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_BLACK);
			} else if (what_is_at(x, y) == CELL_IS_WALL) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_RED);
			} else if (what_is_at(x, y) == CELL_CONTAINS_PACMAN) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_YELLOW);
			} else if (what_is_at(x, y) == CELL_CONTAINS_POWER_PELLET) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_ORANGE);
			} else if (what_is_at(x, y) == CELL_CONTAINS_PACDOT) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_PALE_RED);
			} else if (what_is_at(x, y) == CELL_IS_WALL) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_RED);
			} else if (what_is_at(x, y) >= 0) {
				if (power_pellet_eaten == 0) {
					ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_GREEN);
				} else {
					ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_PALE_GREEN);
				}
			} else if (what_is_at(x, y) == CELL_EMPTY) {
				ledmatrix_update_pixel(matrix_x, matrix_y, COLOUR_BLACK);
			}
			display_count++;
			if (display_count == 8) {
				display_count = 0;
				matrix_y = 8;
			}
		}
	}
}

