#include <iostream>
#include <stdio.h>
#include <inttypes.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>
#include <unistd.h>

short currentDesktop;
bool floatingWindows;





int main() {
  
  std::cout << "Subscribe to Semga!" << std::endl;

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
  values[0] = screen -> black_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE;

  xcb_create_window (connection, /* Connection */
                     XCB_COPY_FROM_PARENT, /*depth, idk what is this but ill tag this */
                     window, /* id of my window */
                     screen -> root, /* parent of window */
                     0, 0, /* x and y of window */
                     800, 400, /* width and height of window */
                     10, /* border width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class, idk what is this too */
                     screen -> root_visual, /* visual, idk what is this too */
                     mask, values); /* masks, still aren't used */
  xcb_map_window (connection, window);
  xcb_flush (connection);



  
  
  printf ("\n");
  printf ("Informations of screen %" PRIu32 ":\n", screen->root);
  printf ("  width: %" PRIu16 "\n", screen->width_in_pixels);
  printf ("  height: %" PRIu16 "\n", screen->height_in_pixels);
  printf ("  white pixel: %" PRIu32 "\n", screen->white_pixel);
  printf ("  black pixel: %" PRIu32 "\n", screen->black_pixel);
  printf ("\n"); 
  pause();

  xcb_disconnect(connection);

  return 0;
}
