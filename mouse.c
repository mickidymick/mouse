#include <yed/plugin.h>

static char *mouse_buttons[] = {
    "LEFT",
    "MIDDLE",
    "RIGHT",
    "WHEEL UP",
    "WHEEL DOWN",
};

static char *mouse_actions[] = {
    "PRESS",
    "RELEASE",
    "DRAG",
};

static int if_dragging;
static yed_frame *drag_frame;

static void mouse(yed_event* event);
static void mouse_unload(yed_plugin *self);

static yed_frame *find_frame(yed_event* event);
static int yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame);
static void left_click(yed_event* event);
static void left_drag(yed_event* event);
static void left_release(yed_event* event);
static void scroll_up(yed_event* event);
static void scroll_down(yed_event* event);


int yed_plugin_boot(yed_plugin *self) {
    yed_plugin_request_mouse_reporting(self);
    yed_event_handler mouse_eh;

    YED_PLUG_VERSION_CHECK();

    if_dragging = 0;

    mouse_eh.kind = EVENT_KEY_PRESSED;
    mouse_eh.fn   = mouse;

    if (yed_get_var("mouse-cursor-scroll") == NULL) {
        yed_set_var("mouse-cursor-scroll", "no");
    }

    if (yed_get_var("mouse-scroll-num-lines") == NULL) {
        yed_set_var("mouse-scroll-num-lines", "1");
    }

    yed_plugin_add_event_handler(self, mouse_eh);
    yed_plugin_set_unload_fn(self, mouse_unload);

    return 0;
}

int yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame) {
    return    (row >= frame->top  && row <= frame->top  + frame->height - 1)
           && (col >= frame->left && col <= frame->left + frame->width  - 1);
}

//see if cursor is inside of active frame
//in reverse order find first one that the cursor is inside
yed_frame *find_frame(yed_event* event) {
    yed_frame **frame_it;

    if (yed_cell_is_in_frame_mouse(MOUSE_ROW(event->key), MOUSE_COL(event->key), ys->active_frame)) {
        return ys->active_frame;
    }

    array_rtraverse(ys->frames, frame_it) {
        if (yed_cell_is_in_frame_mouse(MOUSE_ROW(event->key), MOUSE_COL(event->key), (*frame_it))) {
            return (*frame_it);
        }
    }

    return NULL;
}

static void left_click(yed_event* event) {
    yed_frame *frame;
    yed_event  mouse_left_click_event;

    memset(&mouse_left_click_event, 0, sizeof(mouse_left_click_event));
    mouse_left_click_event.kind                      = EVENT_PLUGIN_MESSAGE;
    mouse_left_click_event.plugin_message.message_id = "mouse-left-click";
    mouse_left_click_event.plugin_message.plugin_id  = "mouse";
    mouse_left_click_event.key                       = event->key;
    yed_trigger_event(&mouse_left_click_event);

    if(mouse_left_click_event.cancel) {
        return;
    }
    frame = find_frame(event);
    if (frame == NULL) {
        if_dragging = 0;
        YEXE("select-off");
        return;
    }
    yed_activate_frame(frame);
    yed_set_cursor_within_frame(frame, MOUSE_ROW(event->key) - frame->top + frame->buffer_y_offset + 1,
                                        MOUSE_COL(event->key) - frame->left + frame->buffer_x_offset - frame->gutter_width + 1);
    YEXE("select-off");
    event->cancel = 1;
}

static void left_drag(yed_event* event) {
    yed_frame *frame;
    yed_event  mouse_left_drag_event;

    memset(&mouse_left_drag_event, 0, sizeof(mouse_left_drag_event));
    mouse_left_drag_event.kind                      = EVENT_PLUGIN_MESSAGE;
    mouse_left_drag_event.plugin_message.message_id = "mouse-left-drag";
    mouse_left_drag_event.plugin_message.plugin_id  = "mouse";
    mouse_left_drag_event.key                       = event->key;
    yed_trigger_event(&mouse_left_drag_event);

    if(mouse_left_drag_event.cancel) {
        return;
    }
    if (if_dragging == 0) {
        if_dragging = 1;
        frame = find_frame(event);
        if (frame == NULL) {
            if_dragging = 0;
            YEXE("select-off");
            return;
        }
        yed_activate_frame(frame);
        yed_set_cursor_within_frame(frame, MOUSE_ROW(event->key) - frame->top + frame->buffer_y_offset + 1,
                                        MOUSE_COL(event->key) - frame->left + frame->buffer_x_offset - frame->gutter_width + 1);
        drag_frame = frame;
        YEXE("select");
    }else {
        frame = find_frame(event);
        if (frame != drag_frame) {
            if (MOUSE_COL(event->key) >= (drag_frame->bleft + drag_frame->bwidth - 1)) {
                yed_move_cursor_within_active_frame(0, 1);
            }
        }else {
            yed_set_cursor_within_frame(frame, MOUSE_ROW(event->key) - frame->top + frame->buffer_y_offset + 1,
                                            MOUSE_COL(event->key) - frame->left + frame->buffer_x_offset - frame->gutter_width + 1);
        }
    }
    event->cancel = 1;
}

