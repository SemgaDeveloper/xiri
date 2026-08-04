#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>
#include <vector>
#include <algorithm>
#include <csignal>

/* state */
static std::vector<xcb_window_t> clients;
static size_t focusedIndex = 0;

static xcb_connection_t   *connection;
static xcb_screen_t       *screen;
static xcb_key_symbols_t  *keysyms;

static xcb_keycode_t keyTab, keyEnter, keyQ;
static xcb_timestamp_t lastSpawnTime = 0;
static xcb_timestamp_t lastSwitchTime = 0;

/* lock modifiers that must be grabbed in every combination, or grabs
   silently fail to match whenever numlock/capslock is on */
static const uint16_t lockMasks[4] = {
    0,
    XCB_MOD_MASK_LOCK,                    /* CapsLock */
    XCB_MOD_MASK_2,                       /* NumLock (common mapping) */
    XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
};

/* helpers */

static void spawn(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        exit(1);
    }
}

static void monocleResize(xcb_window_t win) {
    uint32_t values[4] = {0, 0, screen->width_in_pixels, screen->height_in_pixels};
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    xcb_configure_window(connection, win, mask, values);
}

static void applyMonocleAll() {
    /* only focused window needs to be full size + raised;
       others stay mapped underneath */
    for (auto w : clients) monocleResize(w);
}

static void focusClient(size_t idx) {
    if (clients.empty()) return;
    focusedIndex = idx % clients.size();
    xcb_window_t win = clients[focusedIndex];

    monocleResize(win);

    uint32_t stackMode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_STACK_MODE, &stackMode);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
    xcb_flush(connection);
}

static void killFocused() {
    if (clients.empty()) return;
    xcb_kill_client(connection, clients[focusedIndex]);
    xcb_flush(connection);
}

static xcb_keycode_t firstKeycode(xcb_keysym_t sym) {
    xcb_keycode_t *kc = xcb_key_symbols_get_keycode(keysyms, sym);
    xcb_keycode_t result = kc ? kc[0] : 0;
    if (kc) free(kc);
    return result;
}

