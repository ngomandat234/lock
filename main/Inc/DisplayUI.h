#ifndef _MAIN_DISPLAYUI_
#define _MAIN_DISPLAYUI_

#include "ssd1306.h"
#include "stdint.h"

typedef struct
{
    int _Hour, _Min, _Sec;
} Time;

typedef struct
{
    char _Name[10];
    char _StudientID[10];
    // uint8_t _CardID[5];
} User;


void UI_DisplayInit(SSD1306_t *dev, int16_t _SDA, int16_t _SCL);
void UI_ManualDisplay(SSD1306_t *dev, Time now, bool reset);
void UI_CheckingUser(SSD1306_t *dev);
void UI_HelloUser(SSD1306_t *dev, User _user);
void UI_LockCommand(SSD1306_t *dev, bool lock);
// void UI_UserNotValid(SSD1306_t *dev);
// void UI_swippingCard(SSD1306_t *dev);

#endif