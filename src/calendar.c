#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "common.h"

int8_t num_rows;
Event events[MAX_EVENTS];
uint8_t count;
uint8_t received_rows;
Event event;
Event temp_event;
char event_date[50];
bool bt_ok = false;
int entry_no = 0;
int max_entries = 0;

AppContextRef app_context;

bool calendar_request_outstanding = false;

TimerRecord timer_rec[MAX_EVENTS * 2];

/*
 * Make a calendar request
 */
void calendar_request(DictionaryIterator *iter) {
  dict_write_int8(iter, REQUEST_CALENDAR_KEY, -1);
  dict_write_uint8(iter, CLOCK_STYLE_KEY, CLOCK_STYLE_24H);
  count = 0;
  received_rows = 0;
  calendar_request_outstanding = true;
  app_message_out_send();
  app_message_out_release();
  set_status(STATUS_REQUEST);
}

/*
 * Get the calendar running
 */
void calendar_init(AppContextRef ctx) {
  app_context = ctx;
  app_timer_send_event(ctx, 500, REQUEST_CALENDAR_KEY);
}

/*
 * Crude conversion of character strings to integer
 */
int a_to_i(char *val, int len) {
	int result = 0;
	for (int i=0; i < len; i++) {
		if (val[i] < '0' || val[i] > '9')
			break;
		result = result * 10;
		result = result + (val[i]-'0');
	}
	return result;
}

/*
 * is the date provided today?
 */
bool is_date_today(char *date) {
	char temp[6];
	PblTm time;
  	  get_time(&time);
	  string_format_time(temp, sizeof(temp), "%m/%d", &time);
	if (strncmp(date, temp, 5) == 0)
		return true;
	else
		return false;
}

/*
 * Queue an alert
 */
void queue_alert(int num, int32_t alarm_time, char *title, int32_t alert_event) {
  // work out relative time
  char relative_temp[21];
  if (alarm_time == 0)
       strncpy(relative_temp, "Now", sizeof(relative_temp));
  else if (alarm_time <  3600)
       snprintf(relative_temp, sizeof(relative_temp), "In %ld mins", alarm_time / 60);
  else 
       snprintf(relative_temp, sizeof(relative_temp), "In %ld hours", alarm_time / 3600);

  // Create an alert
  timer_rec[num].handle = app_timer_send_event(app_context, alert_event, ALERT_EVENT + num);
  strncpy(timer_rec[num].event_desc, event.title, sizeof(event.title)); 
  timer_rec[num].active = true;
  strncpy(timer_rec[num].relative_desc, relative_temp, sizeof(relative_temp));
}

/*
 * Do we need an alert? if so schedule one. 
 */
int determine_if_alarm_needed(int num) {
  // Copy the right event across
  memset(&event, 0, sizeof(Event));
  memcpy(&event, &events[num], sizeof(Event));
	
  // Alarms set
  int alarms_set = 0;
	
  // Ignore all day events
	if (event.all_day) {
	  return alarms_set; 
	}

  // Is the event today
  if (is_date_today(event.start_date) == false) {
	  return alarms_set;
  }
 	
  // Does the event have an alarm
  if (event.alarms[0] == -1 && event.alarms[1] == -1) {
	  return alarms_set;
  }

  // Compute the event start time as a figure in ms
  int time_position = 9;
  if (event.start_date[5] != '/')
		time_position = 6;

  int hour = a_to_i(&event.start_date[time_position],2);
  int minute_position = time_position + 3;
  if (event.start_date[time_position + 1] == ':')
	  minute_position = time_position + 2;
  int minute = a_to_i(&event.start_date[minute_position],2);
	
  uint32_t event_in_ms = (hour * 3600 + minute * 60) * 1000;
	
  // Get now as ms
  PblTm time;
  get_time(&time);
  uint32_t now_in_ms = (time.tm_hour * 3600 + time.tm_min * 60 + time.tm_sec) * 1000;

  // First alart
  if (event.alarms[0] != -1) {
  	// Work out the alert interval  
  	int32_t alert_event = event_in_ms - now_in_ms - (event.alarms[0] * 1000);

  	// If this is negative then we are after the alert period
  	if (alert_event >= 0) {
		queue_alert(num, event.alarms[0], event.title, alert_event);
		alarms_set++;
    }
  }

  // Second alart
  if (event.alarms[1] != -1) {
  	// Work out the alert interval  
  	int32_t alert_event = event_in_ms - now_in_ms - (event.alarms[1] * 1000);

  	// If this is negative then we are after the alert period
  	if (alert_event >= 0) {
		queue_alert(num + 15, event.alarms[1], event.title, alert_event);
		alarms_set++;
    }
  }

  return alarms_set;
}

