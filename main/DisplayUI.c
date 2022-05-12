#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DisplayUI.h"
#include "esp_log.h"


char* Int2Char(int num)
{
    char *result = malloc(16);

    if (num < 10) 
    {
        snprintf(result, 16 , "0%d", num);
    }
    else
    {
        snprintf(result, 16 , "%d", num);
    }
    
    return result;
}

void UI_DisplayInit(SSD1306_t *dev, int16_t _SDA, int16_t _SCL)
{
    i2c_master_init(dev, _SDA, _SCL, -1);
	ssd1306_init(dev, 132, 64);
    ssd1306_clear_screen(dev, false);
	ssd1306_contrast(dev, 0xff);
}

void UI_ManualDisplay(SSD1306_t *dev, Time now, bool reset)
{
    char* hour = Int2Char(now._Hour);
    char* min = Int2Char(now._Min);
    char* sec = Int2Char(now._Sec);
    char* time = malloc(128);
    snprintf(time, 128 , "     %s::%s     ", hour, min);
    if (reset){
        ssd1306_clear_screen(dev, false);
        ssd1306_contrast(dev, 0xff);
        ssd1306_display_text(dev, 0, "----------------", 16, false);
        ssd1306_display_text(dev, 1, "| >CEEC--DOOR< |", 16, true);
        ssd1306_display_text(dev, 2, "----------------", 16, false);
        ssd1306_display_text(dev, 6, "----------------", 16, false);
        ssd1306_display_text(dev, 7, ">>>>>>>\\/<<<<<<<", 16, true);
    }  
    ssd1306_display_text(dev, 4, time, 16, false);
    free(hour);
    free(min);
    free(sec);
    free(time);
}

void UI_CheckingUser(SSD1306_t *dev)
{
    ssd1306_clear_screen(dev, false);
    ssd1306_contrast(dev, 0xff);
    // ssd1306_display_text(dev, 3, "----------------", 16, false);
    // ssd1306_display_text(dev, 4, "| > CHECK < |", 16, true);
    // ssd1306_display_text(dev, 5, "----------------", 16, false);
     ssd1306_display_text(dev, 3, "----------------", 16, false);
    ssd1306_display_text(dev, 4, " CHECKING ", 16, false);
    ssd1306_display_text(dev, 5, "----------------", 16, false);
}
//  void UI_UserNotValid(SSD1306_t *dev)
// {
//     ssd1306_clear_screen(dev, false);
//     ssd1306_contrast(dev, 0xff);
//      ssd1306_display_text(dev, 3, "----------------", 16, false);
//     ssd1306_display_text(dev, 4, " User is not valid ", 16, false);
//     ssd1306_display_text(dev, 5, "----------------", 16, false);
// }

// void UI_swippingCard(SSD1306_t *dev)
// {
//      ssd1306_clear_screen(dev, false);
//     ssd1306_contrast(dev, 0xff);
//      ssd1306_display_text(dev, 3, "----------------", 16, false);
//     ssd1306_display_text(dev, 4, " Successfully ", 16, false);
//     ssd1306_display_text(dev, 5, "----------------", 16, false);
// }

void UI_HelloUser(SSD1306_t *dev, User _user)
{
    char name[20];
    char studientID[20];
    snprintf(name, 40 , ":=> %s ", _user._Name);
    snprintf(studientID, 64 , ":=> %s ", _user._StudientID);
   // ssd1306_clear_screen(dev, false);
    ssd1306_contrast(dev, 0xff);
    ssd1306_display_text(dev, 0, "----------------", 16, false);
    ssd1306_display_text(dev, 3, name, strlen(name), false);
    ssd1306_display_text(dev, 4, studientID, strlen(studientID), false);
}

void UI_LockCommand(SSD1306_t *dev, bool lock)
{
    ssd1306_clear_screen(dev, false);
    ssd1306_contrast(dev, 0xff);
    if (lock) {
        ssd1306_display_text(dev, 0, "----------------", 16, true);
        ssd1306_display_text(dev, 4, "  > LOCKING <  ", 16, false);
        ssd1306_display_text(dev, 7, "----------------", 16, true);
    }
    else {
        ssd1306_display_text(dev, 0, "----------------", 16, true);
        ssd1306_display_text(dev, 4, "  > OPENNING <  ", 16, false);
        ssd1306_display_text(dev, 7, "----------------", 16, true);
    }
}