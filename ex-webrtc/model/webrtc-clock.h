#pragma once
#include <stdint.h>
#include <functional>
#include <memory>
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
namespace ns3{
void webrtc_register_clock();
uint32_t webrtc_time32();
int64_t webrtc_time_millis();
int64_t webrtc_time_micros();
int64_t webrtc_time_nanos();
}
namespace webrtc{
class WebrtcSimulationClock : public Clock {
 public:
  explicit WebrtcSimulationClock(){}
  ~WebrtcSimulationClock() override{}

  // Return a timestamp relative to some arbitrary source; the source is fixed
  // for this clock.
  Timestamp CurrentTime() override;

  // Retrieve an NTP absolute timestamp.
  NtpTime CurrentNtpTime() override;

  // Converts an NTP timestamp to a millisecond timestamp.
  int64_t CurrentNtpInMilliseconds() override;
};    
}
