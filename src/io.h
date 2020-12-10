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

#ifndef __io_h__
#define __io_h__

#include <Arduino.h>
#include <DHTesp.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include <list>
#include <array>
#include <Servo.h>
#include "myTicker.h"
#include "defs.h"

class avgDHT; /* forware declaration */

typedef enum
{
    REAL_SENSOR,
    JUST_SWITCH
} sens_type_t;

class genSensor
{
protected:
    const String name;
    const sens_type_t type;

public:
    genSensor(String n, const sens_type_t t = REAL_SENSOR) : name(n), type(t) {}
    virtual ~genSensor() = default;

    sens_type_t get_type() { return type; }
    virtual String to_string(void) { return name; };
    virtual float get_data(void) = 0;
    virtual void publish_data(void)
    {
        mqtt_publish(to_string(), String(get_data()));
    }
};

class periodicSensor : public genSensor
{
    static std::list<periodicSensor *> periodic_sensors;

protected:
    myTicker::Ticker *ticker;

public:
    periodicSensor(const String n, int period, void *a) : genSensor(n)
    {
        ticker = new myTicker::Ticker(wrapper, a, period, 0, myTicker::MILLIS);
        periodic_sensors.push_back(this);
        ticker->start();
    }
    virtual ~periodicSensor() = default;
    static void wrapper(void *a);
    static void tick_sensors(void);
    virtual float get_data(void) override = 0;
    virtual void cb(void) = 0;
};

class myDS18B20 : public periodicSensor
{
    float temp;
    OneWire *wire;
    DallasTemperature *temps;

public:
    myDS18B20(const String n, int p);
    virtual ~myDS18B20() = default;

    virtual void update_data();
    virtual void cb(void) override
    {
        update_data();
        publish_data();
    }
    virtual float get_data(void) override { return temp; }
};

class myCapMoisture : public periodicSensor
{
    float val;
    int pin;

public:
    myCapMoisture(const String n, int p) : periodicSensor(n, 2500, this), pin(p) {}
    virtual ~myCapMoisture() = default;

    virtual void update_data()
    {
        val = analogRead(pin);
    }
    virtual void cb(void) override
    {
        update_data();
        publish_data();
    }
    virtual float get_data(void) override { return val; }
};

class multiPropertySensor : public periodicSensor
{

public:
    multiPropertySensor(String n, int period, void *a) : periodicSensor(n, period, a) {}
    virtual ~multiPropertySensor() = default;
    virtual float get_data(void) override = 0;
    virtual void cb(void) = 0;
    virtual float get_temp(void) = 0;
    virtual float get_hum(void) = 0;
    virtual void add_parent(avgDHT *p) = 0;
};

class myDHT : public multiPropertySensor
{
    DHTesp dht_obj;
    int pin;
    DHTesp::DHT_MODEL_t model;
    float temp, hum;
    std::list<avgDHT *> parents;

public:
    myDHT(String n, int p, DHTesp::DHT_MODEL_t m = DHTesp::DHT22);
    virtual ~myDHT() = default;

    virtual void cb(void) override { update_data(); }
    inline int get_pin(void) { return pin; }
    virtual float get_data(void) { return temp * hum; } /* dummy, never used directly here */
    inline float get_temp(void) { return temp; }
    inline float get_hum(void) { return hum; }

    void update_data();
    virtual void add_parent(avgDHT *p) { parents.push_back(p); }
};

class myBM280 : public multiPropertySensor
{
    int address;
    float temp, hum;
    Adafruit_BME280 *bme; // use I2C interface
    Adafruit_Sensor *bme_temp;
    Adafruit_Sensor *bme_humidity;
    //Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
    std::list<avgDHT *> parents;

public:
    myBM280(String n, int address);
    virtual ~myBM280() = default;

    virtual void cb(void) override { update_data(); }
    virtual float get_data(void) { return temp * hum; } /* dummy, never used directly here */
    inline float get_temp() { return temp; }
    inline float get_hum() { return hum; }

    void update_data();
    virtual void add_parent(avgDHT *p) { parents.push_back(p); }
};

class ioSwitch
{
    uint8_t pin;
    bool invers;

public:
    ioSwitch(uint8_t gpio, bool i = false) : pin(gpio), invers(i) {}
    ~ioSwitch() = default;

    virtual void _set(uint8_t pin, int val) = 0;
    virtual int _state(uint8_t pin) = 0;
    virtual const String to_string(void) = 0;
    inline int state() { return _state(pin); }
    inline int set(uint8_t n, bool ign_invers = false)
    {
        int res = state();
        if (invers && !ign_invers)
        {
            n = (n == HIGH) ? LOW : HIGH;
        }
        _set(pin, n);
        return res;
    }
    inline int toggle()
    {
        int res = state();
        set((res == LOW) ? HIGH : LOW);
        return res;
    }

