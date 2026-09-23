#ifndef MYWM_CONFIG_EXEC_H
#define MYWM_CONFIG_EXEC_H

#include "config.h"

/* Запускает cfg->launcher_cmd в фоне (fork + execl("/bin/sh", "-c", ...)). */
void config_spawn_launcher(const wm_config_t *cfg);

/* Запускает произвольную команду в фоне тем же способом, что и
   config_spawn_launcher() - используется для ACTION_EXEC (секция
   [exec] в config.conf). */
void config_spawn_command(const char *cmd);

/* Ищет ~/.config/QSWM/autostart (или ~/.config/QSWM/autostart.sh) и запускает его
   через sh в фоне, если файл существует. Вызывается один раз из
   wm_init() после config_load(), до входа в основной цикл событий. */
void config_run_autostart(void);

#endif
