#include <iostream>
#include <sstream>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hardware/gpio.h"
#include "PicoOsUart.h"
#include "ssd1306os.h"
#include "pico/cyw43_arch.h"
#include "timers.h"
#include "hardware/timer.h"
#include <cstdio>

#include "PicoI2C.h"

#include "blinker.h"

#include "event_groups.h"

#include "global_definition.h"
#include "screen_selection.h"
#include "eeprom.h"
#include "ModbusClient.h"
#include "ModbusRegister.h"

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

#include "tls_common.h"


//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void eeprom_task(void *param) {
    auto eeprom = std::make_shared<PicoI2C>(I2C_0, I2C_BAUD);
    // Data to write
    readAtBoot(eeprom);
    vTaskDelay(pdMS_TO_TICKS(500));

    while(true) {
        // check if wifi credentials changed
        EventBits_t bits = xEventGroupWaitBits(wifiEventGroup, WIFI_CHANGE_BIT_EEPROM, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));
        if (bits & WIFI_CHANGE_BIT_EEPROM) {
            printf("debug strlen(SSID_WIFI) %d\n", strlen(SSID_WIFI));
            //write new data to eeprom
            writeEEPROM(eeprom ,SSID_ADDR, reinterpret_cast<const uint8_t *>(SSID_WIFI), strlen(SSID_WIFI));
            printf("SSID %s written successfully!\n", SSID_WIFI);
            writeEEPROM(eeprom ,PASS_ADDR, reinterpret_cast<const uint8_t *>(PASS_WIFI), strlen(PASS_WIFI));
            printf("PASS %s written successfully!\n", PASS_WIFI);
            xEventGroupSetBits(wifiEventGroup, WIFI_CHANGE_BIT_TLS);
        }
        // check if co2 setpoint changed
        bits = xEventGroupWaitBits(co2EventGroup, CO2_CHANGE_BIT_EEPROM, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));
        if (bits & CO2_CHANGE_BIT_EEPROM) {
            //write new data to eeprom
            uint8_t buf[2];
            buf[0] = (uint8_t)(setpoint >> ONE_BYTE); //high byte of memory address
            buf[1] = (uint8_t)setpoint;     //low byte of memory address
            writeEEPROM(eeprom ,CO2_SETPOINT_ADDR, buf, 2);
            printf("SETPOINT %u written successfully!\n", setpoint);
        }
    }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
bool connect_to_wifi(const char *ssid, const char *password) {
    // Initial Wi-Fi connection
    if (cyw43_arch_init()) {
        printf("failed to initialise\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(SSID_WIFI, PASS_WIFI, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("failed to connect\n");
        return false;
    }
    printf("wifi connected\n");
    return true;
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
            printf("new setpoint from cloud: %d\n", setpoint);
            xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_LCD | CO2_CHANGE_BIT_EEPROM);
        }
    }
}

