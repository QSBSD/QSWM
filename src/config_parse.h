#ifndef MYWM_CONFIG_PARSE_H
#define MYWM_CONFIG_PARSE_H

#include <xcb/xcb.h>

/* trim() и streq_ci() используются и разбором комбинаций клавиш
   (config_parse.c), и самим config_load() (config.c) при разборе INI. */
char *config_trim(char *s);
int   config_streq_ci(const char *a, const char *b);

/* Имя модификатора ("alt"/"shift"/"control"/"super"/...) -> маска
   XCB_MOD_MASK_*. Возвращает 0, если имя не распознано (используется и
   в разборе комбинаций, и напрямую для директивы "mod = Alt"). */
uint16_t config_modmask_from_name(const char *name);

/* Разбирает значение вида "mod+q" / "Tab" / "shift+mod+f" в модификаторы
   и keysym. Токен "mod" разворачивается в mod_mask, переданный из
   секции [hotkeys] (значение "mod = Alt"). Возвращает -1, если keysym
   не распознан. */
int config_parse_combo(const char *value, uint16_t mod_mask,
                        uint16_t *out_mods, xcb_keysym_t *out_keysym);

/* Разбирает цвет вида "#RRGGBB" (решётка не обязательна) в компоненты
   [0.0, 1.0] для xrender_color_rgba(). Возвращает -1, если строка не
   похожа на 6-значный hex-цвет (значение при этом не трогается —
   остаётся действующий дефолт). */
int config_parse_hex_color(const char *val, double *out_r, double *out_g, double *out_b);

#endif
