#pragma once

#include "LocationProvider.h"
#include <MicroNMEA.h>
#include <RTClib.h>
#include <helpers/RefCountedDigitalPin.h>

#ifndef GPS_EN
    #ifdef PIN_GPS_EN
        #define GPS_EN PIN_GPS_EN
    #else
        #define GPS_EN (-1)
    #endif
#endif

#ifndef GPS_EN_ACTIVE
    #ifdef PIN_GPS_EN_ACTIVE
        #define GPS_EN_ACTIVE PIN_GPS_EN_ACTIVE
    #else
        #define GPS_EN_ACTIVE HIGH
    #endif
#endif

#ifndef GPS_RESET
    #ifdef PIN_GPS_RESET
        #define GPS_RESET PIN_GPS_RESET
    #else
        #define GPS_RESET (-1)
    #endif
#endif

#ifndef GPS_RESET_ACTIVE
    #ifdef PIN_GPS_RESET_ACTIVE
        #define GPS_RESET_ACTIVE PIN_GPS_RESET_ACTIVE
    #else
        #define GPS_RESET_ACTIVE LOW
    #endif
#endif

class MicroNMEALocationProvider : public LocationProvider {
    char _nmeaBuffer[100];
    MicroNMEA nmea;
    mesh::RTCClock* _clock;
    Stream* _gps_serial;
    RefCountedDigitalPin* _peripher_power;
    int8_t _claims = 0;
    int _pin_reset;
    int _pin_en;
    uint32_t _last_time_sync = 0, _fix_received = 0, _altitude_received = 0;
    uint32_t _timestamp = 0;
    long _latitude = 0, _longitude = 0, _altitude = 0, _satellites = 0;
    bool _have_fix = false, _have_altitude = false;
    unsigned _valid_times = 0;
    static const uint32_t FIX_MAX_AGE = 30000;

    void invalidate() {
        nmea.clear(); nmea.setBuffer(_nmeaBuffer, sizeof(_nmeaBuffer));
        while (_gps_serial->available()) _gps_serial->read();
        _have_fix = _have_altitude = false;
        _timestamp = 0; _valid_times = 0; _satellites = 0;
    }
    const char* field(unsigned index) const {
        const char* p = _nmeaBuffer;
        while (index--) { p = strchr(p, ','); if (!p) return nullptr; ++p; }
        return p;
    }
    bool hasNumber(unsigned index) const {
        const char* p = field(index);
        if (!p) return false;
        if (*p == '-' || *p == '+') ++p;
        return *p >= '0' && *p <= '9';
    }
    bool validDateTime() const {
        const char* date = field(9);
        if (!date) return false;
        for (unsigned i = 0; i < 6; ++i) if (date[i] < '0' || date[i] > '9') return false;
        if (date[6] != ',') return false;
        unsigned year = nmea.getYear(), month = nmea.getMonth(), day = nmea.getDay();
        if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1) return false;
        static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        unsigned limit = days[month-1] + (month == 2 && year % 4 == 0 ? 1 : 0);
        return day <= limit && nmea.getHour() < 24 && nmea.getMinute() < 60 && nmea.getSecond() < 60;
    }
    static const unsigned long TIME_SYNC_INTERVAL = 1800000; // Re-sync every 30 minutes