void tls_task(void *param)
{
    // Initial Wi-Fi connection
    if (!connect_to_wifi(SSID_WIFI, PASS_WIFI)) {
        //suspend ourselve if failed to connect to network
        printf("tlf_task has been suspended\n");
        vTaskSuspend(NULL);
    }
    // Create timers
    cloudSender_T = xTimerCreate("SendToCloudTimer", pdMS_TO_TICKS(SEND_INTERVAL), pdTRUE, NULL, cloudSenderCB);
    cloudReceiver_T = xTimerCreate("ReceiveFromCloudTimer", pdMS_TO_TICKS(RECEIVE_INTERVAL), pdTRUE, NULL, cloudReceiverCB);
    //create other tasks

    // Start timers
    xTimerStart(cloudSender_T, 0);
    xTimerStart(cloudReceiver_T, 0);

    while(true) {
        EventBits_t bits = xEventGroupWaitBits(wifiEventGroup, WIFI_CHANGE_BIT_TLS, pdTRUE, pdFALSE, 50);
        if (bits & WIFI_CHANGE_BIT_TLS) {
            // Disconnect and clean up the previous connection
            printf("Reconnecting to new Wi-Fi ...\n");

            xTimerStop(cloudSender_T, 0);
            xTimerStop(cloudReceiver_T, 0);

            cyw43_arch_deinit();  // Clean up the existing Wi-Fi connection
            vTaskDelay(pdMS_TO_TICKS(1000));
            // Initial Wi-Fi connection
            connect_to_wifi(SSID_WIFI, PASS_WIFI);

            // Restart timers
            xTimerStart(cloudSender_T, 0);
            xTimerStart(cloudReceiver_T, 0);
        }
    }
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void display_task(void *param)
{
    auto i2cbus{std::make_shared<PicoI2C>(1, 400000)};
    auto display = std::make_shared<ssd1306os>(i2cbus);
    currentScreen screen(display);
    rotBtnData_str temp_data;
    wifi_char_pos_str char_pos_X;
    screen.welcome();
    vTaskDelay(pdMS_TO_TICKS(5000));
    screen.screenSelection();

    while(true) {
        if (xQueueReceive(gpio_data_q, &temp_data, pdMS_TO_TICKS(20))){
            if (temp_data.action_type == BACK_PRESS){
                screen.screenSelection();
            } else if (temp_data.action_type == OK_PRESS){
                if (selection_screen_option == 0){  //info
                    screen.info(co2, temp, rh, speed, setpoint);
                } else if (selection_screen_option == 1){   //set co2
                    screen.setCo2(setpoint);
                } else if (selection_screen_option == 2){   //config wifi
                    screen.wifiConfig(SSID_WIFI, PASS_WIFI);
                }
            } else if (temp_data.action_type == ROT_CW || temp_data.action_type == ROT_CCW){
                if (rot_btn_data.screen_type == SELECTION_SCR){
                    screen.screenSelection();
                } else if (rot_btn_data.screen_type == SET_CO2_SCR){
                    screen.setCo2(setpoint_temp);
                } else if (rot_btn_data.screen_type == WIFI_CONF_SCR){
                    screen.wifiConfig(SSID_WIFI, PASS_WIFI);
                }
            }
        }
        if (xQueueReceive(wifiCharPos_q, &char_pos_X, pdMS_TO_TICKS(20))){
            screen.asciiCharSelection(char_pos_X.pos_x, char_pos_X.ch);
        }
        EventBits_t bits = xEventGroupWaitBits(co2EventGroup, CO2_CHANGE_BIT_LCD, pdTRUE, pdFALSE, pdMS_TO_TICKS(20));
        if (bits & CO2_CHANGE_BIT_LCD){
            if (rot_btn_data.screen_type == INFO_SCR){
                screen.info(co2, temp, rh, speed, setpoint);
            }
        }
    }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void hw_ISR_CB(uint gpio, uint32_t events){
    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    if (gpio == ok_btn){
        rot_btn_data.action_type = OK_PRESS;
        xQueueSendFromISR(gpio_action_q, &rot_btn_data.action_type, &pxHigherPriorityTaskWoken);
    }
    if (gpio == back_btn){
        rot_btn_data.action_type = BACK_PRESS;
        xQueueSendFromISR(gpio_action_q, &rot_btn_data.action_type, &pxHigherPriorityTaskWoken);
    }
    if (gpio == rotA){
        if (gpio_get(rotA) != gpio_get(rotB)) rot_btn_data.action_type = ROT_CW;
        else rot_btn_data.action_type = ROT_CCW;
        xQueueSendFromISR(gpio_action_q, &rot_btn_data.action_type, &pxHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void gpio_task(void *param) {
    (void) param;
    gpio_init(rotA);
    gpio_set_dir(rotA, GPIO_IN);
    gpio_init(rotB);
    gpio_set_dir(rotB, GPIO_IN);

    gpio_init(led1);
    gpio_set_dir(led1, GPIO_OUT);

    gpio_init(valve);
    gpio_set_dir(valve, GPIO_OUT);

    gpio_init(ok_btn);
    gpio_set_dir(ok_btn, GPIO_IN);
    gpio_pull_up(ok_btn);
    gpio_init(back_btn);
    gpio_set_dir(back_btn, GPIO_IN);
    gpio_pull_up(back_btn);

    gpio_set_irq_enabled_with_callback(rotA, GPIO_IRQ_EDGE_FALL, true, &hw_ISR_CB);
    gpio_set_irq_enabled_with_callback(ok_btn, GPIO_IRQ_EDGE_FALL, true, &hw_ISR_CB);
    gpio_set_irq_enabled_with_callback(back_btn, GPIO_IRQ_EDGE_FALL, true, &hw_ISR_CB);

    char backup_ssid[50] = "\0";
    char backup_pass[50] = "\0";
    int posX = 44;
    int asciiChar = START_ASCII;
    uint64_t lastPressTime = time_us_64();
    strcpy(backup_ssid, SSID_WIFI);
    strcpy(backup_pass, PASS_WIFI);

    while(true) {
        if (xQueueReceive(gpio_action_q, &rot_btn_data.action_type, portMAX_DELAY)){
            switch (rot_btn_data.action_type){
                case BACK_PRESS:
                    rot_btn_data.screen_type = SELECTION_SCR;
                    setpoint_temp = setpoint;
                    isEditing = false;
                    xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                    break;
                case OK_PRESS:
                    gpio_set_irq_enabled_with_callback(ok_btn, GPIO_IRQ_EDGE_FALL, false, &hw_ISR_CB);
                    if (time_us_64() - lastPressTime > 30000){ //debounce 30ms
                        lastPressTime = time_us_64();
                        if (rot_btn_data.screen_type == SELECTION_SCR){
                            if (selection_screen_option == 0){  //info
                                rot_btn_data.screen_type = INFO_SCR;
                                xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                            } else if (selection_screen_option == 1){   //set co2
                                rot_btn_data.screen_type = SET_CO2_SCR;
                                setpoint_temp = setpoint;
                                xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                            } else if (selection_screen_option == 2){   //config wifi
                                rot_btn_data.screen_type = WIFI_CONF_SCR;
                                xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                            }
                        } else if (rot_btn_data.screen_type == SET_CO2_SCR){
                            setpoint = setpoint_temp;
                            xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_EEPROM);
                        } else if (rot_btn_data.screen_type == WIFI_CONF_SCR){
                            if (!isEditing) {
                                int count = 0;
                                while (!gpio_get(ok_btn) && ++count < 150){
                                    vTaskDelay(pdMS_TO_TICKS(10));
                                }
                                if (count >= 150){ //long press to enter editing
                                    isEditing = true;
                                    switch (wifi_screen_option) {
                                        case 0:
                                            strcpy(SSID_WIFI, "");
                                            strcpy(backup_ssid, "");
                                            posX = SSID_POS_X;
                                            break;
                                        case 1:
                                            strcpy(PASS_WIFI, "");
                                            strcpy(backup_pass, "");
                                            posX = PASS_POS_X;
                                            break;
                                        default:
                                            break;
                                    }
                                    asciiChar = START_ASCII - 1;
                                    wifi_char_pos_X.pos_x = posX;
                                    wifi_char_pos_X.ch = asciiChar;
                                    xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                                    xQueueSend(wifiCharPos_q, &wifi_char_pos_X, portMAX_DELAY);
                                } else {    //short press to save data
                                    printf("saving data...\n");
                                    xEventGroupSetBits(wifiEventGroup, WIFI_CHANGE_BIT_EEPROM);
                                }
                            } else {    // isChanging = true
                                int count = 0;
                                while (!gpio_get(ok_btn) && ++count < 150){
                                    vTaskDelay(pdMS_TO_TICKS(10));
                                }
                                if (count >= 150){  // long press to exit editing
                                    isEditing = false;
                                    if (wifi_screen_option == 0){
                                        strcpy(SSID_WIFI, backup_ssid);
                                        SSID_WIFI[strlen(SSID_WIFI)] = '\0';
                                    } else {
                                        strcpy(PASS_WIFI, backup_pass);
                                        PASS_WIFI[strlen(PASS_WIFI)] = '\0';
                                    }
                                    printf("ssid %s\n", SSID_WIFI);
                                    printf("pass %s\n", PASS_WIFI);
                                    //wifi_screen_option = !wifi_screen_option;
                                    xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                                } else {    //short press to confirm character
                                    char c[2];
                                    sprintf(c, "%c", asciiChar);
                                    switch (wifi_screen_option) {
                                        case 0:
                                            strcat(SSID_WIFI, c);
                                            strcat(backup_ssid, c);
                                            break;
                                        case 1:
                                            strcat(PASS_WIFI, c);
                                            strcat(backup_pass, c);
                                            break;
                                        default:
                                            break;
                                    }
                                    vTaskDelay(pdMS_TO_TICKS(5));
                                    posX += CHAR_WIDTH;
                                    if (posX > MAX_POS_X){
                                        posX -= CHAR_WIDTH;
                                        //scroll back 1 char
                                        if (wifi_screen_option == 0){
                                            for (int i = 0; i < strlen(SSID_WIFI); i++){
                                                SSID_WIFI[i] = SSID_WIFI[i+1];
                                            }
                                        } else {
                                            for (int i = 0; i < strlen(PASS_WIFI); i++){
                                                PASS_WIFI[i] = PASS_WIFI[i+1];
                                            }
                                        }
                                        wifi_char_pos_X.pos_x = posX;
                                        wifi_char_pos_X.ch = asciiChar;
                                        xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                                        xQueueSend(wifiCharPos_q, &wifi_char_pos_X, portMAX_DELAY);
                                    }
                                    asciiChar = START_ASCII - 1;
                                    wifi_char_pos_X.pos_x = posX;
                                    wifi_char_pos_X.ch = asciiChar;
                                    xQueueSend(wifiCharPos_q, &wifi_char_pos_X, portMAX_DELAY);
                                }
                            }
                        }
                    }
                    while (!gpio_get(ok_btn)) vTaskDelay(pdMS_TO_TICKS(10));
                    gpio_set_irq_enabled_with_callback(ok_btn, GPIO_IRQ_EDGE_FALL, true, &hw_ISR_CB);
                    break;
                case ROT_CW:
                    if (rot_btn_data.screen_type == SELECTION_SCR){
                        selection_screen_option = (selection_screen_option + 1) % 3;
                        xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                    } else if (rot_btn_data.screen_type == SET_CO2_SCR){
                        setpoint_temp = (setpoint_temp < MAX_SETPOINT) ? setpoint_temp + 1 : MAX_SETPOINT;
                        xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                    } else if (rot_btn_data.screen_type == WIFI_CONF_SCR){
                        if (!isEditing){
                            wifi_screen_option = !wifi_screen_option;
                            xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                        } else {    //change character while editing
                            if (++asciiChar > END_ASCII) asciiChar = START_ASCII;
                            wifi_char_pos_X.pos_x = posX;
                            wifi_char_pos_X.ch = asciiChar;
                            xQueueSend(wifiCharPos_q, &wifi_char_pos_X, portMAX_DELAY);
                        }
                    }
                    break;
                case ROT_CCW:
                    if (rot_btn_data.screen_type == SELECTION_SCR){
                        selection_screen_option = (selection_screen_option + 2) % 3;
                        xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                    } else if (rot_btn_data.screen_type == SET_CO2_SCR){
                        setpoint_temp = (setpoint_temp > MIN_SETPOINT) ? setpoint_temp - 1 : MIN_SETPOINT;
                        xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                    } else if (rot_btn_data.screen_type == WIFI_CONF_SCR){
                        if (!isEditing){
                            wifi_screen_option = !wifi_screen_option;
                            xQueueSend(gpio_data_q, &rot_btn_data, portMAX_DELAY);
                        } else {    //change character while editing
                            if (--asciiChar < START_ASCII) asciiChar = END_ASCII;
                            wifi_char_pos_X.pos_x = posX;
                            wifi_char_pos_X.ch = asciiChar;
                            xQueueSend(wifiCharPos_q, &wifi_char_pos_X, portMAX_DELAY);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void modbus_task(void *param) {
    auto uart{std::make_shared<PicoOsUart>(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE, STOP_BITS)};
    auto rtu_client{std::make_shared<ModbusClient>(uart)};
    ModbusRegister mb_co2(rtu_client, 240, 256);
    ModbusRegister mb_rh(rtu_client, 241, 256);
    ModbusRegister mb_temp(rtu_client, 241, 257);
    ModbusRegister mb_fanSpeed(rtu_client, 1, 0);

    while (true) {
        co2 = mb_co2.read();
        vTaskDelay(pdMS_TO_TICKS(5));
        rh = mb_rh.read()/10;
        vTaskDelay(pdMS_TO_TICKS(5));
        temp = mb_temp.read()/10;
        vTaskDelay(pdMS_TO_TICKS(5));
        xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_LCD);
        vTaskDelay(pdMS_TO_TICKS(100));
        // a loop to wait for 3s to start over
        int count = 0;
        while (++count < 300) {
            co2 = mb_co2.read();
            vTaskDelay(pdMS_TO_TICKS(10));
            if (co2 > CO2_LIMIT) {
                // start the fan
                mb_fanSpeed.write(500);
                vTaskDelay(pdMS_TO_TICKS(500));
                mb_fanSpeed.write(1000);
                speed = 1000 / 10; //100%
                vTaskDelay(pdMS_TO_TICKS(60000));   // wait for 1m
                co2 = mb_co2.read();
                vTaskDelay(pdMS_TO_TICKS(5));
                while (co2 > CO2_LIMIT) {
                    co2 = mb_co2.read();
                    vTaskDelay(pdMS_TO_TICKS(5));
                    xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_LCD);
                }
                // stop the fan
                mb_fanSpeed.write(0);
                speed = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                co2 = mb_co2.read();
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            if ((co2 - OFFSET) > setpoint) {
                // start the fan
                mb_fanSpeed.write(500);
                vTaskDelay(pdMS_TO_TICKS(100));
                speed = 500 / 10; // 50%
                co2 = mb_co2.read();
                vTaskDelay(pdMS_TO_TICKS(5));
                while ((co2 - OFFSET) > setpoint){
                    // read back co2 level every 100ms
                    co2 = mb_co2.read();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_LCD);
                }
                // stop the fan
                mb_fanSpeed.write(0);
                speed = 0;
                vTaskDelay(pdMS_TO_TICKS(500));
                co2 = mb_co2.read();
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            while ((co2 + OFFSET) < setpoint) {
                // open co2 valve for 1s
                gpio_put(valve, 1);
                vTaskDelay(pdMS_TO_TICKS(1000));
                gpio_put(valve, 0);
                vTaskDelay(pdMS_TO_TICKS(60000)); // wait for 1m to get stable co2
                co2 = mb_co2.read();
                vTaskDelay(pdMS_TO_TICKS(5));
                xEventGroupSetBits(co2EventGroup, CO2_CHANGE_BIT_LCD);
            }
        }
    }
}

//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void blink_task(void *param)
{
    auto lpr = (led_params *) param;
    const uint led_pin = lpr->pin;
    const uint delay = pdMS_TO_TICKS(lpr->delay);
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    while (true) {
        gpio_put(led_pin, true);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_put(led_pin, false);
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
}

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int main()
{
    static led_params lp1 = { .pin = 20, .delay = 5000 };
    stdio_init_all();
    printf("\nBoot\n");

    setpoint_q = xQueueCreate(QUEUE_SIZE, sizeof (setpoint));
    wifiCharPos_q = xQueueCreate(QUEUE_SIZE, sizeof(wifi_char_pos_str));
    gpio_data_q = xQueueCreate(QUEUE_SIZE, sizeof(rotBtnData_str));
    gpio_action_q = xQueueCreate(QUEUE_SIZE, sizeof(gpio_t));
    gpio_sem = xSemaphoreCreateBinary();
    wifiEventGroup = xEventGroupCreate();
    co2EventGroup = xEventGroupCreate();

    xTaskCreate(eeprom_task, "eeprom task", 2048, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(display_task, "SSD1306", 512, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(tls_task, "tls task", 6000, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(blink_task, "LED_1", 256, (void *) &lp1, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(modbus_task, "Modbus", 512, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(gpio_task, "gpio task", 256, nullptr, tskIDLE_PRIORITY + 1, nullptr);

    vTaskStartScheduler();

    while(true){};
}
