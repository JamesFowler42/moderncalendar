#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "common.h"

/*
 * Version 1.0 - Initial version
 * Version 1.1 - Altered to ensure that it cannot issue too many alerts and blow up the Pebble
 */
	
#define MY_UUID { 0x69, 0x8B, 0x3E, 0x04, 0xB1, 0x2E, 0x4F, 0xF5, 0xBF, 0xAD, 0x1B, 0xE6, 0xBD, 0xFE, 0xB4, 0xD7 }
PBL_APP_INFO(MY_UUID,
             "Modern Calendar", "Zalew/Fowler/Baeumle",
             1, 1, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);

Window window;
BmpContainer background_image_container;

Layer minute_display_layer;
Layer hour_display_layer;
Layer center_display_layer;
Layer second_display_layer;
TextLayer date_layer;
TextLayer date_layer2;
static char date_text[] = "Wed 13 ";
static char empty_str[] = "";
static char state_text[21]; 
static char state_text2[21];

HeapBitmap icon_status_1;
HeapBitmap icon_status_2;
HeapBitmap icon_status_3;

Layer status_layer;
int status_display = 0;

const GPathInfo MINUTE_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
    {-4, 15},
    {4, 15},
    {4, -70},
    {-4,  -70},
  }
};


const GPathInfo HOUR_HAND_PATH_POINTS = {
  4,
  (GPoint []) {
    {-4, 15},
    {4, 15},
    {4, -50},
    {-4,  -50},
  }
};


GPath hour_hand_path;
GPath minute_hand_path;

AppTimerHandle timer_handle;
#define COOKIE_MY_TIMER 1
#define ANIM_IDLE 0
#define ANIM_START 1
#define ANIM_HOURS 2
#define ANIM_MINUTES 3
#define ANIM_SECONDS 4
#define ANIM_DONE 5
int init_anim = ANIM_DONE;
int32_t second_angle_anim = 0;
unsigned int minute_angle_anim = 0;
unsigned int hour_angle_anim = 0;

void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
  (void)ctx;
  (void)handle;

  if (cookie == COOKIE_MY_TIMER) {
    if(init_anim == ANIM_START)
    {
        init_anim = ANIM_HOURS;
        timer_handle = app_timer_send_event(ctx, 50 /* milliseconds */, COOKIE_MY_TIMER);
    }
    else if(init_anim==ANIM_HOURS)
    {
        layer_mark_dirty(&hour_display_layer);
        timer_handle = app_timer_send_event(ctx, 50 /* milliseconds */, COOKIE_MY_TIMER);
    }
    else if(init_anim==ANIM_MINUTES)
    {
        layer_mark_dirty(&minute_display_layer);
        timer_handle = app_timer_send_event(ctx, 50 /* milliseconds */, COOKIE_MY_TIMER);
    }
    else if(init_anim==ANIM_SECONDS)
    {
        layer_mark_dirty(&second_display_layer);
        timer_handle = app_timer_send_event(ctx, 50 /* milliseconds */, COOKIE_MY_TIMER);
    }
  }
	
  handle_calendar_timer(ctx, handle, cookie);

}

void second_display_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  PblTm t;
  get_time(&t);

  int32_t second_angle = t.tm_sec * (0xffff/60);
  int second_hand_length = 70;
  GPoint center = grect_center_point(&me->frame);
  GPoint second = GPoint(center.x, center.y - second_hand_length);

  if(init_anim < ANIM_SECONDS)
  {
     second = GPoint(center.x, center.y - 70);
  }
  else if(init_anim==ANIM_SECONDS)
  {
     second_angle_anim += 0xffff/60;
     if(second_angle_anim >= second_angle)
     {
        init_anim = ANIM_DONE;
        second = GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
				center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
     }
     else
     {
        second = GPoint(center.x + second_hand_length * sin_lookup(second_angle_anim)/0xffff,
				center.y + (-second_hand_length) * cos_lookup(second_angle_anim)/0xffff);
     }
  }
  else 
  {
     second = GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
			center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
  }

  graphics_context_set_stroke_color(ctx, GColorWhite);

  graphics_draw_line(ctx, center, second);
}

void center_display_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  GPoint center = grect_center_point(&me->frame);
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, 4);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, center, 3);
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  PblTm t;

  get_time(&t);

  unsigned int angle = t.tm_min * 6 + t.tm_sec / 10;

  if(init_anim < ANIM_MINUTES)
  {
     angle = 0;
  }
  else if(init_anim==ANIM_MINUTES)
  {
     minute_angle_anim += 6;
     if(minute_angle_anim >= angle)
     {
        init_anim = ANIM_SECONDS;
     }
     else
     {
        angle = minute_angle_anim;
     }
  }

  gpath_rotate_to(&minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_draw_filled(ctx, &minute_hand_path);
  gpath_draw_outline(ctx, &minute_hand_path);
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  PblTm t;

  get_time(&t);

  unsigned int angle = t.tm_hour * 30 + t.tm_min / 2;

  if(init_anim < ANIM_HOURS)
  {
     angle = 0;
  }
  else if(init_anim==ANIM_HOURS)
  {
     if(hour_angle_anim==0&&t.tm_hour>=12)
     {
        hour_angle_anim = 360;
     }
     hour_angle_anim += 6;
     if(hour_angle_anim >= angle)
     {
        init_anim = ANIM_MINUTES;
     }
     else
     {
        angle = hour_angle_anim;
     }
  }

  gpath_rotate_to(&hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);

  gpath_draw_filled(ctx, &hour_hand_path);
  gpath_draw_outline(ctx, &hour_hand_path);
}
void draw_date(){
  PblTm t;
  get_time(&t);

  string_format_time(date_text, sizeof(date_text), "%a %d", &t);
  
  text_layer_set_text(&date_layer, date_text);
  text_layer_set_text(&date_layer2, empty_str);
}

