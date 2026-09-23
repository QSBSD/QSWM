#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "config_exec.h"

void config_spawn_command(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127); /* execl не вернулся - команда не найдена */
    }
    /* родитель не ждёт запущенный процесс: сирота подхватывается init'ом */
}

void config_spawn_launcher(const wm_config_t *cfg) {
    config_spawn_command(cfg->launcher_cmd);
}

/* Ищет autostart-скрипт в ~/.config/QSWM и запускает его через sh, если
   найден. Проверяются по очереди "autostart" и "autostart.sh"; файлу не
   нужен бит +x — он передаётся интерпретатору напрямую, как и launcher_cmd. */
void config_run_autostart(void) {
    const char *home = getenv("HOME");
    if (!home) return;

    static const char *candidates[] = { "autostart", "autostart.sh" };
    char path[512];

    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        int n = snprintf(path, sizeof(path), "%s/.config/QSWM/%s", home, candidates[i]);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
        if (access(path, F_OK) != 0) continue; /* нет такого файла - пробуем следующий */

        pid_t pid = fork();
        if (pid < 0) return;
        if (pid == 0) {
            setsid();
            execl("/bin/sh", "sh", path, (char *)NULL);
            _exit(127); /* execl не вернулся - интерпретатор не найден */
        }
        return; /* запустили первый найденный, второй кандидат не нужен */
    }
}
