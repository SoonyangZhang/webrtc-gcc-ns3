#pragma once
#include <stdint.h>
#include <functional>
#include <memory>
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
namespace ns3{
class SimulationWebrtcClock:public rtc::ClockInterface{
public:
    SimulationWebrtcClock(){}
    int64_t TimeNanos() const override;
    ~SimulationWebrtcClock() override{}
};
void set_test_clock_webrtc();
uint32_t webrtc_time32();
int64_t webrtc_time_millis();
int64_t webrtc_time_micros();
int64_t webrtc_time_nanos();
}
