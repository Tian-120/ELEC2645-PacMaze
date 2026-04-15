#include "PacMaze.h"
#include "PacMazeMap.h"
#include <stdio.h>
#include <stdlib.h>

static void start_sound(PacMaze_t *game, SoundEffect effect)
{
    if (effect == SFX_PELLET && game->sfx_active &&
        (game->sfx == SFX_WIN || game->sfx == SFX_LOSE)) {
        return;
    }

    game->sfx = effect;
    game->sfx_active = 1;
    game->sfx_step = 0;
    game->sfx_next_tick = 0;
}

static void set_led_mode(PacMaze_t *game, LedMode mode)
{
    game->led_mode = mode;
    game->led_state = 0;
    game->led_next_tick = 0;

    if (mode == LED_MODE_OFF) {
        Game_LED_Off();
    }
    else if (mode == LED_MODE_WIN_ON) {
        Game_LED_On();
        game->led_state = 1;
    }
}

static void update_sound(PacMaze_t *game)
{
    uint32_t now = HAL_GetTick();

    if (!game->sfx_active) {
        return;
    }

    if (now < game->sfx_next_tick) {
        return;
    }

    switch (game->sfx) {
        case SFX_PELLET:
            if (game->sfx_step == 0) {
                Game_Buzzer_Play(1400);
                game->sfx_next_tick = now + 35;
                game->sfx_step = 1;
            } else {
                Game_Buzzer_Stop();
                game->sfx_active = 0;
                game->sfx = SFX_NONE;
            }
            break;

        case SFX_WIN:
            switch (game->sfx_step) {
                case 0:
                    Game_Buzzer_Play(800);
                    game->sfx_next_tick = now + 90;
                    game->sfx_step = 1;
                    break;
                case 1:
                    Game_Buzzer_Stop();
                    game->sfx_next_tick = now + 25;
                    game->sfx_step = 2;
                    break;
                case 2:
                    Game_Buzzer_Play(1200);
                    game->sfx_next_tick = now + 90;
                    game->sfx_step = 3;
                    break;
                case 3:
                    Game_Buzzer_Stop();
                    game->sfx_next_tick = now + 25;
                    game->sfx_step = 4;
                    break;
                case 4:
                    Game_Buzzer_Play(1700);
                    game->sfx_next_tick = now + 120;
                    game->sfx_step = 5;
                    break;
                default:
                    Game_Buzzer_Stop();
                    game->sfx_active = 0;
                    game->sfx = SFX_NONE;
                    break;
            }
            break;

        case SFX_LOSE:
            switch (game->sfx_step) {
                case 0:
                    Game_Buzzer_Play(1500);
                    game->sfx_next_tick = now + 90;
                    game->sfx_step = 1;
                    break;
                case 1:
                    Game_Buzzer_Stop();
                    game->sfx_next_tick = now + 25;
                    game->sfx_step = 2;
                    break;
                case 2:
                    Game_Buzzer_Play(900);
                    game->sfx_next_tick = now + 90;
                    game->sfx_step = 3;
                    break;
                case 3:
                    Game_Buzzer_Stop();
                    game->sfx_next_tick = now + 25;
                    game->sfx_step = 4;
                    break;
                case 4:
                    Game_Buzzer_Play(500);
                    game->sfx_next_tick = now + 120;
                    game->sfx_step = 5;
                    break;
                default:
                    Game_Buzzer_Stop();
                    game->sfx_active = 0;
                    game->sfx = SFX_NONE;
                    break;
            }
            break;

        default:
            Game_Buzzer_Stop();
            game->sfx_active = 0;
            game->sfx = SFX_NONE;
            break;
    }
}

static void update_led(PacMaze_t *game)
{
    uint32_t now = HAL_GetTick();

    if (game->led_mode == LED_MODE_WIN_ON) {
        Game_LED_On();
        return;
    }

    if (game->led_mode == LED_MODE_COUNTDOWN) {
        if (now >= game->led_next_tick) {
            game->led_next_tick = now + 200;
            game->led_state = !game->led_state;

            if (game->led_state) {
                Game_LED_On();
            } else {
                Game_LED_Off();
            }
        }
        return;
    }

    Game_LED_Off();
}