static void grabKey(uint16_t modifiers, xcb_keycode_t code) {
    for (uint16_t lock : lockMasks) {
        xcb_grab_key(connection, 1, screen->root, modifiers | lock, code,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
}

static void grabKeys() {
    keyTab   = firstKeycode(0xff09); /* XK_Tab */
    keyEnter = firstKeycode(0xff0d); /* XK_Return */
    keyQ     = firstKeycode(0x0071); /* XK_q */

    grabKey(XCB_MOD_MASK_4, keyTab);
    grabKey(XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT, keyTab);
    grabKey(XCB_MOD_MASK_4, keyEnter);
    grabKey(XCB_MOD_MASK_4, keyQ);
}

/* event handlers */

static void onMapRequest(xcb_generic_event_t *event) {
    xcb_map_request_event_t *mr = (xcb_map_request_event_t *)event;
    clients.push_back(mr->window);

    uint32_t mask = XCB_CW_EVENT_MASK;
    uint32_t values = XCB_EVENT_MASK_ENTER_WINDOW;
    xcb_change_window_attributes(connection, mr->window, mask, &values);

    xcb_map_window(connection, mr->window);
    focusClient(clients.size() - 1);
}

static void onConfigureRequest(xcb_generic_event_t *event) {
    /* honor request but it'll be overridden to fullscreen on map/focus */
    xcb_configure_request_event_t *cr = (xcb_configure_request_event_t *)event;
    uint32_t values[7];
    int i = 0;
    uint16_t mask = cr->value_mask;
    if (mask & XCB_CONFIG_WINDOW_X)            values[i++] = cr->x;
    if (mask & XCB_CONFIG_WINDOW_Y)            values[i++] = cr->y;
    if (mask & XCB_CONFIG_WINDOW_WIDTH)        values[i++] = cr->width;
    if (mask & XCB_CONFIG_WINDOW_HEIGHT)       values[i++] = cr->height;
    if (mask & XCB_CONFIG_WINDOW_BORDER_WIDTH) values[i++] = cr->border_width;
    if (mask & XCB_CONFIG_WINDOW_SIBLING)      values[i++] = cr->sibling;
    if (mask & XCB_CONFIG_WINDOW_STACK_MODE)   values[i++] = cr->stack_mode;
    xcb_configure_window(connection, cr->window, mask, values);
}

static void removeClient(xcb_window_t win) {
    clients.erase(std::remove(clients.begin(), clients.end(), win), clients.end());
    if (!clients.empty()) focusClient(0);
}

static void onDestroyNotify(xcb_generic_event_t *event) {
    removeClient(((xcb_destroy_notify_event_t *)event)->window);
}

static void onUnmapNotify(xcb_generic_event_t *event) {
    removeClient(((xcb_unmap_notify_event_t *)event)->window);
}

static void onEnterNotify(xcb_generic_event_t *event) {
    xcb_enter_notify_event_t *en = (xcb_enter_notify_event_t *)event;
    for (size_t i = 0; i < clients.size(); ++i) {
        if (clients[i] != en->event) continue;
        focusedIndex = i;
        /* only set input focus here - do NOT resize/restack.
           the window already got the crossing event because it's
           already topmost; re-raising can trigger another crossing
           event and cause a focus ping-pong loop */
        xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, en->event, XCB_CURRENT_TIME);
        xcb_flush(connection);
        break;
    }
}

static void onKeyPress(xcb_generic_event_t *event) {
    xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
    uint16_t state = kp->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2); /* strip caps/numlock */
    if (!(state & XCB_MOD_MASK_4)) return;

    if (kp->detail == keyTab) {
        if (kp->time - lastSwitchTime < 150) return;
        lastSwitchTime = kp->time;
        if (state & XCB_MOD_MASK_SHIFT)
            focusClient(focusedIndex == 0 ? clients.size() - 1 : focusedIndex - 1);
        else
            focusClient(focusedIndex + 1);
    } else if (kp->detail == keyEnter) {
        /* ignore auto-repeat floods */
        if (kp->time - lastSpawnTime < 400) return;
        lastSpawnTime = kp->time;
        spawn("xterm -fa 'DejaVu Sans Mono' -fs 12");
    } else if (kp->detail == keyQ) {
        killFocused();
    }
}

/* main */

int main() {
    signal(SIGCHLD, SIG_IGN); /* auto-reap children */

    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection)) {
        fprintf(stderr, "cannot connect to X server\n");
        return 1;
    }

    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    screen = iter.data;

    /* SubstructureRedirect fails if another WM runs */
    uint32_t rootMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                         XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
    xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(
        connection, screen->root, XCB_CW_EVENT_MASK, &rootMask);
    xcb_generic_error_t *err = xcb_request_check(connection, cookie);
    if (err) {
        fprintf(stderr, "another WM is already running\n");
        free(err);
        xcb_disconnect(connection);
        return 1;
    }

    keysyms = xcb_key_symbols_alloc(connection);
    grabKeys();
    xcb_flush(connection);

    std::cout << "minimal monocle wm started (Mod4+tab switch, Mod4+enter xterm, Mod4+q kill)\n";

    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(connection))) {
        switch (event->response_type & ~0x80) {
            case XCB_MAP_REQUEST:       onMapRequest(event);       break;
            case XCB_CONFIGURE_REQUEST: onConfigureRequest(event); break;
            case XCB_DESTROY_NOTIFY:    onDestroyNotify(event);    break;
            case XCB_UNMAP_NOTIFY:      onUnmapNotify(event);      break;
            case XCB_ENTER_NOTIFY:      onEnterNotify(event);      break;
            case XCB_KEY_PRESS:         onKeyPress(event);         break;
            default: break;
        }
        free(event);
    }

    xcb_key_symbols_free(keysyms);
    xcb_disconnect(connection);
    return 0;
}
