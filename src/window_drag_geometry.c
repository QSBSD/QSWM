#include <xcb/xcb.h>
#include "window.h"
#include "window_drag.h"

/* Прямоугольники трёх кнопок в системе координат frame, порядок слева
   направо всегда − ▢ ×. Блок кнопок анкерится к правому краю titlebar,
   если иконка приложения слева (icon_left=true, значение по умолчанию),
   и к левому краю, если иконка справа (icon_left=false из [appearance],
   пункт 6) — иначе иконка и кнопки накладывались бы друг на друга. */
void window_drag_button_rects(uint16_t frame_width, xcb_rectangle_t *close,
                               xcb_rectangle_t *maximize, xcb_rectangle_t *minimize) {
    uint16_t bs = window_button_size();
    int16_t  y  = (int16_t)((window_titlebar_height() - bs) / 2);

    if (window_icon_left()) {
        int16_t x = (int16_t)frame_width - BUTTON_MARGIN - bs;
        close->x = x; close->y = y; close->width = bs; close->height = bs;
        x = (int16_t)(x - (bs + BUTTON_GAP));
        maximize->x = x; maximize->y = y; maximize->width = bs; maximize->height = bs;
        x = (int16_t)(x - (bs + BUTTON_GAP));
        minimize->x = x; minimize->y = y; minimize->width = bs; minimize->height = bs;
    } else {
        int16_t x = BUTTON_MARGIN;
        minimize->x = x; minimize->y = y; minimize->width = bs; minimize->height = bs;
        x = (int16_t)(x + bs + BUTTON_GAP);
        maximize->x = x; maximize->y = y; maximize->width = bs; maximize->height = bs;
        x = (int16_t)(x + bs + BUTTON_GAP);
        close->x = x; close->y = y; close->width = bs; close->height = bs;
    }
}
