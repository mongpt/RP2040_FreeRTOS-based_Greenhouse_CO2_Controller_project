
#ifndef SCREEN_SELECTION_H
#define SCREEN_SELECTION_H
#include <cstring>
#include "ssd1306os.h"

#define START_ASCII 32
#define END_ASCII 126
#define CHAR_WIDTH 8;

using namespace std;

class currentScreen{
public:
    explicit currentScreen(const std::shared_ptr<ssd1306os> &lcd)
    : lcd(lcd), color0(1),color1(1), color2(1), color3(1){};
    void welcome();
    void screenSelection(int option);
    void setCo2(int val);
    void info(int c, int t, int h, int s, int sp);
    void wifi(int option, const char* ssid, const char* pw);
    void asciiCharSelection(int posX, int option, int asciiChar);
    void askRestart();
private:
    std::shared_ptr<ssd1306os> lcd;
    int color0, color1, color2, color3;
    string text;
};

#endif