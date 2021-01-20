#pragma once 
#include <memory>
#include <vector>
#include "rtc_base/logging.h"
#include "rtc_base/location.h"
#include "rtc_base/time_utils.h"
#include "test/frame_generator.h"
#include "api/test/time_controller.h"
#include "test/scenario/scenario_config.h"
#include "api/transport/goog_cc_factory.h"
#include "test/scenario/network_node.h"
#include "test/scenario/call_client.h"
#include "test/scenario/video_stream2.h"
#include "test/scenario/transport_base.h"
namespace ns3{
enum TimeConollerType{
    EMU_CONTROLLER,
    SIMU_CONTROLLER,
};
class WebrtcSessionManager{
public:
    WebrtcSessionManager(webrtc::TimeController *controller,
                        uint32_t min_rate,uint32_t start_rate,uint32_t max_rate,
                        uint32_t height,uint32_t width);
    ~WebrtcSessionManager();
    void CreateClients();
private:
    friend class WebrtcSender;
    friend class WebrtcReceiver;
    void RegisterSenderTransport(webrtc::test::TransportBase *transport,bool own);
    void RegisterReceiverTransport(webrtc::test::TransportBase *transport,bool own);
    void CreateStreamPair();
    void Start();
    void Stop();
public:
    webrtc::TimeController *time_controller_=nullptr;
    webrtc::test::VideoStreamConfig video_stream_config_;
    webrtc::test::CallClientConfig call_client_config_;   
    webrtc::test::CallClient *sender_client_{nullptr};
    webrtc::test::CallClient *receiver_client_{nullptr};
private:
    bool m_running{false};
    std::vector<std::unique_ptr<webrtc::test::VideoStreamPair2>> video_streams_;
};
void test_match_active();
void test_field_trial();
webrtc::TimeController *CreateTimeController(TimeConollerType type,int64_t start_us,uint64_t stop_us);
}