public :
    MicroNMEALocationProvider(Stream& ser, mesh::RTCClock* clock = NULL, int pin_reset = GPS_RESET, int pin_en = GPS_EN,RefCountedDigitalPin* peripher_power=NULL) :
    nmea(_nmeaBuffer, sizeof(_nmeaBuffer)), _clock(clock), _gps_serial(&ser), _peripher_power(peripher_power), _pin_reset(pin_reset), _pin_en(pin_en) {
        if (_pin_reset != -1) {
            pinMode(_pin_reset, OUTPUT);
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
        }
        if (_pin_en != -1) {
            pinMode(_pin_en, OUTPUT);
            digitalWrite(_pin_en, !GPS_EN_ACTIVE);
        }
    }

    void claim() {
        _claims++;
        if (_peripher_power) _peripher_power->claim();
    }

    void release() {
        if (_claims == 0) return; // avoid negative _claims
        _claims--;
        if (_peripher_power) _peripher_power->release();
    }

    void begin() override {
        invalidate();
        claim();
        if (_pin_en != -1) {
            digitalWrite(_pin_en, GPS_EN_ACTIVE);
        }
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, !GPS_RESET_ACTIVE);
        }
    }

    void reset() override {
        invalidate();
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
            delay(10);
            digitalWrite(_pin_reset, !GPS_RESET_ACTIVE);
        }
    }

    void stop() override {
        invalidate();
        if (_pin_en != -1) {
            digitalWrite(_pin_en, !GPS_EN_ACTIVE);
        }
        if (_pin_reset != -1) {
            digitalWrite(_pin_reset, GPS_RESET_ACTIVE);
        }
        release();
    }

    bool isEnabled() override {
        // directly read the enable pin if present as gps can be
        // activated/deactivated outside of here ...
        if (_pin_en != -1) {
            return digitalRead(_pin_en) == GPS_EN_ACTIVE;
        } else {
            return true; // no enable so must be active
        }
    }

    void syncTime() override { invalidate(); LocationProvider::syncTime(); }
    long getLatitude() override { return _latitude; }
    long getLongitude() override { return _longitude; }
    long getAltitude() override { return _altitude; }
    long satellitesCount() override { return _satellites; }
    bool hasFix() override { return _have_fix; }
    uint32_t getFixAgeMillis() override {
        return _have_fix ? uint32_t(millis() - _fix_received) : UINT32_MAX;
    }
    bool isValid() override { return _have_fix && getFixAgeMillis() <= FIX_MAX_AGE; }
    bool hasAltitude() override {
        return isValid() && _have_altitude && uint32_t(millis() - _altitude_received) <= FIX_MAX_AGE;
    }
    long getTimestamp() override { return _timestamp; }

    void sendSentence(const char *sentence) override {
        nmea.sendSentence(*_gps_serial, sentence);
    }

    void loop() override {
        while (_gps_serial->available()) {
            char c = _gps_serial->read();
#ifdef GPS_NMEA_DEBUG
            Serial.print(c);
#endif
            // process() also returns true for unknown/bad-checksum sentences.
            if (!nmea.process(c) || !MicroNMEA::testChecksum(_nmeaBuffer)) continue;
            bool gga = !strcmp(nmea.getMessageID(), "GGA");
            bool rmc = !strcmp(nmea.getMessageID(), "RMC");
            if (!gga && !rmc) continue;
            if (gga) _satellites = nmea.getNumSatellites();
            if (!nmea.isValid() || !hasNumber(gga ? 2 : 3) || !hasNumber(gga ? 4 : 5) || nmea.getLatitude() < -90000000L || nmea.getLatitude() > 90000000L ||
                nmea.getLongitude() < -180000000L || nmea.getLongitude() > 180000000L) {
                _have_fix = _have_altitude = false; _valid_times = 0; continue;
            }
            uint32_t now = millis();
            _latitude = nmea.getLatitude(); _longitude = nmea.getLongitude();
            _fix_received = now; _have_fix = true;
            if (gga) {
                long altitude = 0;
                _have_altitude = hasNumber(9) && nmea.getAltitude(altitude) && altitude != LONG_MIN;
                if (_have_altitude) { _altitude = altitude; _altitude_received = now; }
            }
            // Only a newly received RMC with a complete date may set the RTC.
            // Never synchronize from a cached parser flag during serial silence.
            if (!rmc) continue;
            if (!validDateTime()) { _timestamp = 0; _valid_times = 0; continue; }
            DateTime dt(nmea.getYear(), nmea.getMonth(), nmea.getDay(), nmea.getHour(), nmea.getMinute(), nmea.getSecond());
            uint32_t timestamp = dt.unixtime();
            if (timestamp < _timestamp) _valid_times = 0;
            if (timestamp != _timestamp && _valid_times < 3) ++_valid_times;
            _timestamp = timestamp;
            if (_clock && _valid_times >= 3 &&
                (_time_sync_needed || uint32_t(now - _last_time_sync) >= TIME_SYNC_INTERVAL)) {
                _clock->setCurrentTime(timestamp);
                _time_sync_needed = false; _last_time_sync = now;
            }
        }
    }
};
