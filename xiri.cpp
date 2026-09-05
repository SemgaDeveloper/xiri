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
#include <unordered_map>
#include <string>
#include <csignal>
#include <fstream>
#include <sstream>
#include <cctype>




/* Test config values, befoe i made special file for configurating your xiri, you can configure it there */
uint32_t customWidgth = 1920; // Change resolution what your windows will open 
uint32_t customHeight = 1200; // Also custom resolution will be applied through xrandr
uint32_t windowGap = 24;
uint32_t windowWidgth = customWidgth - windowGap; // You can set there what you want, this will affect only windows
uint32_t windowHeight = customHeight - windowGap; // Same as previous string
std::string monitor = "eDP-1"; // Change to your monitor name if eDP-1 does not suits you
std::string terminal = "kitty"; // This terminal will be used for Mod4+t variant
std::string wallpaperUtility = "feh"; // This will be used for setting up wallpaper
std::string wallpaper = "~/Pictures/wallpaper.jpg"; // This will be used for setting up wallpaper with feh at startup
std::string keyboardconfig = "-layout us,ru -option 'grp:alt_shift_toggle'"; // This will be used for running stxkbmap
std::string applicationlauncher = "rofi -show drun"; // This will be used for running application launcher
std::string customApplication = "firefox"; // This will be used for enter key
std::string screentaker = "spectacle"; // This will be used for taking screenshots
std::string customBar = "polybar"; // This will be used for running custom bar

/* config reader function */

