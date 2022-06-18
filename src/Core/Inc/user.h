#ifndef USER_H
#define USER_H

#include "main.h"
#include "cmsis_os.h"

extern  UART_HandleTypeDef huart1;
extern CAN_HandleTypeDef hcan;
void user_can_set_rx_filer(void);

#endif
