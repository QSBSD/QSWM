#include <string.h>
#include <xcb/xcb.h>
#include "keysyms_min.h"
#include "config_defaults.h"
#include "altTab_render.h"

void config_defaults(wm_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->titlebar_height   = 24;
    cfg->button_size       = 16;
    cfg->alttab_icon_size  = ALTTAB_CELL_SIZE_DEFAULT;
    cfg->icon_left         = 1;
    /* Полностью чёрный по умолчанию (не серый) — переопределяется
       [appearance].titlebar_color / alttab_bg_color в config.conf. */
    cfg->titlebar_color_r = cfg->titlebar_color_g = cfg->titlebar_color_b = 0.0;
    cfg->alttab_bg_r = cfg->alttab_bg_g = cfg->alttab_bg_b = 0.0;
    strncpy(cfg->alttab_position, "center", sizeof(cfg->alttab_position) - 1);
    strncpy(cfg->launcher_cmd, "dmenu_run", sizeof(cfg->launcher_cmd) - 1);

    /* биндинги по умолчанию соответствуют примеру из config.conf */
    cfg->binding_count = 0;
    keybinding_t defs[] = {
        { XCB_MOD_MASK_1, XK_Tab,   0, ACTION_ALTTAB   },
        { XCB_MOD_MASK_1, XK_q,     0, ACTION_CLOSE    },
        { XCB_MOD_MASK_1, XK_f,     0, ACTION_MAXIMIZE },
        { XCB_MOD_MASK_1, XK_d,     0, ACTION_MINIMIZE },
        { XCB_MOD_MASK_1, XK_space, 0, ACTION_LAUNCHER },
        { 0,              XK_F11,   0, ACTION_FULLSCREEN },
    };
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
        cfg->bindings[cfg->binding_count++] = defs[i];
}
