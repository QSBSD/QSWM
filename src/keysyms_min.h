#ifndef MYWM_KEYSYMS_MIN_H
#define MYWM_KEYSYMS_MIN_H

/* Замена <X11/keysym.h>: значения X keysym-ов протокола не меняются
   (это часть X11 core protocol, стабильная с 1980-х), поэтому их можно
   держать своим списком вместо того, чтобы тянуть в build-depends
   x11proto-dev/libx11-dev ради десятка #define. Значения сверены с
   X11/keysymdef.h. Список — ровно то подмножество, что используется в
   altTab.c/config.c/snap.c; при необходимости новой клавиши просто
   добавь сюда нужный XK_* с тем же числовым значением. */

#define XK_VoidSymbol  0xffffffU

#define XK_BackSpace   0xff08U
#define XK_Tab         0xff09U
#define XK_Return      0xff0dU
#define XK_Escape      0xff1bU
#define XK_Delete      0xffffU

#define XK_Left        0xff51U
#define XK_Up          0xff52U
#define XK_Right       0xff53U
#define XK_Down        0xff54U

#define XK_F1          0xffbeU
#define XK_F11         0xffc8U

#define XK_Print       0xff61U

#define XK_Num_Lock    0xff7fU
#define XK_Alt_L       0xffe9U
#define XK_Alt_R       0xffeaU

#define XK_space       0x0020U
#define XK_0           0x0030U
#define XK_a           0x0061U
#define XK_d           0x0064U
#define XK_f           0x0066U
#define XK_q           0x0071U

#endif
