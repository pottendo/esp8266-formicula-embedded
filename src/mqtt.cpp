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
        Serial.println(addr.toString() + " connected");
        return true;
    }

    virtual void onDisconnect(IPAddress addr, String client_id)
    {
        Serial.println(addr.toString() + " (" + client_id + ") disconnected");
    }

    virtual bool onAuth(String username, String password, String client_id)
    {
        Serial.println("Username/Password/ClientId: " + username + "/" + password + "/" + client_id);
        return true;
    }

    virtual void onData(String topic, const char *data, uint32_t length)
    {
        char data_str[length + 1];
        os_memcpy(data_str, data, length);
        data_str[length] = '\0';

        Serial.println("received topic '" + topic + "' with data '" + (String)data_str + "'");
        //printClients();
        //log_msg("fcce - " + topic + ": " + payload);
        if (topic.startsWith("fcc/cc-alive"))
        {
            log_msg("fcc is alive (" + String((millis() - fcc_last_seen) / 1000) + "s), re-arming watchdog.");
            fcc_last_seen = millis();
            return;
        }
        if (topic.startsWith("fcc/reset-request"))
        {
            log_msg("fcc requested reset... rebooting");
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
            Serial.println("Client " + client_id + " on addr: " + addr.toString());
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

#ifdef MQTT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "defs.h"
// myqtthub credentials
#define EXTMQTT
#ifdef EXTMQTT
static const char *mqtt_server = "node02.myqtthub.com";
static int mqtt_port = 1883;
static const char *clientID = "fcce";
static const char *user = "fcce-user";
static const char *pw = "fOrmicula999";
WiFiClient net;
#else
static const char *mqtt_server = "pottendo-pi30-phono";
static int mqtt_port = 1883;
static const char *clientID = "fcce";
//static const char *user = "fcc-users";
//static const char *pw = "foRmicula666";
WiFiClient net;
#endif

static MQTTClient client;
static unsigned long fcc_last_seen;

void callback(String &topic, String &payload)
{
    //log_msg("fcce - " + topic + ": " + payload);
    if (topic.startsWith("fcc/cc-alive"))
    {
        log_msg("fcc is alive (" + String((millis() - fcc_last_seen) / 1000) + "s), re-arming watchdog.");
        fcc_last_seen = millis();
        return;
    }
    if (topic.startsWith("fcc/reset-request"))
    {
        log_msg("fcc requested reset... rebooting");
        ESP.restart();
    }
}

void reconnect()
{
    static int reconnects = 0;
    static unsigned long last = 0;
    static unsigned long connection_wd = 0;

    // Loop until we're reconnected
    if ((!client.connected()) &&
        ((millis() - last) > 2500))
    {
        if (connection_wd == 0)
            connection_wd = millis();
        last = millis();
        reconnects++;
        log_msg("fcce not connected, attempting MQTT connection..." + String(reconnects));

        // Attempt to connect
#ifdef EXTMQTT
        //net.setInsecure();
        if (client.connect(clientID, user, pw))
#else
        if (client.connect(clientID))
#endif
        {
            client.subscribe("fcc/#", 0);
            mqtt_publish("/config", "Formicula Control Center Embedded - aloha...");
            reconnects = 0;
            connection_wd = 0;
            log_msg("fcce connected.");
        }
        else
        {
            unsigned long t1 = (millis() - connection_wd) / 1000;
            log_msg(String("Connection lost for: ") + String(t1) + "s...");
            if (t1 > 300)
            {
                log_msg("mqtt reconnections failed for 5min... rebooting");
                ESP.restart();
            }
            log_msg("mqtt connect failed, rc=" + String(client.lastError()) + "... retrying.");
        }
    }
}

bool mqtt_reset(void)
{
    log_msg("mqtt reset requested: " + String(client.lastError()));
    return true;
}

void mqtt_publish(String topic, String msg)
{
    if (!client.connected() &&
        (mqtt_reset() == false))
    {
        log_msg("mqtt client not connected...");
        return;
    }
    //log_msg("fcce publish: " + topic + " - " + msg);
    client.publish((clientID + topic).c_str(), msg.c_str());
}

void setup_mqtt(void)
{
    client.begin(mqtt_server, mqtt_port, net);
    client.onMessage(callback);
    delay(50);
    reconnect();
}

void loop_mqtt_dummy()
{
    static long value = 0;
    static char msg[50];
    static unsigned long lastMsg = millis();

    unsigned long now = millis();
    if (now - lastMsg > 2000)
    {
        if (!client.connected())
        {
            mqtt_reset();
        }
        else
        {
            ++value;
            snprintf(msg, 50, "Hello world from fcce #%ld", value);
            mqtt_publish("/config", msg);
            const char *c;
            if (value % 2)
                c = "1";
            else
                c = "0";

            mqtt_publish("/Infrarot", c);
        }
        lastMsg = now;
    }
}

void loop_mqtt()
{
    client.loop();
    if (!client.connected())
    {
        reconnect();
    }
    //loop_mqtt_dummy();

    if ((millis() - fcc_last_seen) > 270 * 1000)
    {
        const String m{"<ERR>fcc offline, rebooting."};
        mqtt_publish("/config", m);
        log_msg(m);
        ESP.restart();
    }
}
#endif