void display_event_text(char *text, char *relative) {
  strncpy(state_text2, relative, sizeof(state_text2));
  text_layer_set_text(&date_layer2, state_text2);
  strncpy(state_text, text, sizeof(state_text));
  text_layer_set_text(&date_layer, state_text);
}

/*
 * Status icon callback handler
 */
void status_layer_update_callback(Layer *layer, GContext *ctx) {
  
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	
  if (status_display == STATUS_REQUEST) {
     graphics_draw_bitmap_in_rect(ctx, &icon_status_1.bmp, GRect(0, 0, 38, 9));
  } else if (status_display == STATUS_REPLY) {
     graphics_draw_bitmap_in_rect(ctx, &icon_status_2.bmp, GRect(0, 0, 38, 9));
  } else if (status_display == STATUS_ALERT_SET) {
     graphics_draw_bitmap_in_rect(ctx, &icon_status_3.bmp, GRect(0, 0, 38, 9));
  }
}

void set_status(int new_status_display) {
	status_display = new_status_display;
	layer_mark_dirty(&status_layer);
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

  window_init(&window, "Modern Watch");
  window_stack_push(&window, true /* Animated */);
  resource_init_current_app(&APP_RESOURCES);

  bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image_container);
  layer_add_child(&window.layer, &background_image_container.layer.layer);
	
  heap_bitmap_init(&icon_status_1, RESOURCE_ID_IMAGE_STATUS_1);
  heap_bitmap_init(&icon_status_2, RESOURCE_ID_IMAGE_STATUS_2);
  heap_bitmap_init(&icon_status_3, RESOURCE_ID_IMAGE_STATUS_3);

  text_layer_init(&date_layer, GRect(27, 112, 90, 21));
  text_layer_set_text_color(&date_layer, GColorWhite);
  text_layer_set_text_alignment(&date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(&date_layer, GColorClear);
  text_layer_set_font(&date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(&window.layer, &date_layer.layer);
	
  text_layer_init(&date_layer2, GRect(27, 93, 90, 21));
  text_layer_set_text_color(&date_layer2, GColorWhite);
  text_layer_set_text_alignment(&date_layer2, GTextAlignmentCenter);
  text_layer_set_background_color(&date_layer2, GColorClear);
  text_layer_set_font(&date_layer2, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  layer_add_child(&window.layer, &date_layer2.layer);

  draw_date();
	
  GRect sframe;
  sframe.origin.x = 54;
  sframe.origin.y = 56;
  sframe.size.w = 38;
  sframe.size.h = 9;

  layer_init(&status_layer, sframe);
  status_layer.update_proc = &status_layer_update_callback;
  layer_add_child(&window.layer, &status_layer);

  layer_init(&hour_display_layer, window.layer.frame);
  hour_display_layer.update_proc = &hour_display_layer_update_callback;
  layer_add_child(&window.layer, &hour_display_layer);

  gpath_init(&hour_hand_path, &HOUR_HAND_PATH_POINTS);
  gpath_move_to(&hour_hand_path, grect_center_point(&hour_display_layer.frame));

  layer_init(&minute_display_layer, window.layer.frame);
  minute_display_layer.update_proc = &minute_display_layer_update_callback;
  layer_add_child(&window.layer, &minute_display_layer);

  gpath_init(&minute_hand_path, &MINUTE_HAND_PATH_POINTS);
  gpath_move_to(&minute_hand_path, grect_center_point(&minute_display_layer.frame));

  layer_init(&center_display_layer, window.layer.frame);
  center_display_layer.update_proc = &center_display_layer_update_callback;
  layer_add_child(&window.layer, &center_display_layer);

  layer_init(&second_display_layer, window.layer.frame);
  second_display_layer.update_proc = &second_display_layer_update_callback;
  layer_add_child(&window.layer, &second_display_layer);
	
  calendar_init(ctx);
}

void handle_deinit(AppContextRef ctx) {
  (void)ctx;

  bmp_deinit_container(&background_image_container);
  heap_bitmap_deinit(&icon_status_1);
  heap_bitmap_deinit(&icon_status_2);
  heap_bitmap_deinit(&icon_status_3);
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *t){
  (void)t;
  (void)ctx;

  if(init_anim == ANIM_IDLE)
  {
     init_anim = ANIM_START;
     timer_handle = app_timer_send_event(ctx, 50 /* milliseconds */, COOKIE_MY_TIMER);
  }
  else if(init_anim == ANIM_DONE)
  {
  if(t->tick_time->tm_sec%10==0)
  {
     layer_mark_dirty(&minute_display_layer);
     
     if(t->tick_time->tm_sec==0)
     {
        if(t->tick_time->tm_min%2==0)
        {
           layer_mark_dirty(&hour_display_layer);
           if(t->tick_time->tm_min==0&&t->tick_time->tm_hour==0)
           {
              draw_date();
           }
        }
     }
  }

  layer_mark_dirty(&second_display_layer);
  }
}

/*
 * Main - or at least the pebble land main
 */
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
	.messaging_info = (PebbleAppMessagingInfo){
      .buffer_sizes = {
        .inbound = 124,
        .outbound = 256,
      },
      .default_callbacks.callbacks = {
        .in_received = received_message,
      },
    },
    .timer_handler = &handle_timer,
    .tick_info = {
			.tick_handler = &handle_tick,
			.tick_units = SECOND_UNIT
		}
  };
  app_event_loop(params, &handlers);
}

