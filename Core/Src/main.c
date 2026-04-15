// ELEC2645 PacMaze Demo
// =====================

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "adc.h"
#include "rng.h"
#include "Utils.h"

#include "Buzzer.h"
#include "PWM.h"
#include "LCD.h"
#include "Joystick.h"
#include "PacMaze.h"

#include <stdint.h>
#include <stdio.h>

/* ===== AUTO-GENERATED STM32 FUNCTION PROTOTYPES ===== */
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);

/* ===== UART printf redirection ===== */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}


/* ===== BUZZER CONFIG ===== */
Buzzer_cfg_t buzzer_cfg = {
    .htim = &htim2,
    .channel = TIM_CHANNEL_3,
    .tick_freq_hz = 1000000,
    .min_freq_hz = 20,
    .max_freq_hz = 20000,
    .setup_done = 0
};

/* ===== LCD CONFIG ===== */
ST7789V2_cfg_t cfg0 = {
    .setup_done = 0,
    .spi = SPI2,
    .RST = {.port = GPIOB, .pin = GPIO_PIN_2},
    .BL = {.port = GPIOB, .pin = GPIO_PIN_1},
    .DC = {.port = GPIOB, .pin = GPIO_PIN_11},
    .CS = {.port = GPIOB, .pin = GPIO_PIN_12},
    .MOSI = {.port = GPIOB, .pin = GPIO_PIN_15},
    .SCLK = {.port = GPIOB, .pin = GPIO_PIN_13},
    .dma = {.instance = DMA1, .channel = DMA1_Channel5}
};

/* ===== JOYSTICK CONFIG ===== */
Joystick_cfg_t joystick_cfg = {
    .adc = &hadc1,
    .x_channel = ADC_CHANNEL_1,   // A5 on Nucleo board
    .y_channel = ADC_CHANNEL_2,   // A4 on Nucleo board
    .sampling_time = ADC_SAMPLETIME_47CYCLES_5,
    .center_x = JOYSTICK_DEFAULT_CENTER_X,
    .center_y = JOYSTICK_DEFAULT_CENTER_Y,
    .deadzone = JOYSTICK_DEADZONE,
    .setup_done = 0
};

Joystick_t joystick_data;

/* ===== PWM CONFIG ===== */
PWM_cfg_t pwm_cfg = {
    .htim = &htim4,
    .channel = TIM_CHANNEL_1,
    .tick_freq_hz = 1000000,
    .min_freq_hz = 10,
    .max_freq_hz = 50000,
    .setup_done = 0
};

/* ===== GAME ===== */
PacMaze_t pacmaze;

void Game_Buzzer_Play(uint16_t freq)
{
    if (freq < 20) freq = 20;
    if (freq > 20000) freq = 20000;

    uint32_t arr = (buzzer_cfg.tick_freq_hz / freq) - 1;

    __HAL_TIM_SET_AUTORELOAD(buzzer_cfg.htim, arr);
    __HAL_TIM_SET_COMPARE(buzzer_cfg.htim, buzzer_cfg.channel, arr / 2);
    HAL_TIM_PWM_Start(buzzer_cfg.htim, buzzer_cfg.channel);
}

void Game_Buzzer_Stop(void)
{
    HAL_TIM_PWM_Stop(buzzer_cfg.htim, buzzer_cfg.channel);
}

void Game_LED_On(void)
{
    PWM_SetDuty(&pwm_cfg, 70);
}

void Game_LED_Off(void)
{
    PWM_SetDuty(&pwm_cfg, 0);
}

/* ===== FRAME TIMING ===== */
#define FPS 30
#define FRAME_TIME_MS (1000 / FPS)

/**
  * @brief  The application entry point
  * @retval int
  */
int main(void)
{
    /* MCU Configuration */
    HAL_Init();
    SystemClock_Config();
    PeriphCommonClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_ADC1_Init();
    MX_RNG_Init();

    /* Initialize LCD first */
    LCD_init(&cfg0);

    /* Initialize buzzer timer */
    MX_TIM2_Init();
    buzzer_init(&buzzer_cfg);

    /* Initialize TIM4 after LCD */
    MX_TIM4_Init();

    /* Initialize PWM for LED */
    PWM_Init(&pwm_cfg);
    PWM_SetFreq(&pwm_cfg, 1000);
    PWM_SetDuty(&pwm_cfg, 0);

    /* Ensure LD2 starts OFF */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    /* Initialize joystick */
    Joystick_Init(&joystick_cfg);

    /* Initialize game */
    PacMaze_Init(&pacmaze);
    PacMaze_Render(&pacmaze, &cfg0);

    printf("PacMaze initialized.\n");

    uint32_t last_tick = HAL_GetTick();

    while (1)
    {
        uint32_t now = HAL_GetTick();
        if ((now - last_tick) < FRAME_TIME_MS) {
            continue;
        }
        last_tick = now;

        /* Read joystick */
        Joystick_Read(&joystick_cfg, &joystick_data);

        UserInput input = Joystick_GetInput(&joystick_data);

        /* Update and render PacMaze */
        PacMaze_Update(&pacmaze, input);
        PacMaze_Render(&pacmaze, &cfg0);
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 1;
    RCC_OscInitStruct.PLL.PLLN = 10;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RNG | RCC_PERIPHCLK_ADC;
    PeriphClkInit.AdcClockSelection = RCC_ADCCLKSOURCE_PLLSAI1;
    PeriphClkInit.RngClockSelection = RCC_RNGCLKSOURCE_PLLSAI1;
    PeriphClkInit.PLLSAI1.PLLSAI1Source = RCC_PLLSOURCE_HSI;
    PeriphClkInit.PLLSAI1.PLLSAI1M = 1;
    PeriphClkInit.PLLSAI1.PLLSAI1N = 8;
    PeriphClkInit.PLLSAI1.PLLSAI1P = RCC_PLLP_DIV7;
    PeriphClkInit.PLLSAI1.PLLSAI1Q = RCC_PLLQ_DIV4;
    PeriphClkInit.PLLSAI1.PLLSAI1R = RCC_PLLR_DIV2;
    PeriphClkInit.PLLSAI1.PLLSAI1ClockOut = RCC_PLLSAI1_48M2CLK | RCC_PLLSAI1_ADC1CLK;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* You can add your own implementation here if needed */
}
#endif /* USE_FULL_ASSERT */