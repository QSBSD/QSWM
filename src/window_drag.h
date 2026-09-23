#ifndef MYWM_WINDOW_DRAG_H
#define MYWM_WINDOW_DRAG_H

#include <xcb/xcb.h>
#include "window.h"

/* Прямоугольники трёх кнопок декорации (− ▢ ×) в системе координат
   frame, в порядке слева направо. Анкерятся к правому краю titlebar при
   icon_left=true (по умолчанию) и к левому при icon_left=false. Общая
   геометрия нужна и отрисовке декорации (window.c), и обработке кликов
   по кнопкам (window_drag.c). */
void window_drag_button_rects(uint16_t frame_width, xcb_rectangle_t *close,
                               xcb_rectangle_t *maximize, xcb_rectangle_t *minimize);

/* window_button_press()/window_motion_notify()/window_button_release() —
   публичный API, объявлен в window.h и реализован в window_drag.c. */

/* Вызывается из window_unmanage()/window_withdraw() ДО free(c). Если
   removed_client сейчас участвует в активном move/resize (пользователь
   тащит окно мышью, а хоткей close/kill убил его в этот момент —
   указатель захвачен, но клавиатура нет), обнуляет g_window_drag,
   чтобы последующие XCB_MOTION_NOTIFY/XCB_BUTTON_RELEASE не разыменовывали
   уже освобождённый client_t (use-after-free). Аналог snap_client_removed()
   в snap.c. Если drag был активен, дополнительно снимает active pointer
   grab (xcb_ungrab_pointer) — иначе указатель останется захваченным
   навсегда, т.к. дошедший позже XCB_BUTTON_RELEASE увидит mode==DRAG_NONE
   и не вызовет xcb_ungrab_pointer сам. */
void window_drag_client_removed(xcb_connection_t *conn, client_t *removed_client);

#endif