/*
 * Clear existing timers
 */
void clear_timers() {
	for (int i=0; i < (MAX_EVENTS *2); i++) {
		if (timer_rec[i].active) {
			timer_rec[i].active = false;
			memset(timer_rec[i].event_desc, 0, sizeof(event.title));
			app_timer_cancel_event(app_context, timer_rec[i].handle);
            memset(timer_rec[i].relative_desc, 0, sizeof(event.title));
		}
	}
}

/*
 * Work through events returned from iphone
 */
void process_events() {
  if (calendar_request_outstanding || max_entries == 0) {
    clear_timers();	
  } else {
    clear_timers();	
	int alerts = 0;
    for (int entry_no = 0; entry_no < max_entries; entry_no++) 
	  alerts = alerts + determine_if_alarm_needed(entry_no);
	if (alerts > 0) 
		set_status(STATUS_ALERT_SET);
   }
}

/*
 * Messages incoming from the phone
 */
void received_message(DictionaryIterator *received, AppContextRef context) {
 
   // Gather the bits of a calendar together	
   Tuple *tuple = dict_find(received, CALENDAR_RESPONSE_KEY);
	  
   if (tuple) {
	    set_status(STATUS_REPLY);
    	uint8_t i, j;

		if (count > received_rows) {
      		i = received_rows;
      		j = 0;
        } else {
      	    count = tuple->value->data[0];
      	    i = 0;
      	    j = 1;
        }

        while (i < count && j < tuple->length) {
    	    memcpy(&temp_event, &tuple->value->data[j], sizeof(Event));
      	    memcpy(&events[temp_event.index], &temp_event, sizeof(Event));

      	    i++;
      	    j += sizeof(Event);
        }

        received_rows = i;

        if (count == received_rows) {
			max_entries = count;
			calendar_request_outstanding = false;
			process_events();
	    }
	}

}



/*
 * Timer handling. Includes a hold off for a period of time if there is resource contention
 */
void handle_calendar_timer(AppContextRef app_ctx, AppTimerHandle handle, uint32_t cookie) {
	
  // Show the alert and let the world know
  if (cookie >= ALERT_EVENT && cookie <= (ALERT_EVENT + (MAX_EVENTS * 2))) {
	  app_timer_cancel_event(app_ctx, handle);
	  int num = cookie - ALERT_EVENT;
	  if (timer_rec[num].active == false)
		  return; // Already had the data for this event deleted - cannot show it.
	  display_event_text(timer_rec[num].event_desc, timer_rec[num].relative_desc);
	  timer_rec[num].active = false;
	  app_timer_send_event(app_ctx, 30000, RESTORE_DATE);	
	  app_timer_send_event(app_ctx, 15000, SECOND_ALERT);	
	  vibes_double_pulse();
      light_enable_interaction();
	  return;
  }

  // Let us know again
  if (cookie == SECOND_ALERT) {
      app_timer_cancel_event(app_ctx, handle);
      vibes_double_pulse();
      light_enable_interaction();
      return;
   }
      
  // Put the date back into the display area	
  if (cookie == RESTORE_DATE) {
	  app_timer_cancel_event(app_ctx, handle);
	  draw_date();
	  return;
  }

  // Server requests	  
  if (cookie != REQUEST_CALENDAR_KEY)
	  return;

  app_timer_cancel_event(app_ctx, handle);
	  
  // If we're going to make a call to the phone, then a dictionary is a good idea.
  DictionaryIterator *iter;
  app_message_out_get(&iter);

  // We didn't get a dictionary - so go away and wait until resources are available
  if (!iter) {
	// Can't get an dictionary then come back in a second
    app_timer_send_event(app_ctx, 1000, cookie);
    return;
  }

  // Make the appropriate call to the server
  if (cookie == REQUEST_CALENDAR_KEY) {
	calendar_request(iter);
    app_timer_send_event(app_ctx, REQUEST_CALENDAR_INTERVAL_MS, cookie);
  } 
}