static void handle_state_effects(PacMaze_t *game)
{
    if (game->state != game->prev_state) {
        if (game->state == STATE_WIN) {
            start_sound(game, SFX_WIN);
            set_led_mode(game, LED_MODE_WIN_ON);
        } else if (game->state == STATE_GAMEOVER) {
            start_sound(game, SFX_LOSE);
            set_led_mode(game, LED_MODE_OFF);
        } else if (game->state == STATE_MENU) {
            Game_Buzzer_Stop();
            set_led_mode(game, LED_MODE_OFF);
        }
        game->prev_state = game->state;
    }

    if (game->state == STATE_RUNNING) {
        if (game->time_enabled && game->time_left > 0 && game->time_left <= 10) {
            if (game->led_mode != LED_MODE_COUNTDOWN) {
                set_led_mode(game, LED_MODE_COUNTDOWN);
            }
        } else if (game->led_mode != LED_MODE_OFF) {
            set_led_mode(game, LED_MODE_OFF);
        }
    }
}

#define PLAYER_SIZE 10
#define GHOST_SIZE  10
#define MOVE_STEP   6
#define GHOST_STEP  4
#define HUD_HEIGHT  30
#define PELLET_SIZE 4

static uint8_t read_btn2_pressed(void)
{
    /* Button confirmed working on PD2, active HIGH */
    return (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_SET);
}

static uint8_t dir_is_up(Direction d)
{
    return (d == N || d == NE || d == NW);
}

static uint8_t dir_is_down(Direction d)
{
    return (d == S || d == SE || d == SW);
}

static uint8_t dir_is_left(Direction d)
{
    return (d == W || d == NW || d == SW);
}

static uint8_t dir_is_right(Direction d)
{
    return (d == E || d == NE || d == SE);
}

static uint8_t collides_box_with_wall(int x, int y, int size)
{
    AABB box = {x, y, size, size};

    for (int i = 0; i < WALL_COUNT; i++) {
        AABB wall = walls[i];
        if (AABB_Collides(&box, &wall)) {
            return 1;
        }
    }
    return 0;
}

static uint8_t collides_with_wall(int x, int y)
{
    return collides_box_with_wall(x, y, PLAYER_SIZE);
}

static uint8_t ghost_collides_with_wall(int x, int y)
{
    return collides_box_with_wall(x, y, GHOST_SIZE);
}

static uint8_t pellet_hits_wall(int x, int y)
{
    AABB pellet_box = {x, y, PELLET_SIZE, PELLET_SIZE};

    for (int i = 0; i < WALL_COUNT; i++) {
        AABB wall = walls[i];
        if (AABB_Collides(&pellet_box, &wall)) {
            return 1;
        }
    }
    return 0;
}

static void draw_walls(void)
{
    for (int i = 0; i < WALL_COUNT; i++) {
        LCD_Draw_Rect(walls[i].x, walls[i].y, walls[i].width, walls[i].height, 1, 1);
    }
}

static void draw_pellets(PacMaze_t *game)
{
    for (int i = 0; i < PELLET_COUNT; i++) {
        if (game->pellets[i].active) {
            LCD_Draw_Rect(game->pellets[i].x, game->pellets[i].y, PELLET_SIZE, PELLET_SIZE, 1, 1);
        }
    }
}

static void draw_ghost(PacMaze_t *game)
{
    if (game->ghost_enabled) {
        LCD_Draw_Rect(game->ghost_x, game->ghost_y, GHOST_SIZE, GHOST_SIZE, 1, 0);
        LCD_Draw_Rect(game->ghost_x + 2, game->ghost_y + 2, 2, 2, 1, 1);
        LCD_Draw_Rect(game->ghost_x + 6, game->ghost_y + 2, 2, 2, 1, 1);
    }
}

static void load_pellets(PacMaze_t *game)
{
    game->pellets_left = 0;

    for (int i = 0; i < PELLET_COUNT; i++) {
        int px = 6 + pellet_grid[i].gx * 6 + 1;
        int py = 36 + pellet_grid[i].gy * 6 + 1;

        game->pellets[i].x = px;
        game->pellets[i].y = py;

        if (pellet_hits_wall(px, py)) {
            game->pellets[i].active = 0;
        } else {
            game->pellets[i].active = 1;
            game->pellets_left++;
        }
    }
}

