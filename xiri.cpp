#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <inttypes.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>

short currentDesktop;
bool floatingWindows;

static void testCookie(xcb_void_cookie_t, xcb_connection_t*, const char *);   /* functions's skeletons, the functions's description are at the bottom of project */                                                                                 
static void setCursor (xcb_connection_t*, xcb_screen_t*, xcb_window_t, int);
static void setColormap (xcb_connection_t* , xcb_window_t, xcb_screen_t*);
static void setPixmap (xcb_connection_t*,xcb_window_t ,xcb_screen_t*);
static void drawRedDot (xcb_connection_t*, xcb_window_t, xcb_screen_t*);
void print_modifiers (uint32_t mask);



int main() {
  
  std::cout << "Hello to my tt subscribers and viewers from reddit!" << std::endl;

  xcb_connection_t *connection = xcb_connect (NULL, NULL); 
  const xcb_setup_t *setup = xcb_get_setup (connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
  xcb_screen_t *screen = iter.data;
 
  /* Create black (foreground) graphic context */
        xcb_drawable_t  window = screen -> root;
        xcb_gcontext_t  foreground = xcb_generate_id (connection);
        uint32_t        mask       = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
        uint32_t        values[2]  = {screen->black_pixel, 0};

        xcb_create_gc (connection, foreground, window, mask, values); 
  
  window = xcb_generate_id (connection);
  
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  values[0] = screen -> white_pixel; 
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
              XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
              XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;
  
  xcb_font_t font = xcb_generate_id (connection);
  xcb_open_font (connection, font, strlen ("cursor"), "cursor");
 

   xcb_create_window (connection, /* Connection */
                     XCB_COPY_FROM_PARENT, /*depth, idk what is this but ill tag this */
                     window, /* id of my window */
                     screen -> root, /* parent of window */
                     0, 0, /* x and y of window */
                     1280, 720, /* width and height of window */
                     10, /* border width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class, idk what is this too */
                     screen -> root_visual, /* visual, idk what is this too */
                     mask, values); /* masks, still aren't used */
  xcb_map_window (connection, window);
  xcb_flush(connection);
  setCursor(connection, screen, window, 68);
  setPixmap(connection, window, screen);
  setColormap(connection, window, screen);
/*drawRedDot(connection, window, screen); -- function that wrote ai for testing pixmap and colormap */

  printf ("\n");
  printf ("Informations of screen %" PRIu32 ":\n", screen->root);
  printf ("  width: %" PRIu16 "\n", screen->width_in_pixels);
  printf ("  height: %" PRIu16 "\n", screen->height_in_pixels);
  printf ("  white pixel: %" PRIu32 "\n", screen->white_pixel);
  printf ("  black pixel: %" PRIu32 "\n", screen->black_pixel);
  printf ("\n"); 
  

  xcb_change_window_attributes (connection, window, XCB_EVENT_MASK_BUTTON_PRESS, values);

        xcb_generic_event_t *event;
        while ( (event = xcb_wait_for_event (connection)) ) {
            switch (event->response_type & ~0x80) {
            case XCB_EXPOSE: {
                xcb_expose_event_t *expose = (xcb_expose_event_t *)event;

                printf ("Window %" PRIu32 " exposed. Region to be redrawn at location (%" PRIu16 ",%" PRIu16 "), with dimension (%" PRIu16 ",%" PRIu16 ")\n",
                        expose->window, expose->x, expose->y, expose->width, expose->height );
                break;
            }
            case XCB_BUTTON_PRESS: {
                xcb_button_press_event_t *bp = (xcb_button_press_event_t *)event;
                print_modifiers (bp->state);

                switch (bp->detail) {
                case 4:
                    printf ("Wheel Button up in window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                            bp->event, bp->event_x, bp->event_y );
                    break;
                case 5:
                    printf ("Wheel Button down in window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                            bp->event, bp->event_x, bp->event_y );
                    break;
                default:
                    printf ("Button %" PRIu8 " pressed in window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                            bp->detail, bp->event, bp->event_x, bp->event_y );
                    break;
                }
                break;
            }
            case XCB_BUTTON_RELEASE: {
                xcb_button_release_event_t *br = (xcb_button_release_event_t *)event;
                print_modifiers(br->state);

                printf ("Button %" PRIu8 " released in window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                        br->detail, br->event, br->event_x, br->event_y );
                break;
            }
            case XCB_MOTION_NOTIFY: {
                xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;

                printf ("Mouse moved in window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                        motion->event, motion->event_x, motion->event_y );
                break;
            }
            case XCB_ENTER_NOTIFY: {
                xcb_enter_notify_event_t *enter = (xcb_enter_notify_event_t *)event;

                printf ("Mouse entered window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                        enter->event, enter->event_x, enter->event_y );
                break;
            }
            case XCB_LEAVE_NOTIFY: {
                xcb_leave_notify_event_t *leave = (xcb_leave_notify_event_t *)event;

                printf ("Mouse left window %" PRIu32 ", at coordinates (%" PRIi16 ",%" PRIi16 ")\n",
                        leave->event, leave->event_x, leave->event_y );
                break;
            }
            case XCB_KEY_PRESS: {
                xcb_key_press_event_t *kp = (xcb_key_press_event_t *)event;
                print_modifiers(kp->state);

                printf ("Key pressed in window %" PRIu32 "\n",
                        kp->event);
                
                // start xterm by pressing Enter
                if (kp->detail == 36) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        // child process
                        execlp("xterm", "xterm", "-e", "/bin/sh", (char *)NULL);
                        exit(1); // exec failed
                    } else if (pid > 0) {
                        printf("Started xterm PID %d\n", pid);
                    } else {
                        perror("fork");
                    }
                }
                break;
            }
            case XCB_KEY_RELEASE: {
                xcb_key_release_event_t *kr = (xcb_key_release_event_t *)event;
                print_modifiers(kr->state);

                printf ("Key released in window %" PRIu32 "\n",
                        kr->event);
                break;
            }
            default:
                /* Unknown event type, ignore it */
                printf ("Unknown event: %" PRIu8 "\n",
                        event->response_type);
                break;
            }

            free (event);
        }
 
 

  pause();

  xcb_disconnect(connection);

  return 0;
}


static void
    testCookie (xcb_void_cookie_t cookie,
                xcb_connection_t *connection,
                const char *errMessage )
    {   
        xcb_generic_error_t *error = xcb_request_check (connection, cookie);
        if (error) {
            fprintf (stderr, "ERROR: %s : %" PRIu8 "\n", errMessage , error->error_code);
            xcb_disconnect (connection);
            exit (-1);
        }   
    }   

static void
  setCursor (xcb_connection_t *connection,
              xcb_screen_t     *screen,
              xcb_window_t      window,
              int               cursorId )
  {
      xcb_font_t font = xcb_generate_id (connection);
      xcb_void_cookie_t fontCookie = xcb_open_font_checked (connection,
                                                            font,
                                                            strlen ("cursor"),
                                                            "cursor" );
      testCookie (fontCookie, connection, "can't open font");
      xcb_cursor_t cursor = xcb_generate_id (connection);
      xcb_create_glyph_cursor (connection,
                               cursor,
                               font,
                               font,
                               cursorId,
                               cursorId + 1,
                               0, 0, 0, 0, 0, 0 );
      xcb_gcontext_t gc = xcb_generate_id (connection);
      uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
      uint32_t values_list[3];
      values_list[0] = screen->black_pixel;
      values_list[1] = screen->white_pixel;
      values_list[2] = font;
      xcb_void_cookie_t gcCookie = xcb_create_gc_checked (connection, gc, window, mask, values_list);
      testCookie (gcCookie, connection, "can't create gc");
      mask = XCB_CW_CURSOR;
      uint32_t value_list = cursor;
      xcb_change_window_attributes (connection, window, mask, &value_list);
      xcb_free_cursor (connection, cursor);
      fontCookie = xcb_close_font_checked (connection, font);
      testCookie (fontCookie, connection, "can't close font");
  }


void
print_modifiers (uint32_t mask)
{
    const char *MODIFIERS[] = {
            "Shift", "Lock", "Ctrl", "Alt",
            "Mod2", "Mod3", "Mod4", "Mod5",
            "Button1", "Button2", "Button3", "Button4", "Button5"
    };
    printf ("Modifier mask: ");
    for (const char **modifier = MODIFIERS ; mask; mask >>= 1, ++modifier) {
        if (mask & 1) {
           printf ("%s", *modifier);
        }
    }
   printf ("\n");
}

static void
setColormap (xcb_connection_t *connection, xcb_window_t window, xcb_screen_t *screen) 
{
  xcb_colormap_t colormapId = xcb_generate_id (connection);
  xcb_create_colormap (connection,
                       XCB_COLORMAP_ALLOC_NONE,
                       colormapId,
                       window,
                       screen -> root_visual);

  xcb_alloc_color (connection,
                   colormapId,
                   65535,
                   0,
                   0);
  printf ("setColormap working!\n");
  xcb_free_colormap(connection,
                    colormapId);
  printf ("Colormap freed.\n");
}

static void
setPixmap (xcb_connection_t *connection, xcb_window_t window, xcb_screen_t *screen) 
{
  xcb_pixmap_t pixmapId = xcb_generate_id (connection);
  
  xcb_create_pixmap (connection,
                     screen -> root_depth,
                     pixmapId,
                     window,
                     1280,
                     720);
  printf ("setPixmap working!\n");
  xcb_free_pixmap(connection,
                  pixmapId);
  printf ("Pixmap freed.\n");
}

static void drawRedDot(xcb_connection_t* connection,
                        xcb_window_t window,
                        xcb_screen_t* screen)
{
    // 1) Allocate a red pixel (get actual pixel from reply)
    xcb_colormap_t cm = xcb_generate_id(connection);
    xcb_create_colormap(connection,
                         XCB_COLORMAP_ALLOC_NONE,
                         cm,
                         window,
                         screen->root_visual);

    auto ck = xcb_alloc_color(connection, cm, 65535, 0, 0);
    xcb_alloc_color_reply_t* rep = xcb_alloc_color_reply(connection, ck, nullptr);
    if (!rep) return;
    uint32_t redPixel = rep->pixel;
    free(rep);

    // 2) Create a GC using that redPixel
    xcb_gcontext_t gc = xcb_generate_id(connection);
    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t values[] = { redPixel };
    xcb_create_gc(connection, gc, window, mask, values);

    // 3) Draw dot at center
    const uint16_t dotSize = 8;
    int16_t cx = static_cast<int16_t>(screen->width_in_pixels / 2);
    int16_t cy = static_cast<int16_t>(screen->height_in_pixels / 2);

    xcb_rectangle_t r;
    r.x = cx - (dotSize / 2);
    r.y = cy - (dotSize / 2);
    r.width = dotSize;
    r.height = dotSize;

    xcb_poly_fill_rectangle(connection, window, gc, 1, &r);
    xcb_flush(connection);

    // 4) Cleanup
    xcb_free_gc(connection, gc);
    xcb_free_colormap(connection, cm);
}
