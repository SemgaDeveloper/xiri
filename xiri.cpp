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
// variables

/* Test config values, befoe i made special file for configurating your xiri, you can configure it there */
uint32_t customWidgth = 1280; // Change resolution what your windows will open 
uint32_t customHeight = 720; // Also custom resolution will be applied through xrandr
uint32_t windowWidgth = customWidgth - 24; // You can set there what you want, this will affect only windows
uint32_t windowHeight = customHeight - 24; // Same as previous string
const char *monitor = "eDP-1"; // Change to your monitor name if eDP-1 does not suits you
const char *terminal = "kitty"; //This terminal will be used for Mod4+t variant


/* state */
static std::vector<xcb_window_t> clients;
static size_t focusedIndex = 0;
bool fullscreen = false;

static xcb_connection_t   *connection;
static xcb_screen_t       *screen;
static xcb_key_symbols_t  *keysyms;

static xcb_keycode_t keyTab, keyEnter, keyQ, keyE, keyB, keyD, keyT, keyF, keyLeft, keyRight;
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
  if (fullscreen == false) {
    uint32_t values[4] = {12, 12, windowWidgth, windowHeight};
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    xcb_configure_window (connection, win, mask, values);
  } else {
    uint32_t values[4] = {0, 0, customWidgth, customHeight};
    uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    xcb_configure_window (connection, win, mask, values);
  }
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

static void refocusCleint() {
    if (clients.empty()) return;
    xcb_window_t win = clients[focusedIndex];

    monocleResize(win);

    uint32_t stackMode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_STACK_MODE, &stackMode);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
    xcb_flush(connection);

}

static void changeFullscreen() {
  if (clients.empty()) return;
  if (fullscreen == false) {
    fullscreen = true;
    refocusCleint();
    std::cout << "Current fullscreen mode is " << fullscreen << std::endl;
  } else {
    fullscreen = false;
    refocusCleint();
    std::cout << "Current fullscreen mode is " << fullscreen << std::endl;
  }
}


/* scrolling functions */

static void removeClient(xcb_window_t win) {
    clients.erase(std::remove(clients.begin(), clients.end(), win), clients.end());
    if (!clients.empty()) focusClient(0);
}


static void checkClients() {
  if (clients.empty()) return;
  printf("Starting to check clients.");
  for (size_t i = clients.size() - 1; i > 0; i--) {
    xcb_window_t win = clients[i];
    std::cout << "Checking window number " << i << std::endl;
    auto cookie = xcb_get_window_attributes(connection, win);
    auto reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
      printf("Client is dead, clearing up it.");
      removeClient(win);
    } else {
      std::cout << "Client is alive, doing nothing" << std::endl;
    }
  }
}


static void focusNext(xcb_key_press_event_t *kp, uint16_t state) { 
  checkClients();
  if (kp->time - lastSwitchTime < 150) return;
      lastSwitchTime = kp->time;
  if (state & XCB_MOD_MASK_SHIFT) {
    focusClient(focusedIndex == 0 ? clients.size() - 1 : focusedIndex - 1);
  } else {
    focusClient(focusedIndex + 1);   
  }
   std::cout << "Window scrolled, current window is:" << focusedIndex << std::endl;
   std::cout << "Current client size is:" << clients.size() << std::endl;
}

static void focusPrev(xcb_key_press_event_t *kp, uint16_t state) {
  checkClients();
  if (kp->time - lastSwitchTime < 150) return;
      lastSwitchTime = kp->time;
  if (state & XCB_MOD_MASK_SHIFT) {
    focusClient(focusedIndex == 0 ? clients.size() - 1 : focusedIndex - 1);
  } else {
    if (focusedIndex > 0) {
      focusClient(focusedIndex - 1);
    } else {
      focusClient(clients.size() - 1);
    }
  }
   std::cout << "Window scrolled, current window is:" << focusedIndex << std::endl;
   std::cout << "Current client size is:" << clients.size() << std::endl;

}


static void switchWindow(xcb_key_press_event_t *kp, uint16_t state) { 
  checkClients();
  if (kp->time - lastSwitchTime < 150) return;
      lastSwitchTime = kp->time;
  if (state & XCB_MOD_MASK_SHIFT) {
    focusClient(focusedIndex == 0 ? clients.size() - 1 : focusedIndex - 1);
  } else {
    focusClient(focusedIndex + 1);
  }
   std::cout << "Window switched, current window is:" << focusedIndex << std::endl;
   std::cout << "Current client size is:" << clients.size() << std::endl;
}

static void changeResolution(const char *monitor ,uint32_t widgth, uint32_t height) {
  std::string command = std::string("xrandr --output ") + monitor + std::string("--mode ") + std::to_string(widgth) + std::string("x") + std::to_string(height);
  std::cout << command << std::endl;
  spawn(command.c_str());
}