static void reset_ghost(PacMaze_t *game)
{
    game->ghost_x = 204;
    game->ghost_y = 216;
    game->last_ghost_tick = HAL_GetTick();
}

static void start_mode(PacMaze_t *game, GameMode mode)
{
    game->mode = mode;
    game->state = STATE_RUNNING;
    game->menu_index = (uint8_t)mode;

    game->x = 12;
    game->y = 42;
    game->score = 0;
    game->lives = 3;

    switch (mode) {
        case MODE_PELLET:
            game->time_enabled = 0;
            game->ghost_enabled = 0;
            game->time_left = 0;
            break;

        case MODE_TIME:
            game->time_enabled = 1;
            game->ghost_enabled = 0;
            game->time_left = 45;
            break;

        case MODE_HARD:
            game->time_enabled = 1;
            game->ghost_enabled = 1;
            game->time_left = 50;
            break;

        default:
            game->time_enabled = 0;
            game->ghost_enabled = 0;
            game->time_left = 0;
            break;
    }

    game->last_move_tick = HAL_GetTick();
    game->last_second_tick = HAL_GetTick();
    game->last_direction = CENTRE;

    load_pellets(game);
    reset_ghost(game);

        game->sfx = SFX_NONE;
    game->sfx_active = 0;
    game->sfx_step = 0;
    game->sfx_next_tick = 0;

    set_led_mode(game, LED_MODE_OFF);
    game->prev_state = game->state;

    Game_Buzzer_Stop();
}

void PacMaze_Init(PacMaze_t *game)
{
    game->x = 12;
    game->y = 42;
    game->score = 0;
    game->lives = 3;
    game->time_left = 0;

    game->mode = MODE_PELLET;
    game->state = STATE_MENU;
    game->menu_index = 0;

    game->time_enabled = 0;
    game->ghost_enabled = 0;

    game->btn_last = 0;
    game->last_direction = CENTRE;
    game->last_move_tick = HAL_GetTick();
    game->last_second_tick = HAL_GetTick();
    game->last_ghost_tick = HAL_GetTick();

    game->ghost_x = 204;
    game->ghost_y = 216;

    game->sfx = SFX_NONE;
    game->sfx_active = 0;
    game->sfx_step = 0;
    game->sfx_next_tick = 0;

    game->led_mode = LED_MODE_OFF;
    game->led_state = 0;
    game->led_next_tick = 0;

    game->prev_state = game->state;

    game->pellets_left = 0;
    for (int i = 0; i < PELLET_COUNT; i++) {
        game->pellets[i].x = 0;
        game->pellets[i].y = 0;
        game->pellets[i].active = 0;
    }
}

static void try_move_ghost(PacMaze_t *game, int dx, int dy, uint8_t *moved)
{
    int nx = game->ghost_x + dx;
    int ny = game->ghost_y + dy;

    if (!ghost_collides_with_wall(nx, ny)) {
        game->ghost_x = nx;
        game->ghost_y = ny;
        *moved = 1;
    }
}

static void update_ghost(PacMaze_t *game)
{
    if (!game->ghost_enabled) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((now - game->last_ghost_tick) < 120) {
        return;
    }
    game->last_ghost_tick = now;

    int dx = game->x - game->ghost_x;
    int dy = game->y - game->ghost_y;

    uint8_t moved = 0;

    /* Prefer the axis with bigger distance */
    if (abs(dx) >= abs(dy)) {
        if (dx > 0) try_move_ghost(game,  GHOST_STEP, 0, &moved);
        if (!moved && dx < 0) try_move_ghost(game, -GHOST_STEP, 0, &moved);
        if (!moved && dy > 0) try_move_ghost(game, 0,  GHOST_STEP, &moved);
        if (!moved && dy < 0) try_move_ghost(game, 0, -GHOST_STEP, &moved);
    } else {
        if (dy > 0) try_move_ghost(game, 0,  GHOST_STEP, &moved);
        if (!moved && dy < 0) try_move_ghost(game, 0, -GHOST_STEP, &moved);
        if (!moved && dx > 0) try_move_ghost(game,  GHOST_STEP, 0, &moved);
        if (!moved && dx < 0) try_move_ghost(game, -GHOST_STEP, 0, &moved);
    }

    /* Fallbacks if preferred directions are blocked */
    if (!moved) try_move_ghost(game,  GHOST_STEP, 0, &moved);
    if (!moved) try_move_ghost(game, -GHOST_STEP, 0, &moved);
    if (!moved) try_move_ghost(game, 0,  GHOST_STEP, &moved);
    if (!moved) try_move_ghost(game, 0, -GHOST_STEP, &moved);
}

