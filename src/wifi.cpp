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
//#include <uMQTTBroker.h>
#include <ESP8266mDNS.h>
static const String hostname{"fcce"};
//static uMQTTBroker mqtt_broker;
#define USE_AC

#ifdef USE_AC

#include <time.h>
#include <ESP8266WebServer.h>
#include <AutoConnectOTA.h>
#include <AutoConnect.h>
#include "defs.h"

static ESP8266WebServer ip_server;
static AutoConnect portal(ip_server);
static AutoConnectConfig config;

void printLocalTime()
{
    time_t t = time(NULL);
    printf("%s", ctime(&t));
}

static void rootPage(void)
{
    char content[] = "Formicula Control Center Embedded - use http://fcce.local/_ac";
    ip_server.send(200, "text/plain", content);
}

void setup_wifi(void)
{
    printf("Setting up Wifi...");
    ip_server.on("/", rootPage);
    config.ota = AC_OTA_BUILTIN;
    config.hostName = hostname;
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
    //mqtt_broker.init();
    delay(25);
}

void loop_wifi(void)
{
    portal.handleClient();
    MDNS.update();
}

#else /* USE_AC */

#include <ESP8266WiFi.h>
#include "defs.h"
//#define AP
//#define MOBILE
//#define TEST
#define EXT
//#define USE_MDNS

void connect_wifi(void)
{
    //WiFi.setPhyMode(WIFI_PHY_MODE_11N);
#ifdef AP
    IPAddress ip(192, 168, 4, 1);
    IPAddress gw(192, 168, 4, 1);
    IPAddress dns(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(ip, gw, subnet);
    WiFi.softAP("FCCENET");

#endif
#ifdef MOBILE
    WiFi.mode(WIFI_STA);
    IPAddress ip(192, 168, 43, 67);
    IPAddress gw(192, 168, 43, 1);
    IPAddress dns(192, 168, 43, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gw, subnet, dns);
    WiFi.begin("pottendosM2", "poTtendosMOBILE");
#endif
#ifdef TEST
    IPAddress ip(10, 0, 0, 2);
    IPAddress gw(10, 0, 0, 138);
    IPAddress dns(10, 0, 0, 138);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gw, subnet, dns);
    WiFi.begin("pottendoT", "poTtendosWLAN");
#endif
#ifdef EXT
#if 0
    IPAddress ip(192, 168, 188, 34);
    IPAddress gw(192, 168, 188, 1);
    IPAddress dns(192, 168, 188, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(ip, gw, subnet, dns);
#endif
    WiFi.begin("pottendo_EXT", "poTtendosWLAN");
#endif
    while (!WiFi.isConnected())
    {
        log_msg("Wifi connecting...");
        delay(500);
    }
#ifdef USE_MDNS
    log_msg("Setting hostname " + hostname);
    while (!MDNS.begin(hostname.c_str()))
    {
        log_msg("DNS setup failed.");
    }
    printf("MDNS add Service: %d\n", MDNS.addService("mqtt", "tcp", 1883));
#endif

    printf("fcce IP = %s, GW = %s, DNS = %s\n",
           WiFi.localIP().toString().c_str(),
           WiFi.gatewayIP().toString().c_str(),
           WiFi.dnsIP().toString().c_str());
    WiFi.printDiag(Serial);
}
void setup_wifi(void)
{
    connect_wifi();
    //delay(2000);
    //mqtt_broker.init();
    delay(500);
}

void loop_wifi(void)
{
    //    portal.handleClient();
    if (!WiFi.isConnected())
    {
        printf("Wifi not connected - reconnecting...\n");
        connect_wifi();
        delay(500);
    }
#ifdef USE_MDNS
    MDNS.update();
#endif
}

#endif
