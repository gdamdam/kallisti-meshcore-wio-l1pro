#pragma once
#include "QuickReplies.h"

// Snapshot of only the provider data that this screen can actually report.
struct GPSPanelData {
  bool available, enabled, valid;
  long satellites, latitude, longitude, altitude;
  uint32_t fix_age_ms;
  bool altitude_valid;
};
inline void renderGPSPanel(DisplayDriver& display, const GPSPanelData& gps) {
  display.setTextSize(1); display.setColor(UIColor::primary_txt);
  bool fresh = gps.enabled && gps.valid && QuickReplies::freshFix(gps.fix_age_ms);
  const char* state = !gps.available ? "UNAVAILABLE" : !gps.enabled ? "OFF" :
      !gps.valid ? "ACQUIRING" : fresh ? "FIX" : "STALE";
  char line[48];
  snprintf(line, sizeof(line), "GPS %s", state);
  display.drawTextLeftAlign(0, 14, line);
  if (gps.available && gps.enabled) {
    snprintf(line, sizeof(line), "%ld sat", gps.satellites);
    display.drawTextRightAlign(display.width()-1, 14, line);
  }
  if (gps.enabled && gps.valid) {
    snprintf(line, sizeof(line), "Lat %.5f %c", fabs(gps.latitude / 1000000.), gps.latitude < 0 ? 'S' : 'N');
    display.drawTextLeftAlign(0, 25, line);
    snprintf(line, sizeof(line), "Lon %.5f %c", fabs(gps.longitude / 1000000.), gps.longitude < 0 ? 'W' : 'E');
    display.drawTextLeftAlign(0, 35, line);
    char age[16];
    if (gps.fix_age_ms == UINT32_MAX) snprintf(age, sizeof(age), "?");
    else if (gps.fix_age_ms / 1000 > 999) snprintf(age, sizeof(age), ">999s");
    else snprintf(age, sizeof(age), "%lus", (unsigned long)(gps.fix_age_ms / 1000));
    if (gps.altitude_valid) snprintf(line, sizeof(line), "Alt %.0fm Fix %s", gps.altitude / 1000., age);
    else snprintf(line, sizeof(line), "Alt -- Fix %s", age);
    display.drawTextEllipsized(0, 45, display.width(), line);
  } else {
    display.drawTextLeftAlign(0, 28, gps.enabled ? "Acquiring satellites" : "GPS location not live");
    display.drawTextLeftAlign(0, 41, gps.enabled ? "Use a clear sky view" : "Press OK to enable");
  }
  display.setCursor(0, 56); display.print("OK:toggle Down:share");
}
