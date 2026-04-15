/**
 * @file PongEngine.c
 * @brief Main game engine implementation for Pong
 * 
 * Handles collision detection using AABB method and game logic.
 */

#include "PongEngine.h"
#include "Buzzer.h"

// Screen dimensions (ST7789V2 display)
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
#define BALL_RESET_OFFSET 20
#define BUZZER_WALL_FREQ_HZ 1200
#define BUZZER_PADDLE_FREQ_HZ 800
#define BUZZER_VOLUME 50
#define BUZZER_BEEP_MS 40

extern Buzzer_cfg_t buzzer_cfg;

static uint32_t buzzer_stop_tick = 0;

/**
 * @brief Play a short buzzer beep for collisions
 *
 * This is kept as a static (internal) helper function because it is an
 * implementation detail of PongEngine, not part of the public API.
 * Callers use PongEngine_Update() and PongEngine_Beep() handles the beeping
 * internally. This practice is called "encapsulation" - we hide complexity
 * from the user and only expose what they need.
 *
 * @param freq_hz Tone frequency in Hz
 */
static void PongEngine_Beep(uint32_t freq_hz)
{
    buzzer_tone(&buzzer_cfg, freq_hz, BUZZER_VOLUME);
    buzzer_stop_tick = HAL_GetTick() + BUZZER_BEEP_MS;
}

/**
 * @brief Update buzzer timing and stop the beep when done
 *
 * This is kept as a static (internal) helper because it is part of the
 * internal timing mechanism of PongEngine. The public API is just
 * PongEngine_Update() - callers don't need to know about buzzer timing.
 * This demonstrates good encapsulation: hide the details, expose only
 * the essential interface.
 */
static void PongEngine_UpdateBuzzer(void)
{
    if (buzzer_stop_tick != 0 && (int32_t)(HAL_GetTick() - buzzer_stop_tick) >= 0) {
        buzzer_off(&buzzer_cfg);
        buzzer_stop_tick = 0;
    }
}

/**
 * @brief Handle ball collision with screen walls
 * 
 * Bounces ball off top, bottom, and right edges of screen.
 * Left edge is handled by goal logic (missed paddle).
 * 
 * @param engine Pointer to game engine
 */
static void PongEngine_CheckWallCollision(PongEngine_t* engine) {
    Ball_t* ball = &engine->ball;
    Vector2D vel = Ball_GetVelocity(ball);
    
    // Top wall collision - reverse Y velocity
    if (ball->y <= 0) {
        ball->y = 2;
        Ball_SetVelocity(ball, vel.x, -vel.y);
        PongEngine_Beep(BUZZER_WALL_FREQ_HZ);
    }
    // Bottom wall collision - reverse Y velocity
    else if (ball->y + ball->size >= SCREEN_HEIGHT) {
        ball->y = SCREEN_HEIGHT - ball->size - 2;
        Ball_SetVelocity(ball, vel.x, -vel.y);
        PongEngine_Beep(BUZZER_WALL_FREQ_HZ);
    }
    
    // Right wall collision - reverse X velocity
    if (ball->x + ball->size >= SCREEN_WIDTH) {
        ball->x = SCREEN_WIDTH - ball->size - 2;
        Ball_SetVelocity(ball, -vel.x, vel.y);
        PongEngine_Beep(BUZZER_WALL_FREQ_HZ);
    }
}

/**
 * @brief Handle ball collision with paddle using AABB
 * 
 * Uses AABB (Axis-Aligned Bounding Box) collision detection:
 * - Gets bounding boxes for both ball and paddle
 * - Checks if they overlap using AABB_Collides()
 * - If collision detected, reverses ball X velocity and increments score
 * 
 * **How AABB Collision Works:**
 * Two boxes collide if they overlap in BOTH X and Y axes:
 * - X-axis overlap: box_a.x < box_b.x+box_b.width AND box_a.x+box_a.width > box_b.x
 * - Y-axis overlap: box_a.y < box_b.y+box_b.height AND box_a.y+box_a.height > box_b.y
 * 
 * @param engine Pointer to game engine
 */
