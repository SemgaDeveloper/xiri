#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <xcb/xcb_icccm.h>
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
uint32_t windowGap = 24;
uint32_t windowWidgth = customWidgth - windowGap; // You can set there what you want, this will affect only windows
uint32_t windowHeight = customHeight - windowGap; // Same as previous string
const char *monitor = "eDP-1"; // Change to your monitor name if eDP-1 does not suits you
const char *terminal = "kitty"; // This terminal will be used for Mod4+t variant
const char *wallpaper = "~/Pictures/wallpaper.jpg"; // This will be used for setting up wallpaper with feh at startup
const char *keyboardconfig = "-layout us,ru -option 'grp:alt_shift_toggle'"; // This will be used for running stxkbmap
/* state */
static std::vector<xcb_window_t> clients;
static size_t focusedIndex = 0;
bool fullscreen = false;

static xcb_connection_t   *connection;
static xcb_screen_t       *screen;
static xcb_key_symbols_t  *keysyms;

static xcb_keycode_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key0, keyTab, keyEnter, keyQ, keyE, keyB, keyD, keyT, keyF, keyLeft, keyRight, keyC;
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
    uint32_t values[4] = {windowGap/2, windowGap/2, windowWidgth, windowHeight};
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
    applyMonocleAll();
    // monocleResize(win);

    uint32_t stackMode = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(connection, win, XCB_CONFIG_WINDOW_STACK_MODE, &stackMode);

    xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, win, XCB_CURRENT_TIME);
    xcb_flush(connection);
}

