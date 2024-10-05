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

extern "C" {
uint32_t read_runtime_ctr(void) {
    return timer_hw->timerawl;
}
}

#include "blinker.h"

//#define CLOUD
//#define USE_MODBUS
//#define USE_SSD1306
#define USE_EEPROM


#define TLS_CLIENT_SERVER           "api.thingspeak.com"
#define TLS_CLIENT_TIMEOUT_SECS     15
#define API_KEY                     "G6YC2UTUAHX5P9HF"
#define TALKBACK_KEY                "5U9WXY13SXD01SER"

#include "tls_common.h"
#define QUEUE_SIZE                  10

SemaphoreHandle_t gpio_sem;
QueueHandle_t setpoint_q;

int co2 = 0;
int temp = 0;
int rh = 0;
int speed = 0;
int setpoint = 0;

char SSID_WIFI[20] = {0};
char PASS_WIFI[20] = {0};

void gpio_callback(uint gpio, uint32_t events) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // signal task that a button was pressed
    xSemaphoreGiveFromISR(gpio_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

struct led_params{
    uint pin;
    uint delay;
};

void blink_task(void *param)
{
    auto lpr = (led_params *) param;
    const uint led_pin = lpr->pin;
    const uint delay = pdMS_TO_TICKS(lpr->delay);
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    while (true) {
        gpio_put(led_pin, true);
        vTaskDelay(delay);
        gpio_put(led_pin, false);
        vTaskDelay(delay);
    }
}

void gpio_task(void *param) {
    (void) param;
    const uint button_pin = 9;
    const uint led_pin = 22;
    const uint delay = pdMS_TO_TICKS(250);
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    gpio_init(button_pin);
    gpio_set_dir(button_pin, GPIO_IN);
    gpio_set_pulls(button_pin, true, false);
    gpio_set_irq_enabled_with_callback(button_pin, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    while(true) {
        if(xSemaphoreTake(gpio_sem, portMAX_DELAY) == pdTRUE) {
            //std::cout << "button event\n";
            gpio_put(led_pin, 1);
            vTaskDelay(delay);
            gpio_put(led_pin, 0);
            vTaskDelay(delay);
        }
    }
}

void serial_task(void *param)
{
    PicoOsUart u(0, 0, 1, 115200);
    Blinker blinky(20);
    uint8_t buffer[64];
    std::string line;
    while (true) {
        if(int count = u.read(buffer, 63, 30); count > 0) {
            u.write(buffer, count);
            buffer[count] = '\0';
            line += reinterpret_cast<const char *>(buffer);
            if(line.find_first_of("\n\r") != std::string::npos){
                u.send("\n");
                std::istringstream input(line);
                std::string cmd;
                input >> cmd;
                if(cmd == "delay") {
                    uint32_t i = 0;
                    input >> i;
                    blinky.on(i);
                }
                else if (cmd == "off") {
                    blinky.off();
                }
                line.clear();
            }
        }
    }
}

void modbus_task(void *param);
void display_task(void *param);
void eeprom_task(void *param);

#ifdef CLOUD

    #define SEND_INTERVAL 60000 // 1m
    #define RECEIVE_INTERVAL 5000 //5s

    TimerHandle_t cloudSender_T;
    TimerHandle_t cloudReceiver_T;

    extern "C" {
    bool run_tls_client_test(const uint8_t *cert, size_t cert_len, const char *server, const char *request, int timeout);
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
        //connect wifi
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

        // Create timers
        cloudSender_T = xTimerCreate("SendToCloudTimer", pdMS_TO_TICKS(SEND_INTERVAL), pdTRUE, NULL, cloudSenderCB);  // 20 seconds
        cloudReceiver_T = xTimerCreate("ReceiveFromCloudTimer", pdMS_TO_TICKS(RECEIVE_INTERVAL), pdTRUE, NULL, cloudReceiverCB);  // 2 seconds

        //start timers
        xTimerStart(cloudSender_T, 0);
        xTimerStart(cloudReceiver_T, 0);

        while(true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

#endif


#include <cstdio>
#include "ModbusClient.h"
#include "ModbusRegister.h"

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#if 0
#define UART_NR 0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define BAUD_RATE 9600
#define STOP_BITS 2 // for real system (pico simualtor also requires 2 stop bits)


#ifdef USE_MODBUS

void modbus_task(void *param) {

    const uint led_pin = 22;
    const uint button = 9;

    // Initialize LED pin
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);

    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);

    // Initialize chosen serial port
    //stdio_init_all();

    //printf("\nBoot\n");


    auto uart{std::make_shared<PicoOsUart>(UART_NR, UART_TX_PIN, UART_RX_PIN, BAUD_RATE, STOP_BITS)};
    auto rtu_client{std::make_shared<ModbusClient>(uart)};
    ModbusRegister mb_co2(rtu_client, 240, 256);
    ModbusRegister mb_rh(rtu_client, 241, 256);
    ModbusRegister mb_temp(rtu_client, 241, 257);
    ModbusRegister mb_fanSpeed(rtu_client, 1, 0);


    while (true) {

        gpio_put(led_pin, !gpio_get(led_pin)); // toggle  led
        printf("RH=%5.1f%%\n", mb_rh.read() / 10.0);
        vTaskDelay(5);
        printf("T =%5.1f%%\n", mb_temp.read() / 10.0);
        vTaskDelay(3000);

    }
}

#endif

#ifdef USE_SSD1306
    typedef enum {
        ssid_o,
        pass_o,
        save_o,
        cancel_o
    } wifi_o;
    wifi_o wifi_option;

    #include "ssd1306os.h"
    #include "screen_selection.h"


    void display_task(void *param)
    {
        auto i2cbus{std::make_shared<PicoI2C>(1, 400000)};
        auto display = std::make_shared<ssd1306os>(i2cbus);
        currentScreen screen(display);

        wifi_option = ssid_o;
        //screen.wifi(wifi_option, "Rhod", "0413113368");
        //screen.welcome();
        //screen.screenSelection(0);
        //screen.setCo2(350);
        screen.info(co2, rh, temp, speed, setpoint);
        while(true) {
            vTaskDelay(100);
        }

    }
#endif

#ifdef USE_EEPROM

#include "eeprom.h"

void eeprom_task(void *param) {
    //auto i2cbus{std::make_shared<PicoI2C>(0, BAUDRATE)};
    char credential[64] = "\0";
    uint16_t co2_sp = 1500;
    uint8_t buffer[64] = {0};

    strcpy(credential, "Rhod's wifi 2.4G");
    write_to_eeprom(SSID_ADDR, reinterpret_cast<const uint8_t *>(credential), strlen(credential)+1);
    strcpy(credential, "0413113368");
    write_to_eeprom(PASS_ADDR, reinterpret_cast<const uint8_t *>(credential), strlen(credential)+1);
    buffer[0] = static_cast<uint8_t>((co2_sp >> 8) & 0xFF);
    buffer[1] = static_cast<uint8_t>(co2_sp & 0xFF);
    write_to_eeprom(CO2_SETPOINT_ADDR, buffer, sizeof (buffer));

    read_from_eeprom(SSID_ADDR, buffer, sizeof (buffer));
    printf("%s\n", buffer);
    read_from_eeprom(PASS_ADDR, buffer, sizeof (buffer));
    printf("%s\n", buffer);
    read_from_eeprom(CO2_SETPOINT_ADDR, buffer, sizeof (buffer));
    uint16_t number = (buffer[0]<<8) | buffer[1];
    printf("%d\n", number);



    while(true) {
        vTaskDelay(100);
    }
}


#endif

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
int main()
{
    static led_params lp1 = { .pin = 20, .delay = 300 };
    stdio_init_all();
    printf("\nBoot\n");


#ifdef CLOUD
    setpoint_q = xQueueCreate(QUEUE_SIZE, sizeof (setpoint));
#endif

    gpio_sem = xSemaphoreCreateBinary();

    //xTaskCreate(blink_task, "LED_1", 256, (void *) &lp1, tskIDLE_PRIORITY + 1, nullptr);
    //xTaskCreate(gpio_task, "BUTTON", 256, (void *) nullptr, tskIDLE_PRIORITY + 1, nullptr);
    //xTaskCreate(serial_task, "UART1", 256, (void *) nullptr,
    //            tskIDLE_PRIORITY + 1, nullptr);
#ifdef USE_MODBUS
    xTaskCreate(modbus_task, "Modbus", 512, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
#endif

#ifdef USE_SSD1306

    xTaskCreate(display_task, "SSD1306", 512, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
#endif
#ifdef USE_EEPROM
    xTaskCreate(eeprom_task, "eeprom task", 512, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
#endif
#ifdef CLOUD
    xTaskCreate(tls_task, "tls task", 6000, (void *) nullptr,
                tskIDLE_PRIORITY + 1, nullptr);
#endif
    vTaskStartScheduler();

    while(true){};
}
