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
void user_can_test_func(void);

uint32_t os_lld_task_1000ms_counter;
UBaseType_t uxHighWaterMark_500ms;
UBaseType_t uxHighWaterMark_1000ms;
CAN_TxHeaderTypeDef user_can_tx_header;
CAN_RxHeaderTypeDef user_can_rx_header;
uint8_t user_can_tx_data[8];
uint8_t user_can_rx_data[8];
uint32_t tx_buffer;

void freertos_lld_task_500ms(void *argument)
{
    (void)argument;

    uxHighWaterMark_500ms = uxTaskGetStackHighWaterMark(NULL);

    for (;;)
    {
        vTaskDelay(500U);
        uxHighWaterMark_500ms = uxTaskGetStackHighWaterMark(NULL);
        printf("water mark fo task 500ms: %d\n", uxHighWaterMark_500ms);
    }
}

void freertos_lld_1000ms_task(void *argument)
{
    /* Infinite loop */
    uxHighWaterMark_1000ms = uxTaskGetStackHighWaterMark(NULL);

    for (;;)
    {
        uint32_t i = 0U;
        os_lld_task_1000ms_counter++;
        user_gpio_test_func();
        for(i = 0U; i < 1U * 9U; i++)
        {
            user_can_test_func();
        }
        vTaskDelay(1000U);
        printf("%d:----------------------------------------------\n", os_lld_task_1000ms_counter);
        uxHighWaterMark_1000ms = uxTaskGetStackHighWaterMark(NULL);
        printf("water mark fo task 1000ms: %d\n", uxHighWaterMark_1000ms);
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
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

void user_can_test_func(void)
{
    uint8_t csend[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    user_can_tx_header.IDE = CAN_ID_STD;
    user_can_tx_header.StdId = 0x77U;
    user_can_tx_header.RTR = CAN_RTR_DATA;
    user_can_tx_header.TransmitGlobalTime = DISABLE;
    HAL_CAN_AddTxMessage(&hcan, &user_can_tx_header, csend, &tx_buffer);

    printf("mailbox used for CAN tx: %d\n", tx_buffer);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan1)
{
    HAL_CAN_GetRxMessage(hcan1, CAN_RX_FIFO0, &user_can_rx_header, user_can_rx_data);
}

void user_can_set_rx_filer(void)
{
    CAN_FilterTypeDef can_filter;

    can_filter.FilterBank = 0;
    can_filter.FilterMode = CAN_FILTERMODE_IDMASK;
    can_filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    can_filter.FilterIdHigh = 0;
    can_filter.FilterIdLow = 0;
    can_filter.FilterMaskIdHigh = 0;
    can_filter.FilterMaskIdLow = 0;
    can_filter.FilterScale = CAN_FILTERSCALE_32BIT;
    can_filter.FilterActivation = ENABLE;
    can_filter.SlaveStartFilterBank = 14;
    HAL_CAN_ConfigFilter(&hcan, &can_filter);
}
