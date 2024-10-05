#include "screen_selection.h"
#include "icons.h"
#include "cstring"
#include <cstdio>

using namespace std;

void currentScreen::welcome() {
    lcd->fill(0);
    lcd->text("GROUP 03",30,20);
    lcd->text("Mong, Xuan, Sami",0,40);
    lcd->show();
}

void currentScreen::screenSelection(const int option){
    color0 = 1;
    color1 = 1;
    color2 = 1;
    lcd->fill(0);
    switch (option) {
        case 0:
            color0 = 0;
            lcd->fill_rect(0, 4, 128, 14, 1);
            break;
        case 1:
            color2 = 0;
            lcd->fill_rect(0, 25, 128, 14, 1);
            break;
        case 2:
            color3 = 0;
            lcd->fill_rect(0, 46, 128, 14, 1);
            break;
        default:
            break;
    }
    lcd->text("SET CO2 LEVEL", 5, 7, color0);
    lcd->text("SHOW STATUS", 5, 28, color2);
    lcd->text("WIFI CONFIG", 5, 49, color3);
    lcd->show();
}

void currentScreen::setCo2(const int val){
    lcd->fill(0);
    lcd->text("SET CO2 LEVEL", 10, 10);
    text = to_string(val) + " ppm";
    lcd->text(text, 35, 35);
    lcd->show();
}

void currentScreen::info(const int c, const int t, const int h, const int s, const int sp) {
    lcd->fill(0);
    mono_vlsb icon(info_icon, 20, 20);
    lcd->blit(icon, 0, 0);
    text = "CO2:" + to_string(c) + " ppm";
    lcd->text(text, 30, 2);
    text = "RH:" + to_string(h) + " %";
    lcd->text(text, 38, 2+13);
    text = "T:" + to_string(t) + " C";
    lcd->text(text, 46, 2+13+13);
    text = "S:" + to_string(s) + " %";
    lcd->text(text, 46,2+13+13+13);
    text = "SP:" + to_string(sp) + " ppm";
    lcd->text(text, 38, 2+13+13+13+13);
    lcd->show();
}

void currentScreen::wifi(const int option, const char* ssid, const char* pw) {
    color0 = 1;
    color1 = 1;
    lcd->fill(0);
    switch (option) {
        case 0:
            lcd->rect(0,0,128,18,1);
            break;
        case 1:
            lcd->rect(0,20,128,18,1);
            break;
        case 2:
            lcd->fill_rect(0,51,36,12,1);
            color0 = 0;
            break;
        case 3:
            lcd->fill_rect(75,51,52,12,1);
            color1 = 0;
            break;
        default:
            break;
    }
    lcd->text("SSID:", 2,5);
    lcd->text(ssid, 44, 5);
    lcd->text("PASS:",2,25);
    lcd->text(pw, 44, 25);
    lcd->rect(0,51,36,12,1);
    lcd->text("save",2,53, color0);
    lcd->rect(75,51,52,12,1);
    lcd->text("cancel",77,53, color1);
    lcd->show();
}

void currentScreen::asciiCharSelection(const int posX, const int option, const int asciiChar){
    int posY = 64;
    char c[2];
    sprintf(c, "%c", asciiChar);
    switch (option) {
        case 0:
            posY = 5;
            break;
        case 1:
            posY = 25;
            break;
        default:
            break;
    }
    if (asciiChar < START_ASCII){
        lcd->fill_rect(posX,posY,8,8,1);
    } else {
        lcd->fill_rect(posX,posY,8,8,0);
        lcd->text(c, posX, posY);
    }
    lcd->show();
}

void currentScreen::askRestart(){
    lcd->fill(0);
    lcd->text("*Reset device*", 0, 30);
    lcd->show();
}