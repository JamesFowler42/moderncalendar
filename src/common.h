#ifndef common_h
#define common_h

#include "pebble_os.h"

#define RECONNECT_KEY 0
#define REQUEST_CALENDAR_KEY 1
#define CLOCK_STYLE_KEY 2
#define CALENDAR_RESPONSE_KEY 3
#define ALERT_EVENT 10
#define RESTORE_DATE 50
#define SECOND_ALERT 60

#define CLOCK_STYLE_12H 1
#define CLOCK_STYLE_24H 2
	
#define MAX_EVENTS 15

#define STATUS_REQUEST 1
#define STATUS_REPLY 2
#define STATUS_ALERT_SET 3
	
#define	MAX_ALLOWABLE_ALERTS 10
	
typedef struct {
  uint8_t index;
  char title[21];
  bool has_location;
  char location[21];
  bool all_day;
  char start_date[18];
  int32_t alarms[2];
} Event;

typedef struct {
  AppTimerHandle handle;
  char event_desc[21];
  bool active;
  char relative_desc[21];
} TimerRecord;

#define REQUEST_CALENDAR_INTERVAL_MS 600003

void calendar_init(AppContextRef ctx);
void handle_calendar_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie);
void display_event_text(char *text, char *relative);
void draw_date();
void received_message(DictionaryIterator *received, AppContextRef context);
void set_status(int new_status_display);

#endif