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
typedef int SemaphoreHandle_t;
#define xSemaphoreCreateMutex() 42
class genSensor;
class uiElements {
    public:
    uiElements() = default;
    ~uiElements() = default;

    void register_sensor(genSensor *) {}
    void update_sensor(genSensor *) {}
};

#define mqtt_register_sensor(x)

#endif