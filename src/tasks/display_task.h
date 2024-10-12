//
// Created by ADMIN on 10/5/2024.
//

#ifndef FREERTOS_VENTILATION_PROJECT_DISPLAY_TASK_H
#define FREERTOS_VENTILATION_PROJECT_DISPLAY_TASK_H

#include <cstdint>

extern int co2;
extern int temp;
extern int rh;
extern int speed;
extern uint16_t setpoint;

void display_task(void *param);

#endif //FREERTOS_VENTILATION_PROJECT_DISPLAY_TASK_H