static void killFocused() {
    xcb_window_t win = clients[focusedIndex];
    if (clients.empty()) return;
    xcb_kill_client(connection, clients[focusedIndex]);
    xcb_flush(connection);
    removeClient(win);
}

static xcb_keycode_t firstKeycode(xcb_keysym_t sym) {
    xcb_keycode_t *kc = xcb_key_symbols_get_keycode(keysyms, sym);
    xcb_keycode_t result = kc ? kc[0] : 0;
    fprintf(stderr, "sym=%u kc[0]=%u\n", (unsigned)sym, (unsigned)kc[0]);
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
    keyE     = firstKeycode(0x0065); /* XK_e */
    keyB     = firstKeycode(0x0062); /* XK_b */
    keyD     = firstKeycode(0x0044); /* XK_d */
    keyT     = firstKeycode(0x0074); /* XK_t */
    keyF     = firstKeycode(0x0046); /* XK_f */
    keyLeft  = firstKeycode(0xff51);
    keyRight = firstKeycode(0xff53);
    grabKey(XCB_MOD_MASK_4, keyTab);
    grabKey(XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT, keyTab);
    grabKey(XCB_MOD_MASK_4, keyEnter);
    grabKey(XCB_MOD_MASK_4, keyQ);
    grabKey(XCB_MOD_MASK_4, keyE);
    grabKey(XCB_MOD_MASK_4, keyB);
    grabKey(XCB_MOD_MASK_4, keyD);
    grabKey(XCB_MOD_MASK_4, keyT);
    grabKey(XCB_MOD_MASK_4, keyF);
    grabKey(XCB_MOD_MASK_4, keyLeft);
    grabKey(XCB_MOD_MASK_4, keyRight);
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



/*
static void onDestroyNotify(xcb_generic_event_t *event) {
    removeClient(((xcb_destroy_notify_event_t *)event)->window);
} */
/*
static void onUnmapNotify(xcb_generic_event_t *event) {
    removeClient(((xcb_unmap_notify_event_t *)event)->window);
} */

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
        fprintf(stderr, "KP detail=%u rawstate=0x%02x keyE=%u keyTab=%u keyQ=%u keyB=%u keyEnter=%u\n",
            kp->detail, state,
            keyE, keyTab, keyQ, keyEnter);
    if (kp->detail == keyTab) {
      std::cout << "Button Tab registered" << std::endl;
      switchWindow(kp, state);
    } else if (kp->detail == keyEnter) {
        /* ignore auto-repeat floods */
      std::cout << "Button Enter registered" << std::endl;
        if (kp->time - lastSpawnTime < 400) return;
        lastSpawnTime = kp->time;
        spawn("xterm -fa 'DejaVu Sans Mono' -fs 12");
    } else if (kp->detail == keyQ) {
      std::cout << "Button Q registered";
        killFocused();
    } else if (kp->detail == keyD) {
      std::cout << "Button D registered" << std::endl;
      if (kp->time - lastSpawnTime < 400) return;
      lastSpawnTime = kp->time;
      spawn("rofi -show drun");
    } else if (kp->detail == keyT) {
      std::cout << "Button T registered" << std::endl;
      if (kp->time - lastSpawnTime < 400) return;
      lastSpawnTime = kp->time;
      spawn(terminal);
    } else if (kp->detail == keyF) {
      std::cout << "Button F registered" << std::endl;
      changeFullscreen();
    } else if (kp->detail == keyLeft) {
      std::cout << "Left Button registered" << std::endl;
      focusPrev(kp, state);
    } else if (kp->detail == keyRight) {
      std::cout << "Right Button registered" << std::endl;
      focusNext(kp, state);
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
    
    std::cout << "minimal monocle wm started (Mod4+tab Switch, Mod4+Enter xterm, Mod4+Q kill, Mod4+D rofi, Mod4+LeftKey scroll left, Mod4+RightKey scroll right)\n";
    //Autostart functions
    spawn("feh --bg-scale ~/Pictures/wallpaper.jpg");
    spawn("xrandr --output eDP-1 --mode 1280x720"); // Put your own monitor and mode here
    spawn("setxkbmap -layout us,ru -option 'grp:alt_shift_toggle'");

    // events receiving and sending it back
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(connection))) {
        switch (event->response_type & ~0x80) {
            case XCB_MAP_REQUEST:       onMapRequest(event);       break;
            case XCB_CONFIGURE_REQUEST: onConfigureRequest(event); break;
        //    case XCB_DESTROY_NOTIFY:    onDestroyNotify(event);    break;
        //    case XCB_UNMAP_NOTIFY:      onUnmapNotify(event);      break;
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
