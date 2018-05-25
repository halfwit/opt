#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <getopt.h>
#include <xcb/xcb.h>
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <locale.h>
#include "util.h"
#include "arg.h"

static xcb_connection_t *conn;
static xcb_screen_t *scr;
static uint32_t mask = XCB_EVENT_MASK_NO_EVENT
                     /* | XCB_EVENT_MASK_KEY_PRESS */
                     /* | XCB_EVENT_MASK_KEY_RELEASE */
                     /* | XCB_EVENT_MASK_BUTTON_PRESS */
                     /* | XCB_EVENT_MASK_BUTTON_RELEASE */
                     | XCB_EVENT_MASK_ENTER_WINDOW
                     /* | XCB_EVENT_MASK_LEAVE_WINDOW */
                     /* | XCB_EVENT_MASK_POINTER_MOTION */
                     /* | XCB_EVENT_MASK_POINTER_MOTION_HINT */
                     /* | XCB_EVENT_MASK_BUTTON_1_MOTION */
                     /* | XCB_EVENT_MASK_BUTTON_2_MOTION */
                     /* | XCB_EVENT_MASK_BUTTON_3_MOTION */
                     /* | XCB_EVENT_MASK_BUTTON_4_MOTION */
                     /* | XCB_EVENT_MASK_BUTTON_5_MOTION */
                     /* | XCB_EVENT_MASK_BUTTON_MOTION */
                     /* | XCB_EVENT_MASK_KEYMAP_STATE */
                     /* | XCB_EVENT_MASK_EXPOSURE */
                     /* | XCB_EVENT_MASK_VISIBILITY_CHANGE */
                     /* | XCB_EVENT_MASK_STRUCTURE_NOTIFY */
                     /* | XCB_EVENT_MASK_RESIZE_REDIRECT */
                     /* | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY */
                     /* | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT */
                     /* | XCB_EVENT_MASK_FOCUS_CHANGE */
                     /* | XCB_EVENT_MASK_PROPERTY_CHANGE */
                     /* | XCB_EVENT_MASK_COLOR_MAP_CHANGE */
                     /* | XCB_EVENT_MASK_OWNER_GRAB_BUTTON */
                     ;

struct xkb_keymap *keymap;
struct xkb_state *state;
struct xkb_context *ctx;
void usage(char*);
void list_events(void);
void handle_events(void);
void register_events(xcb_window_t, uint32_t);
xcb_window_t get_window_id(xcb_generic_event_t*);
int motherfuckingenterevent(xcb_generic_event_t*);

void
usage(char *name)
{
	fprintf(stderr, "usage: %s [-l] [-m <mask>]\n", name);
	exit(1);
}

void
list_events(void)
{
	printf(
		"XCB_EVENT_MASK_NO_EVENT .............. 0\n"
		"XCB_EVENT_MASK_KEY_PRESS ............. 1\n"
		"XCB_EVENT_MASK_KEY_RELEASE ........... 2\n"
		"XCB_EVENT_MASK_BUTTON_PRESS .......... 4\n"
		"XCB_EVENT_MASK_BUTTON_RELEASE ........ 8\n"
		"XCB_EVENT_MASK_ENTER_WINDOW .......... 16\n"
		"XCB_EVENT_MASK_LEAVE_WINDOW .......... 32\n"
		"XCB_EVENT_MASK_POINTER_MOTION ........ 64\n"
		"XCB_EVENT_MASK_POINTER_MOTION_HINT ... 128\n"
		"XCB_EVENT_MASK_BUTTON_1_MOTION ....... 256\n"
		"XCB_EVENT_MASK_BUTTON_2_MOTION ....... 512\n"
		"XCB_EVENT_MASK_BUTTON_3_MOTION ....... 1024\n"
		"XCB_EVENT_MASK_BUTTON_4_MOTION ....... 2048\n"
		"XCB_EVENT_MASK_BUTTON_5_MOTION ....... 4096\n"
		"XCB_EVENT_MASK_BUTTON_MOTION ......... 8192\n"
		"XCB_EVENT_MASK_KEYMAP_STATE .......... 16384\n"
		"XCB_EVENT_MASK_EXPOSURE .............. 32768\n"
		"XCB_EVENT_MASK_VISIBILITY_CHANGE ..... 65536\n"
		"XCB_EVENT_MASK_STRUCTURE_NOTIFY ...... 131072\n"
		"XCB_EVENT_MASK_RESIZE_REDIRECT ....... 262144\n"
		"XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY ... 524288\n"
		"XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT . 1048576\n"
		"XCB_EVENT_MASK_FOCUS_CHANGE .......... 2097152\n"
		"XCB_EVENT_MASK_PROPERTY_CHANGE ....... 4194304\n"
		"XCB_EVENT_MASK_COLOR_MAP_CHANGE ...... 8388608\n"
		"XCB_EVENT_MASK_OWNER_GRAB_BUTTON ..... 16777216\n"
		);
}