static void left_release(yed_event* event) {
    yed_event  mouse_left_release_event;

    memset(&mouse_left_release_event, 0, sizeof(mouse_left_release_event));
    mouse_left_release_event.kind                      = EVENT_PLUGIN_MESSAGE;
    mouse_left_release_event.plugin_message.message_id = "mouse-left-release";
    mouse_left_release_event.plugin_message.plugin_id  = "mouse";
    mouse_left_release_event.key                       = event->key;
    yed_trigger_event(&mouse_left_release_event);

    if(mouse_left_release_event.cancel) {
        return;
    }
    if_dragging = 0;
    event->cancel = 1;
}

static void scroll_up(yed_event* event) {
    int tmp;
    yed_event  mouse_scroll_up_event;

    memset(&mouse_scroll_up_event, 0, sizeof(mouse_scroll_up_event));
    mouse_scroll_up_event.kind                      = EVENT_PLUGIN_MESSAGE;
    mouse_scroll_up_event.plugin_message.message_id = "mouse-scroll-up";
    mouse_scroll_up_event.plugin_message.plugin_id  = "mouse";
    mouse_scroll_up_event.key                       = event->key;
    yed_trigger_event(&mouse_scroll_up_event);

    if(mouse_scroll_up_event.cancel) {
        return;
    }

    if (yed_var_is_truthy("mouse-cursor-scroll")) {
        if(yed_get_var_as_int("mouse-scroll-num-lines", &tmp)) {
            yed_move_cursor_within_active_frame(-tmp, 0);
        }else{
            yed_move_cursor_within_active_frame(-1, 0);
        }
    }else{
        if(yed_get_var_as_int("mouse-scroll-num-lines", &tmp)) {
            yed_frame_scroll_buffer(ys->active_frame, -tmp);
        }else{
            yed_frame_scroll_buffer(ys->active_frame, -1);
        }
    }
    event->cancel = 1;
}

static void scroll_down(yed_event* event) {
    int tmp;
    yed_event  mouse_scroll_down_event;

    memset(&mouse_scroll_down_event, 0, sizeof(mouse_scroll_down_event));
    mouse_scroll_down_event.kind                      = EVENT_PLUGIN_MESSAGE;
    mouse_scroll_down_event.plugin_message.message_id = "mouse-scroll-down";
    mouse_scroll_down_event.plugin_message.plugin_id  = "mouse";
    mouse_scroll_down_event.key                       = event->key;
    yed_trigger_event(&mouse_scroll_down_event);

    if(mouse_scroll_down_event.cancel) {
        return;
    }

    if (yed_var_is_truthy("mouse-cursor-scroll")) {
        if(yed_get_var_as_int("mouse-scroll-num-lines", &tmp)) {
            yed_move_cursor_within_active_frame(tmp, 0);
        }else{
            yed_move_cursor_within_active_frame(1, 0);
        }
    }else{
        if(yed_get_var_as_int("mouse-scroll-num-lines", &tmp)) {
            yed_frame_scroll_buffer(ys->active_frame, tmp);
        }else{
            yed_frame_scroll_buffer(ys->active_frame, 1);
        }
    }
    event->cancel = 1;
}

static void mouse(yed_event* event) {
    if(ys->active_frame == NULL) {
        return;
    }

    LOG_FN_ENTER();
    if (IS_MOUSE(event->key)) {
/*         yed_log("MOUSE: %s %s %d %d\n", */
/*                 mouse_buttons[MOUSE_BUTTON(event->key)], */
/*                 mouse_actions[MOUSE_KIND(event->key)], */
/*                 MOUSE_ROW(event->key), */
/*                 MOUSE_COL(event->key)); */

        switch (MOUSE_BUTTON(event->key)) {
            case MOUSE_BUTTON_LEFT:
                if (MOUSE_KIND(event->key) == MOUSE_PRESS) {
                    left_click(event);

                }else if (MOUSE_KIND(event->key) == MOUSE_DRAG) {
                    left_drag(event);

                }else if(MOUSE_KIND(event->key) == MOUSE_RELEASE) {
                    left_release(event);
                }
                break;
            case MOUSE_WHEEL_DOWN:
                    scroll_down(event);
                break;
            case MOUSE_WHEEL_UP:
                    scroll_up(event);
                break;
        }
    }
    LOG_EXIT();
}

void mouse_unload(yed_plugin *self) {
}