    inline bool is_invers(void) { return invers; }
};

class ioDigitalIO : public ioSwitch
{
    const String on{"on"};
    const String off{"off"};

public:
    ioDigitalIO(uint8_t pin, bool i = false, uint8_t m = OUTPUT) : ioSwitch(pin, i) { pinMode(pin, m); }
    ~ioDigitalIO() = default;

    void _set(uint8_t pin, int v) override
    {
        digitalWrite(pin, v);
        //printf("digital write IO%d = %d\n", pin, v);
    }
    int _state(uint8_t pin) override
    {
        return digitalRead(pin);
    }

    const String to_string(void) override
    {
        int s = state();
        //        if (invers)
        //            return ((s == HIGH) ? off : on);
        return ((s == HIGH) ? on : off);
    }
};

class ioServo : public ioSwitch
{
    Servo *servo;
    int low, high;

public:
    ioServo(uint8_t pin, bool i = false, int l = 0, int h = 180) : ioSwitch(pin, i), low(l), high(h)
    {
        servo = new Servo;
#if 0
        ESP32PWM::allocateTimer(0);
        ESP32PWM::allocateTimer(1);
        ESP32PWM::allocateTimer(2);
        ESP32PWM::allocateTimer(3);
#endif
        //servo->setPeriodHertz(50);
        servo->attach(pin, 50, 2400);
    }

    void _set(uint8_t pin, int v) override
    {
        int val = map(v, LOW, HIGH, low, high);
        //printf("servo write IO%d = %d\n", pin, val);
        servo->write(val);
    }

    int _state(uint8_t pin) override
    {
        return servo->read();
    }

    const String to_string(void) override
    {
        return String(state());
    }
};

#if 0
class timeSwitch : public genSensor
{
public:
    timeSwitch() : genSensor(JUST_SWITCH) {}
    ~timeSwitch() = default;

    float get_data(void) override { return -1.0; }
    virtual String to_string(void) override { return String("timeSwitch"); }
};
#endif

class avgDHT : public genSensor
{
protected:
    static const int sample_no = 10;
    std::array<float, sample_no> data;
    int act_ind;
    std::list<multiPropertySensor *> sensors;

public:
    avgDHT(const sens_type_t t, std::list<multiPropertySensor *> sens, const String n = "avgDHT", float def_val = 0.0)
        : genSensor(n, t), act_ind(0), sensors(sens)
    {
        //        mutex = xSemaphoreCreateMutex();
        for_each(sensors.begin(), sensors.end(),
                 [&](multiPropertySensor *s) { s->add_parent(this); });
        for (int i = 0; i < sample_no; i++)
            data[i] = def_val;
    }
    ~avgDHT() = default;

    virtual void update_data(multiPropertySensor *) = 0;
    virtual void update_display(float) = 0;
    virtual String to_string(void) override = 0;
    void add_data(float d)
    {
        P(mutex);
        data[act_ind] = d;
        act_ind = ((act_ind + 1) % sample_no);
        V(mutex);
    }
    float get_data() override
    {
        float res = 0.0;
        P(mutex);
        std::array<float, sample_no> s = data;
        V(mutex);
        std::sort(s.begin(), s.end());
#if 0
        printf("data:           [");
        for (int i = 0; i < sample_no; i++) {
            printf("%.2f,", data[i]);
        }
        printf("]\n");
        printf("sorted data:    [");
        for (int i = 0; i < sample_no; i++) {
            printf("%.2f,", s[i]);
        }
        printf("]\n");
#endif
        for (int i = 2; i < sample_no - 2; i++)
            res = res + s[i];
        res = res / (sample_no - 4);
        update_display(res);
        return res;
    }
};

class tempSensor : public avgDHT
{
public:
    tempSensor(std::list<multiPropertySensor *> sens, const String n = "TempSensor") : avgDHT(REAL_SENSOR, sens, n, 5.0) {}
    virtual ~tempSensor() = default;
    virtual String to_string(void) override { return String(name); }

    void update_data(multiPropertySensor *s) override
    {
        add_data(s->get_temp());
        publish_data();
    }
    void update_display(float v)
    {
        //log_msg(this->to_string() + String(v));
    }
};
class humSensor : public avgDHT
{
public:
    humSensor(std::list<multiPropertySensor *> sens, const String n = "HumSensor") : avgDHT(REAL_SENSOR, sens, n, 5.5) {}
    virtual ~humSensor() = default;
    virtual String to_string(void) override { return String(name); }

    void update_data(multiPropertySensor *s) override
    {
        add_data(s->get_hum());
        publish_data();
    }

    void update_display(float v)
    {
        //log_msg(this->to_string() + String(v));
    }
};

#endif