static void PongEngine_CheckPaddleCollision(PongEngine_t* engine) {
    Ball_t* ball = &engine->ball;
    Paddle_t* paddle = &engine->paddle;
    Vector2D vel = Ball_GetVelocity(ball);
    
    // Get bounding boxes for collision detection
    AABB ball_box = Ball_GetAABB(ball);
    AABB paddle_box = Paddle_GetAABB(paddle);
    
    // Check if ball and paddle AABBs collide
    if (AABB_Collides(&ball_box, &paddle_box)) {
        // Collision detected! Reverse ball X velocity
        Ball_SetVelocity(ball, -vel.x, vel.y);
        
        // Move ball to prevent getting stuck in paddle
        ball->x = paddle_box.x + paddle_box.width;
        
        // Increment paddle score
        Paddle_AddScore(paddle);

        PongEngine_Beep(BUZZER_PADDLE_FREQ_HZ);
    }
}

/**
 * @brief Check if ball has left the play area (missed paddle)
 * 
 * If ball goes beyond left edge, decrement lives and reset ball to center.
 * 
 * @param engine Pointer to game engine
 */
static void PongEngine_CheckGoal(PongEngine_t* engine) {
    Ball_t* ball = &engine->ball;
    
    // Ball left the left edge (missed by paddle)
    if (ball->x < 0) {
        engine->lives--;

        // Reset ball near center with random offset
        Position2D center_pos;
        center_pos.x = (SCREEN_WIDTH - ball->size) / 2;
        center_pos.y = (SCREEN_HEIGHT - ball->size) / 2;

        // Add a small random offset in range [-BALL_RESET_OFFSET, BALL_RESET_OFFSET] to vary the reset position
        int16_t dx = (int16_t)Random_U16((uint16_t)(2 * BALL_RESET_OFFSET + 1)) - BALL_RESET_OFFSET;
        int16_t dy = (int16_t)Random_U16((uint16_t)(2 * BALL_RESET_OFFSET + 1)) - BALL_RESET_OFFSET;

        center_pos.x += dx;
        center_pos.y += dy;

        Ball_SetPos(ball, center_pos);

        // Reset velocity to initial direction (move right)
        Ball_SetVelocity(ball, 8.0f * 0.707f, 8.0f * 0.707f);
    }
}

void PongEngine_Init(PongEngine_t* engine,
                     int16_t paddle_x, int16_t paddle_y,
                     int16_t paddle_width, int16_t paddle_height,
                     int16_t ball_size, float ball_speed) {
    // Initialize ball at center
    Ball_Init(&engine->ball, ball_size, ball_speed);
    
    // Initialize paddle with faster movement for responsiveness
    Paddle_Init(&engine->paddle, paddle_x, paddle_y, 
                paddle_width, paddle_height, 6);  // speed = 6 pixels/frame (increased from 3)
    
    // Initialize lives
    engine->lives = 4;  // Give player 4 lives
}

uint8_t PongEngine_Update(PongEngine_t* engine, UserInput input) {
    // Step 1: Update paddle based on input
    Paddle_Update(&engine->paddle, input);
    
    // Step 2: Update ball position
    Ball_Update(&engine->ball);
    
    // Step 3: Check collisions
    PongEngine_CheckWallCollision(engine);      // Bounce off top/bottom
    PongEngine_CheckPaddleCollision(engine);    // Hit paddle (with AABB)
    PongEngine_CheckGoal(engine);               // Check if ball left play area
    
    PongEngine_UpdateBuzzer();

    return engine->lives;
}

void PongEngine_Draw(PongEngine_t* engine) {
    Ball_Draw(&engine->ball);
    Paddle_Draw(&engine->paddle);
}

uint8_t PongEngine_GetLives(PongEngine_t* engine) {
    return engine->lives;
}

uint16_t PongEngine_GetScore(PongEngine_t* engine) {
    return Paddle_GetScore(&engine->paddle);
}
