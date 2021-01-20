#include <utility>
#include "ns3/simulator.h"
#include "ns3/nstime.h"
#include "webrtc-clock.h"
namespace ns3{
class ExternalClock:public rtc::ClockInterface{
public:
    ExternalClock(){}
    ~ExternalClock() override{}
    int64_t TimeNanos() const override{
        return Simulator::Now().GetNanoSeconds();
    }
};

static  bool webrtc_clock_init=false;
static ExternalClock external_clock;


void webrtc_register_clock(){
    if(!webrtc_clock_init){
        rtc::SetClockForTesting(&external_clock);
        webrtc_clock_init=true;
    }
    
}
uint32_t webrtc_time32(){
    return rtc::Time32();
}
int64_t webrtc_time_millis(){
    return rtc::TimeMillis();
}
int64_t webrtc_time_micros(){
    return rtc::TimeMicros();
}
int64_t webrtc_time_nanos(){
    return rtc::TimeNanos();
}
}

namespace webrtc{
Timestamp WebrtcSimulationClock::CurrentTime() {
  return Timestamp::Micros(rtc::TimeMicros());
}

NtpTime WebrtcSimulationClock::CurrentNtpTime() {
  int64_t now_ms =rtc::TimeMillis();
  uint32_t seconds = (now_ms / 1000) + kNtpJan1970;
  uint32_t fractions =
      static_cast<uint32_t>((now_ms % 1000) * kMagicNtpFractionalUnit / 1000);
  return NtpTime(seconds, fractions);
}

int64_t WebrtcSimulationClock::CurrentNtpInMilliseconds() {
  return TimeInMilliseconds() + 1000 * static_cast<int64_t>(kNtpJan1970);
}
}
