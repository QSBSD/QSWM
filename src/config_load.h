#ifndef MYWM_CONFIG_LOAD_H
#define MYWM_CONFIG_LOAD_H

#include "config.h"

/* Загружает и парсит config.conf. path == NULL -> "~/.config/QSWM/config.conf".
   Сначала применяются значения по умолчанию, затем - то, что удалось
   разобрать из файла. Возвращает 0 при успехе чтения файла, -1 если файл
   не найден/не открылся (в этом случае используются только значения по
   умолчанию). */
int config_load(const char *path, wm_config_t *cfg);

#endif
