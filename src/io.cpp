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
#include <Arduino.h>
#include <Ticker.h>
#include <list>
#include "io.h"

void periodicSensor::wrapper(void *s)
{
    //printf("%s: %p\n", __FUNCTION__, s);
    static_cast<periodicSensor *>(s)->cb();
}

std::list<periodicSensor *> periodicSensor::periodic_sensors;
void periodicSensor::tick_sensors(void)
{
    std::for_each(periodic_sensors.begin(), periodic_sensors.end(),
                  [](periodicSensor *s) { s->ticker->update(); });
}

/* Temperature DS18B20 */

myDS18B20::myDS18B20(const String n, int pin) : periodicSensor(n, 2000, this)
{
    log_msg("constr." + n);
    wire = new OneWire(pin);
    temps = new DallasTemperature(wire);
    temps->begin();
    log_msg("done.");
}

void myDS18B20::update_data(void)
{
    printf("%s update... \n", name.c_str());
    temps->requestTemperatures();
    temp = temps->getTempCByIndex(0);
    if (temp != DEVICE_DISCONNECTED_C)
        log_msg(name + ":" + String(temp));
    else
        log_msg("Getting data from " + name + " failed.");
}

/* Temperature & Humidity DHT11/22*/
myDHT::myDHT(String n, int p, DHTesp::DHT_MODEL_t m)
    : periodicSensor(n, 2000, this), pin(p), model(m), temp(-500), hum(-99)
{
    dht_obj.setup(pin, m);
    //    mutex = xSemaphoreCreateMutex();
}

void myDHT::update_data(void)
{
    String s = name;
    TempAndHumidity newValues = dht_obj.getTempAndHumidity();
    if (dht_obj.getStatus() != 0)
    {
        log_msg(s + "(" + get_pin() + ") - error status: " + String(dht_obj.getStatusString()));
        return;
    }
    P(mutex);
    temp = newValues.temperature;
    hum = newValues.humidity; // ((rand() % 100) - 50.0) / 20.0;
    V(mutex);
    //log_msg(String("DHT(") + get_pin() + "): Temp = " + String(temp) + ", Hum = " + String(hum));
    std::for_each(parents.begin(), parents.end(),
                  [&](avgDHT *p) { p->update_data(this); });
}