static void refocusCleint() {
    if (clients.empty()) return;
    xcb_window_t win = clients[focusedIndex];
    applyMonocleAll();
    // monocleResize(win);

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

static void centerFocused() {
  if (clients.empty()) return;
  xcb_window_t win = clients[focusedIndex];
  monocleResize(win);
  xcb_flush(connection);
  std::cout << "Focused window snapped back to default geometry" << std::endl;
}


/* scrolling functions */


static void removeClient(xcb_window_t win) {
    clients.erase(std::remove(clients.begin(), clients.end(), win), clients.end());
    if (!clients.empty()) focusClient(0);
}


static void checkClients() {
  if (clients.empty()) return;
  printf("Starting to check clients.\n");
  for (size_t i = clients.size(); i-- > 0;) {
    xcb_window_t win = clients[i];
    std::cout << "Checking window number " << i << std::endl;
    auto cookie = xcb_get_window_attributes(connection, win);
    auto reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
      printf("Client is dead, clearing up it.\n");
      removeClient(win);
    } else {
      std::cout << "Client is alive, doing nothing" << std::endl;
      free(reply);
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


static void gotoWindow(xcb_key_press_event_t *kp, size_t windownumber) {
  if (clients.empty()) return;
  if (kp->time - lastSwitchTime < 150) return;
    lastSwitchTime = kp->time;
  windownumber = windownumber - 1;
  if (windownumber >= clients.size()) return;
  std::cout << "Activationg gotoWindow func, windownumber is " << windownumber << std::endl; 
  xcb_window_t win = clients[windownumber];
  auto cookie = xcb_get_window_attributes(connection, win);
  auto reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
  if (reply == nullptr) {
    printf("Client are does not exists, window hasn't switched\n");
  } else {
    focusClient(windownumber);
    free(reply);
  }
}


static void moveColumnLeft(xcb_key_press_event_t *kp) {
  if (clients.size() < 2) return;
  if (kp->time - lastSwitchTime < 150) return;
  lastSwitchTime = kp->time;

  size_t targetIndex = (focusedIndex == 0) ? clients.size() - 1 : focusedIndex - 1;
  std::swap(clients[focusedIndex], clients[targetIndex]);
  focusClient(targetIndex);
  std::cout << "Column moved left, now at position " << targetIndex << std::endl;
}

static void moveColumnRight(xcb_key_press_event_t *kp) {
  if (clients.size() < 2) return;
  if (kp->time - lastSwitchTime < 150) return;
  lastSwitchTime = kp->time;

  size_t targetIndex = (focusedIndex + 1) % clients.size();
  std::swap(clients[focusedIndex], clients[targetIndex]);
  focusClient(targetIndex);
  std::cout << "Column moved right, now at position " << targetIndex << std::endl;
}

static void changeResolution(const char *monitorChoice ,uint32_t widgth, uint32_t height) {
  std::string command = std::string("xrandr --output ") + monitorChoice + std::string(" --mode ") + std::to_string(widgth) + std::string("x") + std::to_string(height);
  std::cout << command << std::endl;
  spawn(command.c_str());
}

static void setupWallpaper(const char *wallpaperpath) {
  std::string command = std::string("feh --bg-scale ") + wallpaperpath;
  spawn(command.c_str());
}

static void setxkbmapconfig(const char *variant) {
  std::string command = std::string("setxkbmap ") + variant;
  spawn(command.c_str());
}

static void killFocused() {
    if (clients.empty()) return;
    xcb_window_t win = clients[focusedIndex];
    xcb_kill_client(connection, clients[focusedIndex]);
    xcb_flush(connection);
    removeClient(win);
}

static xcb_keycode_t firstKeycode(xcb_keysym_t sym) {
    xcb_keycode_t *kc = xcb_key_symbols_get_keycode(keysyms, sym);
    xcb_keycode_t result = kc ? kc[0] : 0;
    fprintf(stderr, "sym=%u keycode=%u\n", (unsigned)sym, (unsigned)result);
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
    key1     = firstKeycode(0x0031); /* XK_1 */
    key2     = firstKeycode(0x0032); /* XK_2 */
    key3     = firstKeycode(0x0033); /* XK_3 */
    key4     = firstKeycode(0x0034); /* XK_4 */
    key5     = firstKeycode(0x0035); /* XK_5 */
    key6     = firstKeycode(0x0036); /* XK_6 */
    key7     = firstKeycode(0x0037); /* XK_7 */
    key8     = firstKeycode(0x0038); /* XK_8 */
    key9     = firstKeycode(0x0039); /* XK_9 */
    key0     = firstKeycode(0x0030); /* XK_0 */
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
    keyC     = firstKeycode(0x0063); /* XK_c */
    grabKey(XCB_MOD_MASK_4, key0);
    grabKey(XCB_MOD_MASK_4, key1);
    grabKey(XCB_MOD_MASK_4, key2);
    grabKey(XCB_MOD_MASK_4, key3);
    grabKey(XCB_MOD_MASK_4, key4);
    grabKey(XCB_MOD_MASK_4, key5);
    grabKey(XCB_MOD_MASK_4, key6);
    grabKey(XCB_MOD_MASK_4, key7);
    grabKey(XCB_MOD_MASK_4, key8);
    grabKey(XCB_MOD_MASK_4, key9);
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
    grabKey(XCB_MOD_MASK_4, keyC);
    grabKey(XCB_MOD_MASK_4 | XCB_MOD_MASK_CONTROL, keyLeft);
    grabKey(XCB_MOD_MASK_4 | XCB_MOD_MASK_CONTROL, keyRight);
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
    uint16_t state = kp->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2); 
    if (!(state & XCB_MOD_MASK_4)) return;
    if (kp->detail == keyTab) {
      std::cout << "Button Tab registered" << std::endl;
      switchWindow(kp, state);
    } else if (kp->detail == keyEnter) {
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
      if (state & XCB_MOD_MASK_CONTROL) {
        std::cout << "Ctrl+Left registered" << std::endl;
        moveColumnLeft(kp);
      } else {
        std::cout << "Left Button registered" << std::endl;
        focusPrev(kp, state);
      }
    } else if (kp->detail == keyRight) {
      if (state & XCB_MOD_MASK_CONTROL) {
        std::cout << "Ctrl+Right registered" << std::endl;
        moveColumnRight(kp);
      } else {
        std::cout << "Right Button registered" << std::endl;
        focusNext(kp, state);
      }
    } else if (kp->detail == keyC) {
      std::cout << "Button C registered" << std::endl;
      centerFocused();
    }
      else if (kp->detail == key1) {
      std::cout << "Button 1 registered" << std::endl;
      gotoWindow(kp, 1);
    } else if (kp->detail == key2) {
      std::cout << "Button 2 registered" << std::endl;
      gotoWindow(kp, 2);
    } else if (kp->detail == key3) {
      std::cout << "Button 3 registered" << std::endl;
      gotoWindow(kp, 3);
    } else if (kp->detail == key4) {
      std::cout << "Button 4 registered" << std::endl;
      gotoWindow(kp, 4);
    } else if (kp->detail == key5) {
      std::cout << "Button 5 registered" << std::endl;
      gotoWindow(kp, 5);
    } else if (kp->detail == key6) {
      std::cout << "Button 6 registered" << std::endl;
      gotoWindow(kp, 6);
    } else if (kp->detail == key7) {
      std::cout << "Button 7 registered" << std::endl;
      gotoWindow(kp, 7);
    } else if (kp->detail == key8) {
      std::cout << "Button 8 registered" << std::endl;
      gotoWindow(kp, 8);
    } else if (kp->detail == key9) {
      std::cout << "Button 9 registered" << std::endl;
      gotoWindow(kp, 9);
    } else if (kp->detail == key0) {
      std::cout << "Button 0 registered" << std::endl;
      gotoWindow(kp, 10);
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
    
    std::cout << "minimal monocle wm started (Mod4+tab Switch, Mod4+Enter xterm, Mod4+Q kill, Mod4+D application launcher, Mod4+T terminal, Mod4+LeftKey scroll left, Mod4+RightKey scroll right, Mod4+Ctrl+Left/Right move column, Mod4+C center window, Mod4+(0-9) gotoWindow)\n";
    //Autostart functions //
    changeResolution(monitor, customWidgth, customHeight);
    setupWallpaper(wallpaper);
    setxkbmapconfig(keyboardconfig);
    // spawn("setxkbmap -layout us,ru -option 'grp:alt_shift_toggle'");
    // events receiving and sending it back
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
