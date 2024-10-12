//
// Created by ADMIN on 10/5/2024.
//

#ifndef FREERTOS_VENTILATION_PROJECT_TLS_TASK_H
#define FREERTOS_VENTILATION_PROJECT_TLS_TASK_H

#include "event_groups.h"
#include "tls_common.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"

#define SEND_INTERVAL 60000 // 1m
#define RECEIVE_INTERVAL 5000 //5s
#define WIFI_CHANGE_BIT_TLS    (1 << 0)  // Bit 0 for tls_task
#define WIFI_CHANGE_BIT_EEPROM (1 << 1)  // Bit 1 for eeprom_task
#define TLS_CLIENT_SERVER           "api.thingspeak.com"
#define TLS_CLIENT_TIMEOUT_SECS     15
#define API_KEY                     "G6YC2UTUAHX5P9HF"
#define TALKBACK_KEY                "5U9WXY13SXD01SER"

extern TimerHandle_t cloudSender_T;
extern TimerHandle_t cloudReceiver_T;
extern char SSID_WIFI[64];
extern char PASS_WIFI[64];
extern int co2;
extern int temp;
extern int rh;
extern int speed;
extern uint16_t setpoint;
extern EventGroupHandle_t wifiEventGroup;

void connect_to_wifi(const char *ssid, const char *password);
void sendToCloud(int c, int t, int h, int s, int sp);
bool receiveFromCloud();
void cloudSenderCB(TimerHandle_t xTimer);
void cloudReceiverCB(TimerHandle_t xTimer);
void tls_task(void *param);

#endif //FREERTOS_VENTILATION_PROJECT_TLS_TASK_H