void
register_events(xcb_window_t w, uint32_t m)
{
	uint32_t val[] = { m };

	xcb_change_window_attributes(conn, w, XCB_CW_EVENT_MASK, val);
	xcb_flush(conn);
}

int
motherfuckingenterevent(xcb_generic_event_t *e)
{
	xcb_enter_notify_event_t *ee;

	ee = (xcb_enter_notify_event_t*)e;
	if (ee->detail == 0 && 
			(ee->mode == XCB_NOTIFY_MODE_NORMAL ||
			 ee->mode == XCB_NOTIFY_MODE_UNGRAB))
		return 1;

	return 0;
}

int
motherfuckingfocusevent(xcb_generic_event_t *e)
{
	xcb_focus_in_event_t *ee;

	ee = (xcb_focus_in_event_t*)e;
	if (ee->detail != XCB_NOTIFY_DETAIL_VIRTUAL &&
		ee->detail != XCB_NOTIFY_DETAIL_NONLINEAR_VIRTUAL &&
		ee->detail != XCB_NOTIFY_DETAIL_POINTER &&
		(ee->mode == XCB_NOTIFY_MODE_NORMAL ||
		 ee->mode == XCB_NOTIFY_MODE_WHILE_GRABBED))
		return 1;

	return 0;
}

xcb_window_t
get_window_id(xcb_generic_event_t *e)
{
	xcb_window_t wid;

	wid = 0x0;
	switch (e->response_type & ~0x80) {
		case XCB_ENTER_NOTIFY:
		case XCB_LEAVE_NOTIFY:
			if (motherfuckingenterevent(e))
				wid = ((xcb_enter_notify_event_t*)e)->event;
			break;

		case XCB_FOCUS_IN:
		case XCB_FOCUS_OUT:
			if (motherfuckingfocusevent(e))
				wid = ((xcb_focus_in_event_t*)e)->event;
			break;

		case XCB_CREATE_NOTIFY:
		case XCB_DESTROY_NOTIFY:
			wid = ((xcb_create_notify_event_t*)e)->window;
			break;

		case XCB_UNMAP_NOTIFY:
		case XCB_MAP_NOTIFY:
			wid = ((xcb_map_notify_event_t*)e)->window;
			break;

		case XCB_BUTTON_PRESS:
		case XCB_BUTTON_RELEASE:
			wid = ((xcb_button_press_event_t*)e)->child;
			break;

		/*
		 * MOTION_NOTIFY. Be careful with this one...
		 *         It triggers ALOT! 
		 *
		 *          |\.---./|_
	 	 *         (o)   (°) ''--._
	 	 *         / .A--.         '\
	 	 *         |'     `.         \
	 	 *         /                 \
		 *        /_;`|__\------(__/-\
		 */
		case XCB_MOTION_NOTIFY:
			wid = ((xcb_motion_notify_event_t*)e)->event;
			break;

		/* please... handle me ... ;_;
		case XCB_KEY_RELEASE:
		case XCB_KEYMAP_NOTIFY:
		case XCB_EXPOSE:
		case XCB_GRAPHICS_EXPOSURE:
		case XCB_NO_EXPOSURE:
		case XCB_VISIBILITY_NOTIFY:
		case XCB_MAP_REQUEST:
		case XCB_REPARENT_NOTIFY:
		case XCB_CONFIGURE_NOTIFY:
		case XCB_CONFIGURE_REQUEST:
		case XCB_GRAVITY_NOTIFY:
		case XCB_RESIZE_REQUEST:
		case XCB_CIRCULATE_NOTIFY:
		case XCB_CIRCULATE_REQUEST:
		case XCB_PROPERTY_NOTIFY:
		case XCB_SELECTION_CLEAR:
		case XCB_SELECTION_REQUEST:
		case XCB_SELECTION_NOTIFY:
		case XCB_COLORMAP_NOTIFY:
		case XCB_CLIENT_MESSAGE:
		case XCB_MAPPING_NOTIFY:
		*/
		default:
			return 0x0;
	}

	return wid;
}

