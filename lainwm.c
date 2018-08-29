/* TinyWM is written by Nick Welch <mack@incise.org>, 2005.
 * TinyWM-XCB is rewritten by Ping-Hsun Chen <penkia@gmail.com>, 2010
 *
 * This software is in the public domain
 * and is provided AS IS, with NO WARRANTY. */


#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <xcb/randr.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <sys/wait.h>

#define LENGTH(x) (sizeof(x)/sizeof(*x))

uint32_t values[3];

xcb_generic_event_t *ev;
xcb_get_geometry_reply_t *geom;

xcb_connection_t *dpy;

xcb_screen_t *screen;
xcb_drawable_t win;
xcb_drawable_t root;
xcb_drawable_t focuswin;


key_t keys[] = 
   { XK_w,
     XK_e,
     XK_p,
     XCB_NO_SYMBOL     
   };

bool
setup_keyboard(void)
{
	xcb_get_modifier_mapping_reply_t *reply;
	xcb_keycode_t *modmap;

	reply = xcb_get_modifier_mapping_reply(dpy,
				xcb_get_modifier_mapping_unchecked(dpy), NULL);

	if (!reply)
		return false;

	modmap = xcb_get_modifier_mapping_keycodes(reply);

	if (!modmap)
		return false;

	xcb_flush(dpy);
	free(reply);
	return true;
}


/* wrapper to get xcb keycodes from keysymbol */
xcb_keycode_t * 
xcb_get_keycodes(xcb_keysym_t keysym)
{
	xcb_key_symbols_t *keysyms;
	xcb_keycode_t *keycode;
	
	if (!(keysyms = xcb_key_symbols_alloc(dpy)))
		return NULL;

	keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
	xcb_key_symbols_free(keysyms);

	return keycode;
}

/* wrapper to get xcb keysymbol from keycode */
static xcb_keysym_t
xcb_get_keysym(xcb_keycode_t keycode)
{
	xcb_key_symbols_t *keysyms;

	if (!(keysyms = xcb_key_symbols_alloc(dpy)))
		return 0;

	xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, keycode, 0);
	xcb_key_symbols_free(keysyms);

	return keysym;
}

void
grabkeys(void)
{
	xcb_keycode_t *keycode;
	int i, k;

	xcb_ungrab_key(dpy, XCB_GRAB_ANY, root, XCB_MOD_MASK_ANY);

	for (i=0; i<LENGTH(keys) ; i++) {
		keycode = xcb_get_keycodes(keys[i]);
	
           for (k=0; keycode[k] != XCB_NO_SYMBOL; k++) {
			xcb_grab_key(dpy, 1, root, XCB_MOD_MASK_4, keycode[k],
			XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC );
                      }         
	

        free(keycode); 
	}

        xcb_flush(dpy);
}


/* Move window win to root coordinates x,y. */
void movewindow(xcb_drawable_t win, uint16_t x, uint16_t y)
{
    uint32_t values[2];

    if (root == win || 0 == win)
    {
        /* Can't move root. */
        return;
    }
    
    values[0] = x;
    values[1] = y;
 
    xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X
                         | XCB_CONFIG_WINDOW_Y, values);
    
    xcb_flush(dpy);
}

void 
canmove(xcb_drawable_t win, xcb_keysym_t keysym) 
{
   if (win != 0) {

	 if (keysym == XK_w) 
             movewindow(win, 0, 0);
	 
	 if (keysym == XK_e)  
            movewindow(win, screen->width_in_pixels - geom->width, 0);
   }
}

void
setfocus(xcb_drawable_t win) 
{
	if (win == 0 || win == root)
		return;

	xcb_set_input_focus(dpy, XCB_INPUT_FOCUS_POINTER_ROOT, win,
				XCB_CURRENT_TIME);
	focuswin = win;
	geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, focuswin), NULL);
	xcb_flush(dpy);
}

/* Resize window win to width,height. */
void resize(xcb_drawable_t win, uint16_t width, uint16_t height)
{
    uint32_t values[2];

    if (screen->root == win || 0 == win)
    {
        /* Can't resize root. */
        return;
    }

    printf("Resizing to %d x %d.\n", width, height);
    
    values[0] = width;
    values[1] = height;
                                        
    xcb_configure_window(dpy, win,
                         XCB_CONFIG_WINDOW_WIDTH
                         | XCB_CONFIG_WINDOW_HEIGHT, values);
    xcb_flush(dpy);
}


void
newwin(xcb_window_t win) {
	uint32_t mask = 0;

	xcb_map_window(dpy, win);

	resize(win, 200, 200);
	 /* Declare window normal. */
	long data[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_NONE };
	//xcb_change_property(dpy, XCB_PROP_MODE_REPLACE, win,
	//wm_state, wm_state, 32, 2, data);

	 /* Subscribe to events we want to know about in this window. */
	mask = XCB_CW_EVENT_MASK;
	values[0] = XCB_EVENT_MASK_ENTER_WINDOW;
	xcb_change_window_attributes_checked(dpy, win, mask, values);
	setfocus(win);
	xcb_flush(dpy);
}

int
start(void)
{
     pid_t pid=fork();
    if (pid==0) { /* child process */
        static char *argv[]={"/usr/bin/dmenu_run",NULL ,NULL};
        execv(argv[0], argv);
        exit(127); /* only if execv fails */
    }
    else {  
       waitpid(pid,0,0); /* wait for child to exit */
    }
    return 0;
}

