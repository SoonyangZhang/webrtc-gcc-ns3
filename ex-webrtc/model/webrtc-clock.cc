#include <utility>
#include "ns3/simulator.h"
#include "ns3/nstime.h"

#include "webrtc-clock.h"
namespace ns3{
namespace{
    static  bool webrtc_clock_init=false;
    static SimulationWebrtcClock webrtc_clock;
}
int64_t SimulationWebrtcClock::TimeNanos() const{
    return Simulator::Now().GetNanoSeconds();
}
void set_test_clock_webrtc(){
    if(!webrtc_clock_init){
        rtc::SetClockForTesting(&webrtc_clock);
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