static uint8_t ghost_hits_player(PacMaze_t *game)
{
    if (!game->ghost_enabled) {
        return 0;
    }

    AABB player_box = {game->x, game->y, PLAYER_SIZE, PLAYER_SIZE};
    AABB ghost_box  = {game->ghost_x, game->ghost_y, GHOST_SIZE, GHOST_SIZE};

    return AABB_Collides(&player_box, &ghost_box);
}

void PacMaze_Update(PacMaze_t *game, UserInput input)
{
    uint32_t now = HAL_GetTick();
    uint8_t btn_now = read_btn2_pressed();

    switch (game->state) {

        case STATE_MENU:
            if (dir_is_up(input.direction) && !dir_is_up(game->last_direction)) {
                if (game->menu_index == 0) {
                    game->menu_index = 2;
                } else {
                    game->menu_index--;
                }
            }

            if (dir_is_down(input.direction) && !dir_is_down(game->last_direction)) {
                game->menu_index = (game->menu_index + 1) % 3;
            }

            if (btn_now && !game->btn_last) {
                start_mode(game, (GameMode)game->menu_index);
            }
            break;

        case STATE_RUNNING:
            if (btn_now && !game->btn_last) {
                game->state = STATE_PAUSED;
                break;
            }

            if (game->time_enabled && ((now - game->last_second_tick) >= 1000)) {
                game->last_second_tick = now;
                if (game->time_left > 0) {
                    game->time_left--;
                }
                if (game->time_left <= 0) {
                    game->state = STATE_GAMEOVER;
                    break;
                }
            }

            if ((now - game->last_move_tick) >= 60) {
                game->last_move_tick = now;

                int new_x = game->x;
                int new_y = game->y;

                switch (input.direction) {
                    case N:
                        new_y -= MOVE_STEP;
                        break;
                    case S:
                        new_y += MOVE_STEP;
                        break;
                    case E:
                        new_x += MOVE_STEP;
                        break;
                    case W:
                        new_x -= MOVE_STEP;
                        break;
                    case NE:
                        new_x += MOVE_STEP;
                        new_y -= MOVE_STEP;
                        break;
                    case NW:
                        new_x -= MOVE_STEP;
                        new_y -= MOVE_STEP;
                        break;
                    case SE:
                        new_x += MOVE_STEP;
                        new_y += MOVE_STEP;
                        break;
                    case SW:
                        new_x -= MOVE_STEP;
                        new_y += MOVE_STEP;
                        break;
                    case CENTRE:
                    default:
                        break;
                }

                if (new_x < 0) new_x = 0;
                if (new_y < HUD_HEIGHT) new_y = HUD_HEIGHT;
                if (new_x > 230) new_x = 230;
                if (new_y > 230) new_y = 230;

                if (!collides_with_wall(new_x, new_y)) {
                    game->x = new_x;
                    game->y = new_y;
                }
            }

            {
                AABB player_box = {game->x, game->y, PLAYER_SIZE, PLAYER_SIZE};

                for (int i = 0; i < PELLET_COUNT; i++) {
                    if (game->pellets[i].active) {
                        AABB pellet_box = {
                            game->pellets[i].x,
                            game->pellets[i].y,
                            PELLET_SIZE,
                            PELLET_SIZE
                        };

                        if (AABB_Collides(&player_box, &pellet_box)) {
                            game->pellets[i].active = 0;
                            game->score += 1;
                            game->pellets_left--;
                            start_sound(game, SFX_PELLET);

                            if (game->pellets_left <= 0) {
                                game->state = STATE_WIN;
                            }
                        }
                    }
                }
            }

            update_ghost(game);

            if (ghost_hits_player(game)) {
                game->state = STATE_GAMEOVER;
            }
            break;

        case STATE_PAUSED:
            if (btn_now && !game->btn_last) {
                game->state = STATE_RUNNING;
            }
            break;

        case STATE_WIN:
        case STATE_GAMEOVER:
            if (dir_is_left(input.direction) && !dir_is_left(game->last_direction)) {
                start_mode(game, game->mode);   /* retry */
            }

            if (dir_is_right(input.direction) && !dir_is_right(game->last_direction)) {
                game->state = STATE_MENU;       /* back to menu */
                game->menu_index = (uint8_t)game->mode;
            }

            if (btn_now && !game->btn_last) {
                start_mode(game, game->mode);   /* button = retry */
            }
            break;

        default:
            break;
    }

    game->btn_last = btn_now;
    game->last_direction = input.direction;

    handle_state_effects(game);
    update_sound(game);
    update_led(game);
}

