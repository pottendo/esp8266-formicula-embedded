#include <Arduino.h>
#include <ESP8266WiFi.h>
static unsigned long fcc_last_seen;

//#define UMQTT
#ifdef UMQTT
#include <uMQTTBroker.h>
#include "defs.h"

static const String my_clientID{"fcce"};

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
        // printClients();
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
    // log_msg("publishing: " + my_clientID + topic + msg);
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

#else // std mqtt broker
#include <ESP8266mDNS.h>
#include "mqtt-cred.h"
#include "mqtt.h"
const char *client_id = "fcce";

static myMqtt *fcce_connection;
static const char *mqtt_broker = "fcc-rpi";

static void fcce_upstream(String &t, String &payload);

void setup_mqtt(void)
{
    while (true)
    {
        try
        {
            fcce_connection = new myMqttLocal(client_id, mqtt_broker, fcce_upstream, "fcce broker");
            break;
        }
        catch (String msg)
        {
            log_msg(msg + "... retrying");
            delay(500);
        }
    }
    log_msg("mqtt setup done");
}

void loop_mqtt()
{
    if (fcce_connection->connected())
        fcce_connection->loop();
    else
        fcce_connection->reconnect();
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
    //log_msg(String{"publishing: "} + client_id + topic + " '" + msg + "'");
    if (fcce_connection->publish(topic, msg))
    {
        errcount = 0;
        return;
    }

    /* zlorfik, mqtt sucks again */
    log_msg("mqtt not connected " + String(errcount) + "- discarding: " + topic + "-" + msg);
    if (++errcount > 100)
    {
        log_msg("too many mqtt errors, rebooting...");
        ESP.restart(); /* lets try a reboot */
    }
}

void mqtt_publish(const char *topic, const char *msg)
{
    mqtt_publish(String(topic), String(msg));
}

myMqtt *mqtt_register_logger(void)
{
    while (true)
    {
        try
        {
#ifdef MQTT_LOG_LOCAL
            return new myMqttLocal(client_id, MQTT_LOG, nullptr, "log broker", 1883, MQTT_LOG_USER, MQTT_LOG_PW);
#else
            return new myMqttSec(client_id, MQTT_LOG, nullptr, "log broker", 8883, MQTT_LOG_USER, MQTT_LOG_PW);
#endif
        }
        catch (String msg)
        {
            log_msg(msg + "... retrying");
            delay(500);
        }
    }
}

/* class myMqtt broker */
myMqtt::myMqtt(const char *id, upstream_fn f, const char *n, const char *user, const char *pw)
    : id(id), user(user), pw(pw)
{
    name = (n ? n : id);
    if (!f)
        up_fn = [](String &t, String &p)
        { log_msg(String(client_id) + " received: " + t + ":" + p); };
    else
        up_fn = f;
}

void myMqtt::loop(void)
{
    client->loop();
}

bool myMqtt::connected(void)
{
    bool r;
    r = client->connected();
    conn_stat = (r ? CONN : NO_CONN);
    return r;
}

void myMqtt::reconnect(void)
{
    reconnect_body();
}

void myMqtt::cleanup(void)
{
}

void myMqtt::reconnect_body(void)
{
    // Loop until we're reconnected
    if (client->connected())
    {
        set_conn_stat(CONN);
        log_msg("reconnect_body, client is connected - shouldn't happen here");
    }
    if ((millis() - last) < 2500)
    {
        delay(500);
        return;
    }
    if (connection_wd == 0)
        connection_wd = millis();
    last = millis();
    reconnects++;
    // log_msg("fcc not connected, attempting MQTT connection..." + String(reconnects));

    // Attempt to connect
    if (connect())
    {
        reconnects = 0;
        connection_wd = 0;
        log_msg("fcce connected.");
        client->subscribe("fcc/#", 0);
        mqtt_publish("/sensor-alive", "Formicula embedded starting...");
        fcc_last_seen = millis(); /* give fcc some tome to connect */

        set_conn_stat(CONN);
    }
    else
    {
        unsigned long t1 = (millis() - connection_wd) / 1000;
        log_msg(String("Connection lost for: ") + String(t1) + "s...");
        if (t1 > 300)
        {
            log_msg("mqtt reconnections failed for 5min... rebooting");
            delay(250);
            ESP.restart();
        }
    }
}

bool myMqtt::publish(String &t, String &p, int qos)
{
    if (!client->connected() ||
        !client->publish(client_id + t, p, false, qos))
    {
        log_msg(String(name) + " mqtt failed, rc=" + String(client->lastError()) + ", discarding: " + t + ":" + p);
        return false;
    }
    return true;
}

void myMqtt::register_callback(upstream_fn fn)
{
    client->onMessage(fn);
}

myMqttSec::myMqttSec(const char *id, const char *server, upstream_fn fn, const char *n, int port, const char *user, const char *pw)
    : myMqtt(id, fn, n, user, pw)
{
    client = new MQTTClient{256};
    client->begin(server, port, net);
    client->onMessage(up_fn);
    // mqtt_connections.push_back(this);
    log_msg(String(name) + " mqtt client created.");

    delay(50);
}

bool myMqttSec::connect(void)
{
    P(mutex);
    if (!client->connected() &&
        !client->connect(id, user, pw))
    {
        log_msg(String(name) + " mqtt connect failed, rc=" + String(client->lastError()));
        V(mutex);
        return false;
    }
    V(mutex);
    return true;
}

myMqttLocal::myMqttLocal(const char *id, const char *server, upstream_fn fn, const char *n, int port, const char *user, const char *pw)
    : myMqtt(id, fn, n, user, pw)
{
    client = new MQTTClient{256}; /* default msg size, 128 */
    /*
    IPAddress sv = MDNS.queryHost(server);
    if (uint32_t(sv) == 0)
    {
        throw(String(name) + " mqtt broker '" + server + "' not found ");
    }
    */

    client->begin(server, port, net);
    client->onMessage(up_fn);

    // mqtt_connections.push_back(this);
    log_msg(String(name) + " mqtt client created.");

    delay(50);
}

char *myMqttLocal::get_id(void)
{
    snprintf(id_buf, 16, "%s-%04d", id, nr++);
    return id_buf;
}

bool myMqttLocal::connect(void)
{
    if (!client->connected() &&
        !client->connect(id, "hugo", "schrammel"))
    {
        log_msg(String(name) + " mqtt connect failed, rc=" + String(client->lastError()));
        return false;
    }
    return true;
}

static void fcce_upstream(String &topic, String &payload)
{
    // log_msg("fcce mqtt cb: " + t + ":" + payload);

    log_msg("received topic '" + topic + "' with data '" + payload + "'");
    // printClients();
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

#endif
