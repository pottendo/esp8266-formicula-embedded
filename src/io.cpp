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
#include <list>
//#include "ui.h"
#include "defs.h"
#include "io.h"

#ifdef ESP32
void periodicSensor::periodic_wrapper(lv_task_t *t)
{
    periodicSensor *obj = static_cast<periodicSensor *>(t->user_data);
    obj->cb();
}

periodicSensor::periodicSensor(uiElements *ui, const String n, int period)
    : genSensor(ui, n)
{
    ticker_task = lv_task_create(periodicSensor::periodic_wrapper, period, LV_TASK_PRIO_LOW, this);
    if (!ticker_task)
        log_msg("Failed to create periodic task for sensor " + name);
}
#endif

#ifdef ESP8266
void periodicSensor::periodic_wrapper(void *s)
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
#endif

/* Temperature DS18B20 */

myDS18B20::myDS18B20(uiElements *ui, const String n, int pin, int period)
    : periodicSensor(ui, n, period)
{
    uint8_t address[8];
    wire = new OneWire(pin);
    temps = new DallasTemperature(wire);
    temps->begin();
    temps->requestTemperatures();
    /* temps->getDS18Count() doesn't work */
    while (wire->search(address))
    {
        no_DS18B20++;
    }
    log_msg(name + " found " + String(no_DS18B20) + " sensors.");
    if (no_DS18B20 == 0)
    {
        error = true;
        log_msg(name + ": no sensors found.");
        mqtt_publish(name, "<ERR>no sensors found");
    }
}

void myDS18B20::update_data(void)
{
    temps->begin();
    temps->requestTemperatures();

    error = false;
    if (no_DS18B20 == 0)
    {
        log_msg(name + ": no sensors found.");
        mqtt_publish(name, "<ERR>no sensors found");
        error = true;
    }
    P(mutex);
    for (int i = 0; i < no_DS18B20; i++)
    {
        all_temps[i] = temps->getTempCByIndex(i);
        if (all_temps[i] != DEVICE_DISCONNECTED_C)
        {
            //log_msg(name + ":" + String(all_temps[i]) + " Sensor " + String(i + 1) + "/" + String(no_DS18B20));
            //mqtt_publish(name + "-" + String(i), String(all_temps[i]));
            //temp_meter->set_val(all_temps[i]);
        }
        else
        {
            log_msg("Getting data from " + name + " failed.");
            error = true;
            all_temps[i] = NAN;
            mqtt_publish(name + "-" + String(i), "<ERR>disconnected.");
        }
    }
    V(mutex);
    std::for_each(parents.begin(), parents.end(),
                  [&](avgSensor *p) {
                      for (int i = 0; i < no_DS18B20; i++)
                          p->add_data(all_temps[i]);
                      p->update_data();
                  });
}

myCapMoisture::myCapMoisture(uiElements *ui, const String n, int p1, int p2, int period)
    : periodicSensor(ui, n, period), pin_sel0(p1), pin_sel1(p2), no_sensors(0)
{
    if (max_sensors > 1)
    {
        pinMode(pin_sel0, OUTPUT);
        pinMode(pin_sel1, OUTPUT);
    }
    pinMode(A0, INPUT);
    no_sensors = poll_analog_inputs(max_sensors);
    log_msg(n + ": found " + String(no_sensors) + " sensors.");
}

float myCapMoisture::sens2hum(int t)
{
    float val;
    static const float cal = 49.9F / (825.0 - 400.0); /* Sensor delivers: max 825 (dry air) down to ~412 (in water) */
    //log_msg("hum = " + String(t) + "cal = " + String(cal));
    val = 99.9F - static_cast<float>(t - 400) * cal; /* align with BME280 sensor: ~50% in dry air */
    if ((val >= 100) || (val <= 10))
        return -1;
    return val;
}

