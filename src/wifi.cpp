/* -*-c++-*-
 * This file is part of formicula2.
 * 
 * vice-mapper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * vice-mapper is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with vice-mapper.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <time.h>
#include <ESP8266WebServer.h>
#include <AutoConnectOTA.h>
#include <AutoConnect.h>
#include <uMQTTBroker.h>
#include <ESP8266mDNS.h>
#include "defs.h"

static ESP8266WebServer ip_server;
static AutoConnect portal(ip_server);
static AutoConnectConfig config;
static const String hostname = "fcce";
static uMQTTBroker mqtt_broker;

void printLocalTime()
{
    time_t t = time(NULL);
    printf("%s", ctime(&t));
}

static void rootPage(void)
{
    char content[] = "ESP12x WebPage!";
    ip_server.send(200, "text/plain", content);
}

void setup_wifi(void)
{
    printf("Setting up Wifi...");
    ip_server.on("/", rootPage);
    config.ota = AC_OTA_BUILTIN;
    portal.config(config);
    if (portal.begin())
    {
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    }
    printf("done.\nSetting up local time...");
    printLocalTime();
    log_msg("Setting hostname " + hostname);
    while (!MDNS.begin(hostname.c_str()))
    {
        log_msg("DNS setup failed.");
    }
    printf("MDNS add Service: %d\n", MDNS.addService("mqtt", "tcp", 1883));
    delay(25);
    printf("Starting MQTT broker\n");
    mqtt_broker.init();
    delay(25);
}

void loop_wifi(void)
{
    portal.handleClient();
    MDNS.update();
}
