#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <ncurses.h>
// The reason I added delay for not overheat and smoother performance 128ms not bad
#define STEP 0.05          // 5% step for left/right arrows
#define MIN_BRIGHT 0.1     // 10% minimum floor
#define MAX_BRIGHT 2.0     // 200% maximum ceiling
#define DELAY_MS 128       // 500ms debounced hardware write delay
#ifndef VERSION
#define VERSION "2026/00"
#endif
#define GITHUB_URL "https://github.com/HalanoSiblee/xorg-brightness-adjuster-tui/"

// Apply brightness via Xrandr CRTC gamma ramps
static void set_x11_brightness(Display *dpy, double brightness) {
    Window root = DefaultRootWindow(dpy);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);
    if (!res) return;

    for (int i = 0; i < res->ncrtc; i++) {
        RRCrtc crtc = res->crtcs[i];
        int size = XRRGetCrtcGammaSize(dpy, crtc);
        if (size <= 0) continue;

        XRRCrtcGamma *gamma = XRRAllocGamma(size);
        if (!gamma) continue;

        for (int j = 0; j < size; j++) {
            double value = (j / (double)(size - 1)) * brightness;
            if (value > 1.0) value = 1.0;
            if (value < 0.0) value = 0.0;

            unsigned short val = (unsigned short)(value * 65535.0);
            gamma->red[j]   = val;
            gamma->green[j] = val;
            gamma->blue[j]  = val;
        }

        XRRSetCrtcGamma(dpy, crtc, gamma);
        XRRFreeGamma(gamma);
    }
    XRRFreeScreenResources(res);
    XFlush(dpy);
}

static inline long long get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL);
}

// Clean, centered normal text display
static void draw_ui(double brightness, const char *status) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    clear();

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "Brightness: %.0f%%", brightness * 100.0);
    int start_x = (max_x - len) / 2;
    int start_y = max_y / 2;

    if (start_x < 0) start_x = 0;

    // Centered brightness text
    mvprintw(start_y, start_x, "%s", buf);

    // Centered status text below it
    int status_len = snprintf(buf, sizeof(buf), "Status: %s", status);
    int status_x = (max_x - status_len) / 2;
    if (status_x < 0) status_x = 0;
    mvprintw(start_y + 2, status_x, "%s", buf);

    refresh();
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("\033]8;;%s\033\\\033[1m\033[3mxorg-brightness-adjuster-tui\033[0m\033]8;;\033\\\n", GITHUB_URL);
            printf("Version: %s\n", VERSION);
            return EXIT_SUCCESS;
        }
    }
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Error: Cannot open X Display.\n");
        return EXIT_FAILURE;
    }

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    set_escdelay(25);  // Instant ESC response
    timeout(20);       // Non-blocking 20ms poll

    double brightness = 1.0;
    bool running = true;
    bool pending_change = false;
    long long last_keypress_time = 0;

    draw_ui(brightness, "Ready");

    while (running) {
        int ch = getch();

        if (ch != ERR) {
            bool handled = false;

            if (ch >= '1' && ch <= '9') {
                brightness = (ch - '0') * 0.1;
                handled = true;
            } else {
                switch (ch) {
                    case KEY_LEFT:
                        brightness -= STEP;
                        if (brightness < MIN_BRIGHT) brightness = MIN_BRIGHT;
                        handled = true;
                        break;

                    case KEY_RIGHT:
                        brightness += STEP;
                        if (brightness > MAX_BRIGHT) brightness = MAX_BRIGHT;
                        handled = true;
                        break;

                    case ' ':
                    case '0':
                        brightness = 1.0;
                        handled = true;
                        break;

                    case 'q':
                    case 'Q':
                    case 27: // Instant ESC exit
                        running = false;
                        break;
                }
            }

            if (handled) {
                pending_change = true;
                last_keypress_time = get_time_ms();
                draw_ui(brightness, "Adjusting...");
            }
        }

        // Apply hardware change non-blockingly after 128ms of inactivity
        if (pending_change && (get_time_ms() - last_keypress_time >= DELAY_MS)) {
            set_x11_brightness(dpy, brightness);
            pending_change = false;
            draw_ui(brightness, "Applied!");
        }
    }

    // Instant exit flush: apply pending hardware brightness immediately before exit
    if (pending_change) {
        set_x11_brightness(dpy, brightness);
    }

    endwin();
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}