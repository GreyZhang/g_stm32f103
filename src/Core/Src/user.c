/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdint.h"
#include "printf.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "cmsis_gcc.h"
#include "user.h"

void user_gpio_test_func(void);

uint32_t os_lld_task_1000ms_counter;
UBaseType_t uxHighWaterMark_500ms;
UBaseType_t uxHighWaterMark_1000ms;

void freertos_lld_task_500ms(void *argument)
{
    (void)argument;

    uxHighWaterMark_500ms = uxTaskGetStackHighWaterMark( NULL );

    for (;;)
    {
        vTaskDelay(500U);
        uxHighWaterMark_500ms = uxTaskGetStackHighWaterMark( NULL );
        printf("water mark fo task 500ms: %d\n", uxHighWaterMark_500ms);
    }
}

void freertos_lld_1000ms_task(void *argument)
{
    /* Infinite loop */
    uxHighWaterMark_1000ms = uxTaskGetStackHighWaterMark( NULL );

    for (;;)
    {
        os_lld_task_1000ms_counter++;
        user_gpio_test_func();
        vTaskDelay(1000U);
        printf("%d:----------------------------------------------\n", os_lld_task_1000ms_counter);
        uxHighWaterMark_1000ms = uxTaskGetStackHighWaterMark( NULL );
        printf("water mark fo task 1000ms: %d\n", uxHighWaterMark_1000ms);
    }
}

void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    printf("stack overflow found.\n");
}

void HAL_IncTick(void)
{
    osSystickHandler();
}

void _putchar(char character)
{
    char c = character;

    HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1U, 0xFFFFFFFFU);
}

void user_gpio_test_func(void)
{
    HAL_GPIO_TogglePin(mcu_pin_pc13_led_GPIO_Port, mcu_pin_pc13_led_Pin);
}