static std::string trim(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

static std::string unquote(const std::string &value) {
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

static void readConfigFile() {
  const char *home = std::getenv("HOME");
  std::string configPath = home ? std::string(home) + "/.config/xiri/config.ini" : "config.ini";
  std::ifstream configFile(configPath);
  if (!configFile.is_open() && configPath != "config.ini") {
    configPath = "config.ini";
    configFile.open(configPath);
  }
  if (!configFile.is_open()) {
    std::cerr << "Failed to open config file: " << configPath << std::endl;
    std::cout << "Using default configuration values." << std::endl;
    return;
  }

  std::string section;
  std::string line;
  while (std::getline(configFile, line)) {
    line = trim(line);
    if (line.empty() || line.front() == '#' || line.front() == ';') continue;
    if (line.front() == '[' && line.back() == ']') {
      section = trim(line.substr(1, line.size() - 2));
      continue;
    }

    const auto separator = line.find('=');
    if (separator == std::string::npos) continue;
    const std::string key = trim(line.substr(0, separator));
    const std::string value = unquote(trim(line.substr(separator + 1)));
    try {
      if (section == "General" && key == "windowgap") {
        windowGap = std::stoul(value);
      } else if (section == "Applications" && key == "terminal") {
        terminal = value;
      } else if (section == "Applications" && key == "wallpaperUtility") {
        wallpaperUtility = value;
      } else if (section == "Applications" && key == "wallpaper") {
        wallpaper = value;
      } else if (section == "Applications" && key == "keyboardconfig") {
        keyboardconfig = value;
      } else if (section == "Applications" && key == "applicationlauncher") {
        applicationlauncher = value;
      } else if (section == "Applications" && key == "customApplication") {
        customApplication = value;
      } else if (section == "Applications" && key == "screentaker") {
        screentaker = value;
      } else if (section == "Applications" && key == "customBar") {
        customBar = value;
      } else if (section == "Display" && key == "monitor") {
        monitor = value;
      } else if (section == "Display" && key == "customWidgth") {
        customWidgth = std::stoul(value);
      } else if (section == "Display" && key == "customHeight") {
        customHeight = std::stoul(value);
      }
    } catch (const std::exception &) {
      std::cerr << "Invalid value for [" << section << "] " << key << std::endl;
    }
  }

  windowWidgth = customWidgth - windowGap;
  windowHeight = customHeight - windowGap;
}



/* state */

static std::vector<xcb_window_t> clients;
static size_t focusedIndex = 0;
bool fullscreen = false;
bool activeBar = false;
bool DesktopMode = false;

static xcb_connection_t   *connection;
static xcb_screen_t       *screen;
static xcb_key_symbols_t  *keysyms;

static xcb_keycode_t key1, key2, key3, key4, key5, key6, key7, key8, key9, key0, keyTab, keyEnter, keyQ, keyE, keyB, keyD, keyT, keyF, keyP, keyH, keyLeft, keyRight, printScreen;
static xcb_timestamp_t lastSpawnTime = 0;
static xcb_timestamp_t lastSwitchTime = 0;
static xcb_atom_t netWmWindowType;
static xcb_atom_t netWmWindowTypeDock;

enum class KeyAction {
  SwitchWindow, SpawnTerminal, KillFocused, SpawnLauncher,
  SpawnConfiguredTerminal, ToggleFullscreen, FocusPrevious, FocusNext,
  GotoWindow1, GotoWindow2, GotoWindow3, GotoWindow4, GotoWindow5,
  GotoWindow6, GotoWindow7, GotoWindow8, GotoWindow9, GotoWindow10,
  launchScreenshot, launchSpecialApplication, exitSession, launchBar,
  showDesktop
};

static std::unordered_map<xcb_keycode_t, KeyAction> keyActions;

/* lock modifiers that must be grabbed in every combination, or grabs
   silently fail to match whenever numlock/capslock is on */
static const uint16_t lockMasks[4] = {
    0,
    XCB_MOD_MASK_LOCK,                    /* CapsLock */
    XCB_MOD_MASK_2,                       /* NumLock (common mapping) */
    XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2
};



/* helpers */

static xcb_atom_t internAtom(const char *name) {
  xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, strlen(name), name);
  xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
  if (!reply) return XCB_ATOM_NONE;
  xcb_atom_t atom = reply->atom;
  free(reply);
  return atom;
}

static bool isUtilityWindow(xcb_window_t window) {
  if (netWmWindowType != XCB_ATOM_NONE && netWmWindowTypeDock != XCB_ATOM_NONE) {
    xcb_get_property_cookie_t cookie = xcb_get_property(
      connection, 0, window, netWmWindowType, XCB_ATOM_ATOM, 0, 8);
    xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, nullptr);
    if (reply) {
      auto *types = static_cast<xcb_atom_t *>(xcb_get_property_value(reply));
      int typeCount = xcb_get_property_value_length(reply) / sizeof(xcb_atom_t);
      for (int i = 0; i < typeCount; ++i) {
        if (types[i] == netWmWindowTypeDock) {
          free(reply);
          return true;
        }
      }
      free(reply);
    }
  }

  xcb_get_property_cookie_t cookie = xcb_get_property(
    connection, 0, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 64);
  xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, nullptr);
  if (!reply) return false;
  std::string className(static_cast<char *>(xcb_get_property_value(reply)),
              xcb_get_property_value_length(reply));
  free(reply);
  return className.find("Polybar") != std::string::npos ||
       className.find("polybar") != std::string::npos;
}

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
    if (activeBar == false) {
      uint32_t values[4] = {windowGap/2, windowGap/2, windowWidgth, windowHeight};
      uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
      xcb_configure_window (connection, win, mask, values);
    } else {
      uint32_t values[4] = {windowGap/2, windowGap/2 + 35, windowWidgth, windowHeight - 35};
      uint16_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
      XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
      xcb_configure_window (connection, win, mask, values);
    }
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


/* scrolling functions */


static void normalizeFocusedIndex() {
  if (clients.empty()) {
    focusedIndex = 0;
    return;
  }
  if (focusedIndex >= clients.size()) {
    focusedIndex = clients.size() - 1;
  }
}

static void removeClient(xcb_window_t win) {
    clients.erase(std::remove(clients.begin(), clients.end(), win), clients.end());
    normalizeFocusedIndex();
    if (!clients.empty()) focusClient(focusedIndex);
}


