//
// Created by lsk on 6/27/25.
//

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * Well I have to admit that I did a bad job when creating module display and hardware, they should be either merged,
 * or be separated into more modules, but, well, I'm lazy to fix this issue, and since the code will work no matter how
 * I arrange these modules, and the readability won't be influenced too much with enough comments, so just let it go ;)
 *
 * Hope one day I or someone else would fix it...
 *
 * Module display: code for OLED display, and for WS2812.
 * Module hardware: code for I2S devices (microphone and amplifier), and buttons.
 * These modules should only provide a wrap for GPIO and bus operations. Their logic should be implemented in main.c .
 */

/*
 * ---------- OLED FUNCTIONS ----------
*/

/**
 * OLED Initialization
 */
void display_oled_init();

/**
 * Clear the OLED
 */
void display_oled_clear();

/**
 * Show text on the OLED
 * @param text The text to be shown
 */
void display_oled_show_text(char* text);

/*
 * ---------- WS2812 FUNCTIONS ----------
 */

/**
 * WS2812 LED Initialization
 */
void display_rgb_init();

/**
 * WS2812 LED Set Color And Brightness
 * @param r Red
 * @param g Green
 * @param b Blue
 * @param brightness Brightness
 */
void display_rgb_set(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);

#endif //DISPLAY_H