int myCapMoisture::poll_analog_inputs(int max_sensors)
{
    int t, valid = 0;
    for (int i = 0; i < max_sensors; i++)
    {
        all_hums[i] = NAN;
        if (max_sensors > 1)
        {
            digitalWrite(pin_sel0, i & 1);
            digitalWrite(pin_sel1, i & 2);
        }
        t = analogRead(PIN_A0);
        if ((t >= 50) && (t <= 1000))
        {
            all_hums[valid] = sens2hum(t);
            if (all_hums[valid] < 0)
            {
                all_hums[valid] = NAN;
                log_msg(name + "(" + String(valid) + "): invalid sensor value - " + String(t) + " on analog input " + String(i));
                mqtt_publish(name, "<ERR>invalid value: " + String(t) + " on analog input " + String(i));
            }
            valid++;
            continue;
        }
    }
    return valid;
}

void myCapMoisture::update_data()
{
    P(mutex);
    int valid = poll_analog_inputs(max_sensors);
    V(mutex);
    std::for_each(parents.begin(), parents.end(),
                  [&](avgSensor *p) {
                      for (int i = 0; i < valid; i++)
                          p->add_data(all_hums[i]);
                      p->update_data();
                  });
}

/* Temperature & Humidity */
myDHT::myDHT(uiElements *ui, String n, int p, DHTesp::DHT_MODEL_t m, int period)
    : multiPropertySensor(ui, n, period), pin(p), model(m), temp(NAN), hum(NAN)
{
    dht_obj.setup(pin, m);
}

void myDHT::update_data(void)
{
    error = false;
    TempAndHumidity newValues = dht_obj.getTempAndHumidity();
    if (dht_obj.getStatus() != 0)
    {
        log_msg(name + "(" + get_pin() + ") - error status: " + String(dht_obj.getStatusString()));
        error = true;
        mqtt_publish(name + "(" + get_pin() + ")", "<ERR>" + String(dht_obj.getStatusString()));
        newValues.temperature = NAN;
        newValues.humidity = NAN;
    }
    P(mutex);
    temp = newValues.temperature;
    hum = newValues.humidity;
    V(mutex);
    //publish_data();
    std::for_each(parents.begin(), parents.end(),
                  [&](genSensor *p) { p->update_data(this); });
}

/* BME280 Sensor */
myBM280::myBM280(uiElements *ui, String n, int sda, int scl, uint8_t a, int period)
    : multiPropertySensor(ui, n, period), sda(sda), scl(scl), address(a), temp(NAN), hum(NAN)
{
    i2c = new TwoWire;
    bme = new Adafruit_BME280;
    bme_temp = nullptr;
    bme_humidity = nullptr;
    i2c->begin(sda, scl, address);
    if (!bme->begin(address, i2c))
    {
        log_msg("failed to initialize BME280 sensor " + name);
        error = true;
    }
    else
    {
        char buf[8];
        snprintf(buf, 8, "0x%0x", address);
        log_msg(name + " found on address " + String(buf));
        P(mutex);
        bme_temp = bme->getTemperatureSensor();
        bme_humidity = bme->getHumiditySensor();
        V(mutex);
    }
}

void myBM280::update_data(void)
{
    if (!bme_temp || !bme_humidity)
    {
        if (!bme->begin(address, i2c))
        {
            log_msg("failed to initialize BME280 sensor " + name);
            temp = hum = NAN;
            error = true;
            goto out;
        }
        P(mutex);
        bme_temp = bme->getTemperatureSensor();
        bme_humidity = bme->getHumiditySensor();
        V(mutex);
    }
    sensors_event_t temp_event, humidity_event;
    P(mutex);
    error = false;
    bme_temp->getEvent(&temp_event);
    bme_humidity->getEvent(&humidity_event);
    temp = temp_event.temperature;
    hum = humidity_event.relative_humidity;
    if (isnan(temp) || isnan(hum) ||
        (temp < -40) || (hum >= 100))
    {
        temp = hum = NAN;
        error = true;
        if (!bme->begin(address, i2c))
            log_msg("failed to initialize BME280 sensor " + name);
    }
    V(mutex);

out:
    //publish_data();
    std::for_each(parents.begin(), parents.end(),
                  [&](genSensor *p) {
                      //printf("updating data for %s at parent %s\n", this->get_name().c_str(), p->get_name().c_str());
                      p->update_data(this);
                  });
    if (error)
        mqtt_publish(name + "(" + String(sda) + "," + String(scl) + ")", "<ERR>returned " + String(temp) + "C," + String(hum) + "%");
}