static void checkClients() {
  if (clients.empty()) return;

  std::vector<xcb_window_t> validClients;
  validClients.reserve(clients.size());

  for (xcb_window_t win : clients) {
    if (isUtilityWindow(win)) {
      std::cout << "Utility client detected, removing it from the client list" << std::endl;
      continue;
    }

    auto cookie = xcb_get_window_attributes(connection, win);
    auto reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
    if (reply == nullptr) {
      std::cout << "Client is dead, clearing it up" << std::endl;
      continue;
    }
    free(reply);
    validClients.push_back(win);
  }

  if (validClients.size() != clients.size()) {
    clients.swap(validClients);
    normalizeFocusedIndex();
    if (!clients.empty()) {
      focusClient(focusedIndex);
    }
  } else {
    normalizeFocusedIndex();
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
  checkClients();
  if (windownumber == 0 || windownumber > clients.size()) return;
  if (kp->time - lastSwitchTime < 150) return;
    lastSwitchTime = kp->time;
  windownumber = windownumber - 1;
  std::cout << "Activationg gotoWindow func, windownumber is " << windownumber << std::endl; 
  xcb_window_t win = clients[windownumber];
  auto cookie = xcb_get_window_attributes(connection, win);
  auto reply = xcb_get_window_attributes_reply(connection, cookie, nullptr);
  if (reply == nullptr) {
    printf("Client are does not exists, window hasn't switched\n");
  } else {
    focusClient(windownumber);
  }
}

static void changeResolution(const char *monitorChoice ,uint32_t widgth, uint32_t height) {
  std::string command = std::string("xrandr --output ") + monitorChoice + std::string(" --mode ") + std::to_string(widgth) + std::string("x") + std::to_string(height);
  std::cout << command << std::endl;
  spawn(command.c_str());
}

static void setupWallpaper(const char *wallpaperpath) {
  if (wallpaperUtility == "feh") {
    std::string command = std::string("feh --bg-scale ") + wallpaperpath;
    spawn(command.c_str());
  } else if (wallpaperUtility == "xwallpaper") {
    std::string command = std::string("xwallpaper --zoom ") + wallpaperpath;
    spawn(command.c_str());
  } else if (wallpaperUtility == "nitrogen") {
    std::string command = std::string("nitrogen --set-zoom-fill ") + wallpaperpath;
    spawn(command.c_str());
  } else {
    return;
  }
}

static void launchBar() {
  if (activeBar == false) {
    spawn(customBar.c_str());
    activeBar = true;
  } else {
    std::string command = std::string("killall ") + customBar;
    spawn(command.c_str());
    activeBar = false;
  }
  if (clients.empty()) return;
  monocleResize(clients[focusedIndex]);
}

static void setxkbmapconfig(const char *variant) {
  std::string command = std::string("setxkbmap ") + variant;
  spawn(command.c_str());
}

static void killFocused() {
    checkClients();
    if (clients.empty()) return;
    if (focusedIndex >= clients.size()) {
      focusedIndex = clients.size() - 1;
    }
    xcb_window_t win = clients[focusedIndex];
    xcb_kill_client(connection, clients[focusedIndex]);
    xcb_flush(connection);
    removeClient(win);
}

static void launchScreenshot() {
  spawn(screentaker.c_str());
}

static void launchSpecialApplication() {
  spawn(customApplication.c_str());
}

static void showDesktop() {
  if (DesktopMode == false) {
    for (auto w : clients) {
      xcb_unmap_window(connection, w);
    }
    DesktopMode = true;
  } else {
    for (auto w : clients) {
      xcb_map_window(connection, w);
    }
    DesktopMode = false;
  }
}

static void exitSession() {
  xcb_disconnect(connection);
  exit(0);
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
    keyP     = firstKeycode(0x0070); /* XK_p */
    keyH     = firstKeycode(0x0068); /* XK_h */
    keyLeft  = firstKeycode(0xff51); 
    keyRight = firstKeycode(0xff53);
    printScreen = firstKeycode(0xff61); /* XK_Print */
    keyActions = {
      {keyTab, KeyAction::SwitchWindow}, {keyEnter, KeyAction::launchSpecialApplication},
      {keyQ, KeyAction::KillFocused}, {keyD, KeyAction::SpawnLauncher},
      {keyT, KeyAction::SpawnConfiguredTerminal}, {keyF, KeyAction::ToggleFullscreen},
      {keyE, KeyAction::exitSession}, {keyLeft, KeyAction::FocusPrevious}, {keyRight, KeyAction::FocusNext},
      {key1, KeyAction::GotoWindow1}, {key2, KeyAction::GotoWindow2},
      {key3, KeyAction::GotoWindow3}, {key4, KeyAction::GotoWindow4},
      {key5, KeyAction::GotoWindow5}, {key6, KeyAction::GotoWindow6},
      {key7, KeyAction::GotoWindow7}, {key8, KeyAction::GotoWindow8},
      {key9, KeyAction::GotoWindow9}, {key0, KeyAction::GotoWindow10},
      {printScreen, KeyAction::launchScreenshot}, {keyP, KeyAction::launchBar},
      {keyH, KeyAction::showDesktop}
    };
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
    grabKey(XCB_MOD_MASK_4 | XCB_MOD_MASK_SHIFT, keyE);
    grabKey(XCB_MOD_MASK_4, keyB);
    grabKey(XCB_MOD_MASK_4, keyD);
    grabKey(XCB_MOD_MASK_4, keyT);
    grabKey(XCB_MOD_MASK_4, keyF);
    grabKey(XCB_MOD_MASK_4, keyP);
    grabKey(XCB_MOD_MASK_4, keyH);
    grabKey(XCB_MOD_MASK_4, keyLeft);
    grabKey(XCB_MOD_MASK_4, keyRight);
    grabKey(XCB_MOD_MASK_4, printScreen);
}

/* event handlers */

static void onMapRequest(xcb_generic_event_t *event) {
    xcb_map_request_event_t *mr = (xcb_map_request_event_t *)event;

  if (isUtilityWindow(mr->window)) {
    xcb_map_window(connection, mr->window);
    xcb_flush(connection);
    return;
  }

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
    uint16_t state = kp->state & ~(XCB_MOD_MASK_LOCK | XCB_MOD_MASK_2); 
    if (!(state & XCB_MOD_MASK_4)) return;
    auto action = keyActions.find(kp->detail);
    if (action == keyActions.end()) return;

    switch (action->second) {
        case KeyAction::SwitchWindow: switchWindow(kp, state); break;
        case KeyAction::KillFocused: killFocused(); break;
        case KeyAction::SpawnLauncher:
            if (kp->time - lastSpawnTime < 400) return;
            lastSpawnTime = kp->time;
            spawn(applicationlauncher.c_str());
            break;
        case KeyAction::SpawnConfiguredTerminal:
            if (kp->time - lastSpawnTime < 400) return;
            lastSpawnTime = kp->time;
            spawn(terminal.c_str());
            break;
        case KeyAction::launchBar: launchBar(); break;
        case KeyAction::exitSession: exitSession(); break;
        case KeyAction::launchScreenshot: launchScreenshot(); break;
        case KeyAction::launchSpecialApplication: launchSpecialApplication(); break;
        case KeyAction::ToggleFullscreen: changeFullscreen(); break;
        case KeyAction::FocusPrevious: focusPrev(kp, state); break;
        case KeyAction::FocusNext: focusNext(kp, state); break;
        case KeyAction::showDesktop: showDesktop(); break;
        case KeyAction::GotoWindow1:
        case KeyAction::GotoWindow2:
        case KeyAction::GotoWindow3:
        case KeyAction::GotoWindow4:
        case KeyAction::GotoWindow5:
        case KeyAction::GotoWindow6:
        case KeyAction::GotoWindow7:
        case KeyAction::GotoWindow8:
        case KeyAction::GotoWindow9:
        case KeyAction::GotoWindow10:
            gotoWindow(kp, static_cast<size_t>(action->second) - static_cast<size_t>(KeyAction::GotoWindow1) + 1);
            break;
        case KeyAction::SpawnTerminal:
          break;
    }
} 



/* main */

int main() {
    signal(SIGCHLD, SIG_IGN); /* auto-reap children */
  readConfigFile();

    connection = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(connection)) {
        fprintf(stderr, "cannot connect to X server\n");
        return 1;
    }

    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    screen = iter.data;
    netWmWindowType = internAtom("_NET_WM_WINDOW_TYPE");
    netWmWindowTypeDock = internAtom("_NET_WM_WINDOW_TYPE_DOCK");

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
    readConfigFile();
    
    // Autostart Functions
    changeResolution(monitor.c_str(), customWidgth, customHeight);
    setupWallpaper(wallpaper.c_str());
    setxkbmapconfig(keyboardconfig.c_str());
    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(connection))) {
        switch (event->response_type & ~0x80) {
            case XCB_MAP_REQUEST:       onMapRequest(event);       break;
            case XCB_CONFIGURE_REQUEST: onConfigureRequest(event); break;
        /*  case XCB_DESTROY_NOTIFY:    onDestroyNotify(event);    break;
            case XCB_UNMAP_NOTIFY:      onUnmapNotify(event);      break; -- This 2 strokes are unrecommended to uncomment, because
            they can cause some bugs with focused index */
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
