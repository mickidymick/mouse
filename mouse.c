#include <yed/plugin.h>
is
static int        if_dragging;
static yed_frame *drag_frame;
static u64        last_timestamp;

static void       mouse(yed_event* event);
static void       mouse_unload(yed_plugin *self);
static yed_frame *find_frame(yed_event* event);
static int        yed_cell_is_in_frame_mouse(int row, int col, yed_frame *frame);
static void       left_click(yed_event* event);
static void       double_left_click(yed_event* event);
static void       left_drag(yed_event* event);
static void       left_release(yed_event* event);
static void       scroll_up(yed_event* event);
static void       scroll_down(yed_event* event);


int yed_plugin_boot(yed_plugin *self) {
    yed_plugin_request_mouse_reporting(self);
    yed_event_handler mouse_eh;

    YED_PLUG_VERSION_CHECK();

    if_dragging = 0;
    last_timestamp = measure_time_now_ms();

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

static void double_left_click(yed_event* event) {
    yed_frame *frame;
    int        key;

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
    key = ENTER;
    yed_feed_keys(1, &key);
}

static void left_drag(yed_event* event) {
    yed_frame *frame;

    if (if_dragging == 0) {
        if_dragging = 1;
        frame = find_frame(event);
        if (frame == NULL) {
            if_dragging = 0;
            YEXE("select-off");
            return;
        }
        yed_activate_frame(frame);
        YEXE("select");
        yed_set_cursor_within_frame(frame, MOUSE_ROW(event->key) - frame->top + frame->buffer_y_offset + 1,
                                        MOUSE_COL(event->key) - frame->left + frame->buffer_x_offset - frame->gutter_width + 1);
        drag_frame = frame;
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
    if_dragging = 0;
    event->cancel = 1;
}

static void scroll_up(yed_event* event) {
    int tmp;

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
    u64 cur_timestamp;

    if(ys->active_frame == NULL) {
        return;
    }

    if (IS_MOUSE(event->key)) {
        switch (MOUSE_BUTTON(event->key)) {
            case MOUSE_BUTTON_LEFT:
                if (MOUSE_KIND(event->key) == MOUSE_PRESS) {
                    cur_timestamp = measure_time_now_ms();
                    if((cur_timestamp - last_timestamp) <= 300) {
                        double_left_click(event);
                    }else{
                        left_click(event);
                    }
                    last_timestamp = cur_timestamp;
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
}

void mouse_unload(yed_plugin *self) {
}