void
handle_events(void)
{
	int i, wn;
	xcb_window_t *wc, wid = 0;
	xcb_generic_event_t *e;
	xcb_create_notify_event_t *ec;

	/*
	 * We need to get notifed of window creations, no matter what, because
	 * we need to register the event mask on all newly created windows
	 */
	register_events(scr->root, XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY);

	if (mask & XCB_EVENT_MASK_BUTTON_PRESS) {
		xcb_grab_button(conn, 0, scr->root,
		                XCB_EVENT_MASK_BUTTON_PRESS |
		                XCB_EVENT_MASK_BUTTON_RELEASE,
		                XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
		                scr->root, XCB_NONE, 1, XCB_NONE);
	}

	/* register the events on all mapped windows */
	wn = get_windows(conn, scr->root, &wc);
	for (i=0; i<wn; i++)
		register_events(wc[i], mask);

	xcb_flush(conn);
	xcb_keycode_t keycode;
	size_t size = 0;

	for(;;) {
		e = xcb_wait_for_event(conn);

		switch (e->response_type & ~0x80)
		{
			case XCB_KEY_PRESS:
				keycode = ((xcb_key_press_event_t*)e)->detail;
				char *buffer = malloc(xkb_state_key_get_utf8(state, keycode, NULL, size) + 1);
				xkb_state_key_get_utf8(state, keycode, buffer, sizeof(buffer));
				printf("%s\n", buffer);
				free(buffer);
			case XCB_CREATE_NOTIFY:
				ec = (xcb_create_notify_event_t*)e;
				register_events(ec->window, mask);
			default:
				wid = get_window_id(e);
		}

		if (wid > 0) {
			printf("%d:0x%08x\n", e->response_type, wid);
			fflush(stdout);
		}

		free(e);
	}
}

void
init_xkb(xcb_connection_t **conn) {
	uint8_t *first_xkb_event = NULL;
	xkb_x11_setup_xkb_extension(*conn, XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION, XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS, NULL, NULL, first_xkb_event, NULL);
	int32_t device_id = xkb_x11_get_core_keyboard_device_id(*conn);
	ctx = xkb_context_new(XKB_CONTEXT_NO_DEFAULT_INCLUDES | XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
	    enum {
        required_events =
            (XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
             XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
             XCB_XKB_EVENT_TYPE_STATE_NOTIFY),

        required_nkn_details =
            (XCB_XKB_NKN_DETAIL_KEYCODES),

        required_map_parts =
            (XCB_XKB_MAP_PART_KEY_TYPES |
             XCB_XKB_MAP_PART_KEY_SYMS |
             XCB_XKB_MAP_PART_MODIFIER_MAP |
             XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
             XCB_XKB_MAP_PART_KEY_ACTIONS |
             XCB_XKB_MAP_PART_VIRTUAL_MODS |
             XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP),

        required_state_details =
            (XCB_XKB_STATE_PART_MODIFIER_BASE |
             XCB_XKB_STATE_PART_MODIFIER_LATCH |
             XCB_XKB_STATE_PART_MODIFIER_LOCK |
             XCB_XKB_STATE_PART_GROUP_BASE |
             XCB_XKB_STATE_PART_GROUP_LATCH |
             XCB_XKB_STATE_PART_GROUP_LOCK),
    };

    static const xcb_xkb_select_events_details_t details = {
        .affectNewKeyboard = required_nkn_details,
        .newKeyboardDetails = required_nkn_details,
        .affectState = required_state_details,
        .stateDetails = required_state_details,
    };

	struct xkb_keymap *new_keymap = xkb_x11_keymap_new_from_device(ctx, *conn, device_id, 0);
    struct xkb_state *new_state = xkb_x11_state_new_from_device(new_keymap, *conn, device_id);

    xkb_state_unref(state);
    xkb_keymap_unref(keymap);
    keymap = new_keymap;
    state = new_state;

    xcb_void_cookie_t cookie = xcb_xkb_select_events_aux_checked(*conn, device_id, required_events, 0, 0, required_map_parts, required_map_parts, &details);

    xcb_request_check(*conn, cookie);

}

int
main (int argc, char **argv)
{

	char *argv0;

	ARGBEGIN {
		case 'l':
			list_events();
			exit(0);
		case 'm':
			  mask = strtoul(EARGF(usage(argv0)), NULL, 10);
			  break;
		default: usage(argv0);
	} ARGEND;

	init_xcb(&conn);
	get_screen(conn, &scr);

	if (mask & XCB_EVENT_MASK_KEY_PRESS) {
		init_xkb(&conn);
	}

	handle_events();

	if (mask & XCB_EVENT_MASK_KEY_PRESS) {
		xkb_state_unref(state);
		xkb_keymap_unref(keymap);
	}

	kill_xcb(&conn);

	return 0;
}
