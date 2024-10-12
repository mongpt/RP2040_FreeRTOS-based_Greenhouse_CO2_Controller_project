//
// Created by ADMIN on 10/5/2024.
//

#include "tls_task.h"

void connect_to_wifi(const char *ssid, const char *password) {
    // Initial Wi-Fi connection
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return;
    }
    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(SSID_WIFI, PASS_WIFI, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect\n");
        return;
    }
    printf("wifi connected\n");
}

void sendToCloud(int c, int t, int h, int s, int sp){
    char REQUEST[256];
    snprintf(REQUEST, sizeof (REQUEST),
             "POST https://api.thingspeak.com/update?api_key=" API_KEY "&field1=%d&field2=%d&field3=%d&field4=%d&field5=%d HTTP/1.1\r\n" \
                 "Host: " TLS_CLIENT_SERVER "\r\n" \
                 "Connection: close\r\n" \
                 "\r\n", c, t, h, s, sp);
    bool pass = run_tls_client_test(NULL, 0, TLS_CLIENT_SERVER, REQUEST, TLS_CLIENT_TIMEOUT_SECS);
    if (pass) {
        printf("Data sent to cloud\n");
    } else {
        printf("Data failed to send to cloud\n");
    }
}

bool receiveFromCloud(){
    char REQUEST[256];
    snprintf(REQUEST, sizeof (REQUEST),
             "POST https://api.thingspeak.com/talkbacks/53272/commands/execute.json?api_key=" TALKBACK_KEY " HTTP/1.1\r\n" \
                 "Host: " TLS_CLIENT_SERVER "\r\n" \
                 "Connection: close\r\n" \
                 "\r\n");
    bool pass = run_tls_client_test(NULL, 0, TLS_CLIENT_SERVER, REQUEST, TLS_CLIENT_TIMEOUT_SECS);
    if (pass) {
        return true;
    } else {
        printf("failed to send request\n");
        return false;
    }
}

void cloudSenderCB(TimerHandle_t xTimer){
    sendToCloud(co2, temp, rh, speed, setpoint);  // Replace with actual values
}

void cloudReceiverCB(TimerHandle_t xTimer){
    if (receiveFromCloud()){
        if (xQueueReceive(setpoint_q, &setpoint, 0)){
            printf("new setpoint: %d\n", setpoint);
        }
    }
}

void tls_task(void *param)
{
    // Initial Wi-Fi connection
    connect_to_wifi(SSID_WIFI, PASS_WIFI);
    // Create timers
    cloudSender_T = xTimerCreate("SendToCloudTimer", pdMS_TO_TICKS(SEND_INTERVAL), pdTRUE, NULL, cloudSenderCB);
    cloudReceiver_T = xTimerCreate("ReceiveFromCloudTimer", pdMS_TO_TICKS(RECEIVE_INTERVAL), pdTRUE, NULL, cloudReceiverCB);
    //create other tasks

    // Start timers
    xTimerStart(cloudSender_T, 0);
    xTimerStart(cloudReceiver_T, 0);

    while(true) {
        EventBits_t bits = xEventGroupWaitBits(wifiEventGroup, WIFI_CHANGE_BIT_TLS, pdTRUE, pdFALSE, 0);
        if (bits & WIFI_CHANGE_BIT_TLS) {
            // Disconnect and clean up the previous connection
            printf("Reconnecting to new Wi-Fi credentials...\n");

            xTimerStop(cloudSender_T, 0);
            xTimerStop(cloudReceiver_T, 0);

            cyw43_arch_deinit();  // Clean up the existing Wi-Fi connection

            // Initial Wi-Fi connection
            connect_to_wifi(SSID_WIFI, PASS_WIFI);

            // Restart timers
            xTimerStart(cloudSender_T, 0);
            xTimerStart(cloudReceiver_T, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Continue running
    }
}