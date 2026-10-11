#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

namespace QuickReplies {
static constexpr unsigned Count = 10, SavedPosition = 7, GPSPosition = 8, CustomText = 9;
inline const char* label(unsigned index) {
  static const char* const labels[] = {"OK", "NO", "STATS", "YES", "THANKS", "RECEIVED", "NEED HELP", "Saved position", "GPS position", "Custom text"};
  return index < Count ? labels[index] : nullptr;
}
inline bool coordinates(char* dest, size_t size, const char* source, double lat, double lon) {
  if (!isfinite(lat) || !isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
  int n = snprintf(dest, size, "%s: %.6f, %.6f", source, lat, lon);
  return n > 0 && size_t(n) < size;
}
inline bool freshFix(uint32_t age_ms) { return age_ms <= 30000; }
// Encoded monotonic receipt time; zero denotes a non-GPS reply. At the single
// wrap boundary, use the preceding millisecond (conservative by one ms).
inline uint32_t fixToken(uint32_t now, uint32_t age_ms) {
  uint32_t token = now - age_ms + 1;
  return token ? token : UINT32_MAX;
}
inline bool previewFresh(uint32_t token, uint32_t now) {
  return token && freshFix(uint32_t(now - (token - 1)));
}
inline const char* gpsProblem(bool enabled, bool valid, uint32_t age_ms) {
  if (!enabled) return "GPS is off";
  if (!valid) return "GPS: waiting for fix";
  if (!freshFix(age_ms)) return "GPS: fix is stale";
  return nullptr;
}
inline bool build(unsigned index, double saved_lat, double saved_lon, bool gps_enabled,
                  bool gps_valid, long gps_lat, long gps_lon, uint32_t gps_age_ms,
                  uint32_t now, char* dest, size_t size, uint32_t& fix_time) {
  fix_time = 0;
  if (index >= Count || index == CustomText) return false;
  if (index < SavedPosition) {
    int n = snprintf(dest, size, "%s", label(index));
    return n > 0 && size_t(n) < size;
  }
  if (index == SavedPosition) {
    // (0,0) is the firmware's unset saved-location value.
    if (saved_lat == 0 && saved_lon == 0) return false;
    return coordinates(dest, size, "Saved position", saved_lat, saved_lon);
  }
  if (gpsProblem(gps_enabled, gps_valid, gps_age_ms)) return false;
  fix_time = fixToken(now, gps_age_ms);
  return coordinates(dest, size, "GPS position", gps_lat / 1000000.0, gps_lon / 1000000.0);
}
}
