#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <xcb/xcb.h>
#include "keysyms_min.h"
#include "config_parse.h"

char *config_trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

int config_streq_ci(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

/* Имя модификатора -> маска XCB_MOD_MASK_*. Возвращает 0, если это не
   модификатор (значит токен - имя клавиши). */
uint16_t config_modmask_from_name(const char *name) {
    if (config_streq_ci(name, "alt") || config_streq_ci(name, "mod1"))       return XCB_MOD_MASK_1;
    if (config_streq_ci(name, "shift"))                                      return XCB_MOD_MASK_SHIFT;
    if (config_streq_ci(name, "control") || config_streq_ci(name, "ctrl"))   return XCB_MOD_MASK_CONTROL;
    if (config_streq_ci(name, "super") || config_streq_ci(name, "mod4") ||
        config_streq_ci(name, "win"))                                        return XCB_MOD_MASK_4;
    return 0;
}

/* Имя клавиши ("q", "space", "Tab", "F1", ...) -> keysym. Покрывает
   набор клавиш, встречающийся в config.conf. Константы XK_* — из
   keysyms_min.h (свой список вместо X11/keysym.h, см. этот файл). */
static xcb_keysym_t keysym_from_name(const char *name) {
    if (config_streq_ci(name, "tab"))               return XK_Tab;
    if (config_streq_ci(name, "space"))             return XK_space;
    if (config_streq_ci(name, "escape") || config_streq_ci(name, "esc")) return XK_Escape;
    if (config_streq_ci(name, "return") || config_streq_ci(name, "enter")) return XK_Return;
    if (config_streq_ci(name, "backspace"))         return XK_BackSpace;
    if (config_streq_ci(name, "delete"))            return XK_Delete;
    if (config_streq_ci(name, "print"))             return XK_Print;
    if (config_streq_ci(name, "left"))              return XK_Left;
    if (config_streq_ci(name, "right"))             return XK_Right;
    if (config_streq_ci(name, "up"))                return XK_Up;
    if (config_streq_ci(name, "down"))              return XK_Down;

    if (tolower((unsigned char)name[0]) == 'f' &&
        (strlen(name) == 2 || strlen(name) == 3) &&
        isdigit((unsigned char)name[1]) &&
        (strlen(name) == 2 || isdigit((unsigned char)name[2]))) {
        int n = atoi(name + 1);
        if (n >= 1 && n <= 12) return (xcb_keysym_t)(XK_F1 + (n - 1));
    }

    if (strlen(name) == 1) {
        char c = (char)tolower((unsigned char)name[0]);
        if (c >= 'a' && c <= 'z') return (xcb_keysym_t)(XK_a + (c - 'a'));
        if (c >= '0' && c <= '9') return (xcb_keysym_t)(XK_0 + (c - '0'));
    }

    return XK_VoidSymbol;
}

int config_parse_combo(const char *value, uint16_t mod_mask,
                        uint16_t *out_mods, xcb_keysym_t *out_keysym) {
    /* 256 с запасом хватает на любую реальную комбинацию ("mod+shift+f"
       и т.п.); если значение всё же длиннее - это заведомо некорректная
       строка конфига, а не подлежащая молчаливой обрезке комбинация. */
    char buf[256];
    if (strlen(value) >= sizeof(buf)) return -1;
    strncpy(buf, value, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    uint16_t mods = 0;
    xcb_keysym_t keysym = XK_VoidSymbol;

    char *tok = strtok(buf, "+");
    while (tok) {
        char *t = config_trim(tok);
        if (config_streq_ci(t, "mod")) {
            mods |= mod_mask;
        } else {
            uint16_t m = config_modmask_from_name(t);
            if (m) {
                mods |= m;
            } else {
                keysym = keysym_from_name(t);
            }
        }
        tok = strtok(NULL, "+");
    }

    if (keysym == XK_VoidSymbol) return -1;
    *out_mods = mods;
    *out_keysym = keysym;
    return 0;
}

int config_parse_hex_color(const char *val, double *out_r, double *out_g, double *out_b) {
    const char *s = val;
    if (*s == '#') s++;
    if (strlen(s) != 6) return -1;
    for (int i = 0; i < 6; i++)
        if (!isxdigit((unsigned char)s[i])) return -1;

    char buf[3] = {0};
    buf[0] = s[0]; buf[1] = s[1];
    long rv = strtol(buf, NULL, 16);
    buf[0] = s[2]; buf[1] = s[3];
    long gv = strtol(buf, NULL, 16);
    buf[0] = s[4]; buf[1] = s[5];
    long bv = strtol(buf, NULL, 16);

    *out_r = (double)rv / 255.0;
    *out_g = (double)gv / 255.0;
    *out_b = (double)bv / 255.0;
    return 0;
}
