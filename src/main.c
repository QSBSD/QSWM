#define _POSIX_C_SOURCE 200809L /* sigaction()/pipe2() требуют POSIX при -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <xcb/xcb.h>
#include "wm.h"

/* SIGTERM/SIGINT раньше не обрабатывались вообще - процесс просто убивался
   ядром без единого шанса завершиться штатно. Сам обработчик сигнала, однако,
   не должен звать НИЧЕГО, кроме async-signal-safe функций (POSIX.1-2001,
   signal-safety(7)) - раньше здесь напрямую вызывался xcb_disconnect(), а он
   внутри делает free() на буферах соединения; если сигнал придёт в момент,
   когда основной поток держит внутреннюю блокировку malloc (она не
   реентерабельна), это дедлок или порча кучи, а не просто "some cleanup".
   Поэтому обработчик только пишет один байт в pipe (write() - async-signal-
   safe) - самый обычный self-pipe trick. wm_run() слушает читающий конец
   этого pipe через poll() наравне с файловым дескриптором X-соединения:
   простой sig_atomic_t-флаг тут не подошёл бы, потому что xcb сама
   перезапускает read()/poll() при EINTR, и xcb_wait_for_event() в этом
   случае никогда не проснулась бы от сигнала, пока не придёт следующее
   X-событие. */
static int g_exit_pipe_write = -1;

static void handle_term_signal(int sig) {
    (void)sig;
    if (g_exit_pipe_write >= 0) {
        char byte = 0;
        ssize_t ignored = write(g_exit_pipe_write, &byte, 1);
        (void)ignored; /* нечего делать с ошибкой записи в обработчике сигнала */
    }
}

static int install_signal_handlers(void) {
    int fds[2];
    if (pipe(fds) != 0) return -1;

    /* O_NONBLOCK на записывающем конце - если обработчик сигнала успеет
       выполниться дважды подряд до того, как wm_run() вычитает pipe (не
       должно происходить на практике, SIGTERM/SIGINT обычно шлют один раз),
       write() не должен заблокироваться внутри обработчика сигнала. */
    fcntl(fds[1], F_SETFL, O_NONBLOCK);
    g_exit_pipe_write = fds[1];

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_term_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    return fds[0]; /* читающий конец - отдаём wm_run() */
}

int main(void) {
    int screen_num;
    xcb_connection_t *conn = xcb_connect(NULL, &screen_num);

    if (xcb_connection_has_error(conn)) {
        fprintf(stderr, "QSWM: не удалось подключиться к X-серверу\n");
        return EXIT_FAILURE;
    }

    int exit_fd = install_signal_handlers();
    if (exit_fd < 0) {
        fprintf(stderr, "QSWM: не удалось создать pipe для сигналов\n");
        xcb_disconnect(conn);
        return EXIT_FAILURE;
    }

    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; i++)
        xcb_screen_next(&iter);
    xcb_screen_t *screen = iter.data;

    if (wm_init(conn, screen) != 0) {
        fprintf(stderr, "QSWM: root-окно уже занято другим WM\n");
        xcb_disconnect(conn);
        return EXIT_FAILURE;
    }

    wm_run(conn, screen, exit_fd);

    wm_cleanup(conn);
    xcb_disconnect(conn);
    return EXIT_SUCCESS;
}
