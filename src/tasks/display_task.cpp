//
// Created by ADMIN on 10/5/2024.
//

#include "display_task.h"

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