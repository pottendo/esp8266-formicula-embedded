#ifndef __defs_h__
#define __defs_h__

extern void setup_wifi(void);
extern void loop_wifi(void);
extern void setup_io(void);
extern void log_msg(const char *m);
extern void log_msg(const String &s);

extern void setup_mqtt(void);
extern void loop_mqtt(void);
extern void mqtt_publish(String topic, String msg);

/* mutual exclusion, not yet used on ESP8266 */
#define P(s) 
#define V(s)

#endif