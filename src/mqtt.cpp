#define UMQTT
#ifdef UMQTT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <uMQTTBroker.h>
#include "defs.h"

static const String my_clientID{"fcce"};
static unsigned long fcc_last_seen;

/*
 * Custom broker class with overwritten callback functions
 */
class myMQTTBroker : public uMQTTBroker
{
public:
    virtual bool onConnect(IPAddress addr, uint16_t client_count)
    {
        log_msg(addr.toString() + " connected");
        return true;
    }

    virtual void onDisconnect(IPAddress addr, String client_id)
    {
        log_msg(addr.toString() + " (" + client_id + ") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id)
    {
        return true;
    }

    virtual void onData(String topic, const char *data, uint32_t length)
    {
        char data_str[length + 1];
        os_memcpy(data_str, data, length);
        data_str[length] = '\0';

        log_msg("received topic '" + topic + "' with data '" + (String)data_str + "'");
        //printClients();
        if (topic.startsWith("fcc/cc-alive"))
        {
            log_msg("fcc is alive (" + String((millis() - fcc_last_seen) / 1000) + "s), re-arming watchdog.");
            fcc_last_seen = millis();
            return;
        }
        if (topic.startsWith("fcc/reset-request"))
        {
            log_msg("fcc requested reset... rebooting");
            mqtt_publish("fcce/config", "reboot requested via mqtt...");
            delay(250);
            ESP.restart();
        }
    }

    // Sample for the usage of the client info methods

    virtual void printClients()
    {
        for (int i = 0; i < getClientCount(); i++)
        {
            IPAddress addr;
            String client_id;

            getClientAddr(i, addr);
            getClientId(i, client_id);
            log_msg("Client " + client_id + " on addr: " + addr.toString());
        }
    }
};

myMQTTBroker myBroker;

void setup_mqtt(void)
{
    printf("Starting uMQTT broker\n");
    myBroker.init();
    
    myBroker.subscribe("fcc/#");
    log_msg("mqtt setup done");
    mqtt_publish("/sensor-alive", "Formicula embedded starting...");
    fcc_last_seen = millis(); /* give fcc some tome to connect */
}

void loop_mqtt(void)
{
    if ((millis() - fcc_last_seen) > 270 * 1000)
    {
        const String m{"<ERR>fcc offline, rebooting."};
        mqtt_publish("/config", m);
        log_msg(m);
        delay(250);
        ESP.restart();
    }
}

void mqtt_publish(String topic, String msg)
{
    static int errcount = 0;
    //log_msg("publishing: " + my_clientID + topic + msg);
    if (myBroker.publish(my_clientID + topic, msg, 0, 0))
    {
        errcount = 0;
        return;
    }

    /* zlorfik, mqtt sucks again */
    log_msg("mqtt not connected " + String(errcount) + "- discarding: " + topic + "-" + msg);
    if (++errcount > 10)
    {
        log_msg("too many mqtt errors, rebooting...");
        ESP.restart(); /* lets try a reboot */
    }
}

void mqtt_publish(const char *topic, const char *msg)
{
    mqtt_publish(String(topic), String(msg));
}

#endif