static void draw_center_page(const char *title, const char *line1, const char *line2)
{
    LCD_Fill_Buffer(0);

    LCD_printString(title, 45, 70, 1, 3);

    if (line1 != NULL) {
        LCD_printString(line1, 35, 125, 1, 2);
    }

    if (line2 != NULL) {
        LCD_printString(line2, 35, 155, 1, 2);
    }
}

void PacMaze_Render(PacMaze_t *game, ST7789V2_cfg_t *lcd_cfg)
{
    char buf[32];

    if (game->state == STATE_MENU) {
        LCD_Fill_Buffer(0);

        LCD_printString("PAC-MAZE", 55, 25, 1, 3);

        if (game->menu_index == 0) LCD_printString(">", 25, 80, 1, 2);
        if (game->menu_index == 1) LCD_printString(">", 25, 110, 1, 2);
        if (game->menu_index == 2) LCD_printString(">", 25, 140, 1, 2);

        LCD_printString("Pellet Mode", 45, 80, 1, 2);
        LCD_printString("Time Mode",   45, 110, 1, 2);
        LCD_printString("Hard Mode",   45, 140, 1, 2);

        LCD_printString("Stick: Select", 35, 185, 1, 2);
        LCD_printString("Button: Start", 30, 205, 1, 2);

        LCD_Refresh(lcd_cfg);
        return;
    }

    if (game->state == STATE_PAUSED) {
        draw_center_page("PAUSED", "Button: Resume", NULL);
        LCD_Refresh(lcd_cfg);
        return;
    }

    if (game->state == STATE_WIN) {
        LCD_Fill_Buffer(0);

        LCD_printString("YOU WIN!", 60, 70, 1, 3);
        sprintf(buf, "Score: %d", game->score);
        LCD_printString(buf, 55, 120, 1, 2);
        LCD_printString("Left: Retry", 55, 155, 1, 2);
        LCD_printString("Right: Menu", 50, 180, 1, 2);

        LCD_Refresh(lcd_cfg);
        return;
    }

    if (game->state == STATE_GAMEOVER) {
        LCD_Fill_Buffer(0);

        LCD_printString("GAME OVER", 45, 70, 1, 3);
        sprintf(buf, "Score: %d", game->score);
        LCD_printString(buf, 55, 120, 1, 2);
        LCD_printString("Left: Retry", 55, 155, 1, 2);
        LCD_printString("Right: Menu", 50, 180, 1, 2);

        LCD_Refresh(lcd_cfg);
        return;
    }

    LCD_Fill_Buffer(0);

    LCD_printString("SCORE:", 10, 5, 1, 2);
    LCD_printString("LIVES:", 85, 5, 1, 2);
    LCD_printString("TIME:", 165, 5, 1, 2);

    sprintf(buf, "%d", game->score);
    LCD_printString(buf, 15, 18, 1, 2);

    sprintf(buf, "%d", game->lives);
    LCD_printString(buf, 95, 18, 1, 2);

    sprintf(buf, "%d", game->time_enabled ? game->time_left : 0);
    LCD_printString(buf, 175, 18, 1, 2);

    draw_walls();
    draw_pellets(game);
    draw_ghost(game);

    LCD_Draw_Rect(game->x, game->y, PLAYER_SIZE, PLAYER_SIZE, 1, 1);
    /* mouth cut-out */
    LCD_Draw_Rect(game->x + 6, game->y + 3, 4, 4, 0, 1);

    LCD_Refresh(lcd_cfg);
}