int main (int argc, char **argv)
{
    xcb_generic_error_t *error;
    xcb_void_cookie_t cookie;

    uint32_t mask = 0;

    dpy = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(dpy)) return 1;

    screen = xcb_setup_roots_iterator(xcb_get_setup(dpy)).data;
    root = screen->root;

      /* Subscribe to events. */
    mask = XCB_CW_EVENT_MASK;

    values[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
        | XCB_EVENT_MASK_STRUCTURE_NOTIFY
        | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
	| XCB_EVENT_MASK_PROPERTY_CHANGE;

    cookie =
        xcb_change_window_attributes_checked(dpy, root, mask, values);
	error = xcb_request_check(dpy, cookie);


    if (NULL != error)
	    exit(1);

    setup_keyboard();
    grabkeys();

    xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
				XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
				XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 1, XCB_MOD_MASK_1);
    xcb_grab_button(dpy, 0, root, XCB_EVENT_MASK_BUTTON_PRESS | 
				XCB_EVENT_MASK_BUTTON_RELEASE, XCB_GRAB_MODE_ASYNC, 
				XCB_GRAB_MODE_ASYNC, root, XCB_NONE, 3, XCB_MOD_MASK_1);

    xcb_flush(dpy);


   for (;;) {

	while(!(ev = xcb_wait_for_event(dpy)))
		xcb_flush(dpy);

	switch (ev->response_type & ~0x80) {
	case XCB_CONFIGURE_REQUEST:
	case XCB_MAP_REQUEST:
        {
            xcb_map_request_event_t *e;

            printf("event: Map request.\n");
            e = (xcb_map_request_event_t *) ev;
            newwin(e->window);
	    xcb_flush(dpy);
	}break;
	case XCB_KEY_PRESS: 
	{
	  xcb_key_press_event_t *e;
	  e = ( xcb_key_press_event_t *) ev;
	  win = e->child;
  
	  printf("Keycode: %d\n", e->detail);
	  geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, focuswin), NULL);
	  canmove(focuswin, xcb_get_keysym(e->detail));

	  if (xcb_get_keysym(e->detail) == XK_p)
	  	start();
	  
	  xcb_flush(dpy);
        
	}break;
	case XCB_KEY_RELEASE:
	{
	    xcb_flush(dpy);
	}break;
        case XCB_BUTTON_PRESS:
        {
            xcb_button_press_event_t *e;
            e = ( xcb_button_press_event_t *) ev;
	    printf ("Button press = %d, Modifier = %d\n", e->detail, e->state);	
	    win = e->child;

            values[0] = XCB_STACK_MODE_ABOVE;
    		xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_STACK_MODE, values);
 		geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);

         	printf("%d\n", e->child);
	        printf ("e->response_type = %d, e->sequence = %d, e->detail = %d, e->state = %d\n", e->response_type, e->sequence, e->detail, e->state);
			if (1 == e->detail) {
				values[2] = 1; 
				 xcb_warp_pointer(dpy, XCB_NONE, win, 0, 0, 0, 0, 1, 1); 
			} else if (win != 0) {
				values[2] = 3; 
				 xcb_warp_pointer(dpy, XCB_NONE, win, 0, 0, 0, 0, geom->width, geom->height); 
			}
			xcb_grab_pointer(dpy, 0, root, XCB_EVENT_MASK_BUTTON_RELEASE
					| XCB_EVENT_MASK_BUTTON_MOTION | XCB_EVENT_MASK_POINTER_MOTION_HINT, 
					XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, root, XCB_NONE, XCB_CURRENT_TIME);
			xcb_flush(dpy);
			
        }break;
	case XCB_ENTER_NOTIFY:
	{
		xcb_enter_notify_event_t *enter = (xcb_enter_notify_event_t *)ev;
		printf("mouse did something");
		setfocus(enter->event);	
	        xcb_flush(dpy);
	}break;
        case XCB_MOTION_NOTIFY:
        {
            xcb_query_pointer_reply_t *pointer;
            pointer = xcb_query_pointer_reply(dpy, xcb_query_pointer(dpy, root), 0);

            if (values[2] == 1 && win != 0) {/* move */
               	geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);
				values[0] = (pointer->root_x + geom->width > screen->width_in_pixels)?
					(screen->width_in_pixels - geom->width):pointer->root_x;
    			values[1] = (pointer->root_y + geom->height > screen->height_in_pixels)?
					(screen->height_in_pixels - geom->height):pointer->root_y;
				xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
				xcb_flush(dpy);
            } else if (values[2] == 3 && win != 0) { /* resize */
				geom = xcb_get_geometry_reply(dpy, xcb_get_geometry(dpy, win), NULL);
				values[0] = pointer->root_x - geom->x;
				values[1] = pointer->root_y - geom->y;
				xcb_configure_window(dpy, win, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
				xcb_flush(dpy);
            }
	}break;
        case XCB_BUTTON_RELEASE:
	{
		xcb_ungrab_pointer(dpy, XCB_CURRENT_TIME);
		xcb_flush(dpy); 
	}break;
	default: {
	   printf("idk what to do \n");
	}break;
        }
	free(ev);
    }
return 0;
}
