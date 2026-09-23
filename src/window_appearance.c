#include "window.h"
#include "xrender_util.h"

/* Текущие значения [appearance] из config.conf (пункт 6). Заполняются
   window_set_appearance()/window_set_titlebar_color() при старте WM; до
   их вызова действуют значения по умолчанию из window.h (цвет — чёрный,
   а не серый). */
static struct {
    uint16_t titlebar_height;
    uint16_t button_size;
    int      icon_left;
    double   color_r, color_g, color_b;
} appearance = { TITLEBAR_HEIGHT_DEFAULT, BUTTON_SIZE_DEFAULT, 1, 0.0, 0.0, 0.0 };

void window_set_appearance(uint16_t titlebar_height, uint16_t button_size, int icon_left) {
    if (titlebar_height > 0) appearance.titlebar_height = titlebar_height;
    if (button_size > 0)     appearance.button_size = button_size;
    appearance.icon_left = icon_left;
}

void window_set_titlebar_color(double r, double g, double b) {
    appearance.color_r = r;
    appearance.color_g = g;
    appearance.color_b = b;
}

uint16_t window_titlebar_height(void) { return appearance.titlebar_height; }
uint16_t window_button_size(void)     { return appearance.button_size; }
int      window_icon_left(void)       { return appearance.icon_left; }

xcb_render_color_t window_titlebar_color(void) {
    return xrender_color_rgba(appearance.color_r, appearance.color_g, appearance.color_b, 1.0);
}
