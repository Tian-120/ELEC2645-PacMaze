/**
 * @file Paddle.c
 * @brief Paddle object implementation for Pong game
 */

#include "Paddle.h"
#include "LCD.h"

// Screen dimensions (ST7789V2 display)
#define SCREEN_HEIGHT 240

void Paddle_Init(Paddle_t* paddle, int16_t x, int16_t y, int16_t width, int16_t height, int16_t speed) {
    paddle->x = x;
    paddle->y = y;
    paddle->width = width;
    paddle->height = height;
    paddle->speed = speed;
    paddle->score = 0;
}

void Paddle_Update(Paddle_t* paddle, UserInput input) {
    // Move paddle based on joystick direction (N/S for up/down)
    if (input.direction == N || input.direction == NE || input.direction == NW) {
        // Move up
        paddle->y -= paddle->speed;
    } else if (input.direction == S || input.direction == SE || input.direction == SW) {
        // Move down
        paddle->y += paddle->speed;
    }
    
    // IMPORTANT: Apply boundary constraints AFTER movement
    // These ensure the paddle stays within screen bounds at all times
    
    // TOP BOUNDARY: Paddle's top edge (y) must be >= 0
    if (paddle->y < 0) {
        paddle->y = 0;
    }
    
    // BOTTOM BOUNDARY: Paddle's bottom edge (y + height) must be <= SCREEN_HEIGHT
    // This ensures the paddle's BOTTOM doesn't go past the bottom of the screen
    int16_t paddle_bottom = paddle->y + paddle->height;
    if (paddle_bottom > SCREEN_HEIGHT) {
        paddle->y = SCREEN_HEIGHT - paddle->height;
    }
}

void Paddle_Draw(Paddle_t* paddle) {
    // Draw paddle as a filled rectangle
    // Color: white (15 in 4-bit color), filled (1)
    LCD_Draw_Rect(
        paddle->x,
        paddle->y,
        paddle->width,
        paddle->height,
        15,        // white color
        1          // filled
    );
}

AABB Paddle_GetAABB(Paddle_t* paddle) {
    AABB box;
    box.x = paddle->x;
    box.y = paddle->y;
    box.width = paddle->width;
    box.height = paddle->height;
    return box;
}

void Paddle_AddScore(Paddle_t* paddle) {
    paddle->score++;
}

uint16_t Paddle_GetScore(Paddle_t* paddle) {
    return paddle->score;
}

Position2D Paddle_GetPos(Paddle_t* paddle) {
    Position2D pos;
    pos.x = paddle->x;
    pos.y = paddle->y;
    return pos;
}

int16_t Paddle_GetHeight(Paddle_t* paddle) {
    return paddle->height;
}

int16_t Paddle_GetWidth(Paddle_t* paddle) {
    return paddle->width;
}
