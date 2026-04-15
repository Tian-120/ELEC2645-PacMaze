#ifndef PACMAZE_H
#define PACMAZE_H

#include <stdint.h>
#include "LCD.h"
#include "Joystick.h"
#include "Utils.h"
#include "main.h"
#include "PacMazeMap.h"

typedef enum {
    MODE_PELLET = 0,
    MODE_TIME,
    MODE_HARD
} GameMode;

typedef enum {
    STATE_MENU = 0,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_WIN,
    STATE_GAMEOVER
} GameState;

typedef enum {
    SFX_NONE = 0,
    SFX_PELLET,
    SFX_WIN,
    SFX_LOSE
} SoundEffect;

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_COUNTDOWN,
    LED_MODE_WIN_ON
} LedMode;

typedef struct {
    int x;
    int y;
    uint8_t active;
} Pellet_t;

typedef struct {
    int x;
    int y;
    int score;
    int lives;
    int time_left;

    GameMode mode;
    GameState state;
    uint8_t menu_index;

    uint8_t time_enabled;
    uint8_t ghost_enabled;

    uint8_t btn_last;
    Direction last_direction;

    uint32_t last_move_tick;
    uint32_t last_second_tick;

    uint32_t last_ghost_tick;
    int ghost_x;
    int ghost_y;

    Pellet_t pellets[PELLET_COUNT];
    int pellets_left;

    SoundEffect sfx;
    uint8_t sfx_active;
    uint8_t sfx_step;
    uint32_t sfx_next_tick;

    LedMode led_mode;
    uint8_t led_state;
    uint32_t led_next_tick;

    GameState prev_state;

} PacMaze_t;

void PacMaze_Init(PacMaze_t *game);
void PacMaze_Update(PacMaze_t *game, UserInput input);
void PacMaze_Render(PacMaze_t *game, ST7789V2_cfg_t *lcd_cfg);
void Game_Buzzer_Play(uint16_t freq);
void Game_Buzzer_Stop(void);
void Game_LED_On(void);
void Game_LED_Off(void);

#endif