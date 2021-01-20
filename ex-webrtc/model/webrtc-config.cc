#include "webrtc-config.h"
#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <memory>
#include <unistd.h>
#include <iostream>

#include "test/scenario/scenario_config.h"
#include "test/scenario/video_frame_matcher.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "api/transport/field_trial_based_config.h"
#include "webrtc-emu-controller.h"
#include "webrtc-simu-controller.h"
namespace ns3{
namespace{
const uint32_t kInitialBitrateKbps = 60;
const webrtc::DataRate kInitialBitrate = webrtc::DataRate::KilobitsPerSec(kInitialBitrateKbps);
const float kDefaultPacingRate = 2.5f;    
}
WebrtcSessionManager::WebrtcSessionManager(webrtc::TimeController *controller,
                        uint32_t min_rate,uint32_t start_rate,uint32_t max_rate,
                        uint32_t height,uint32_t width):time_controller_(controller){
    call_client_config_.transport.rates.min_rate=webrtc::DataRate::KilobitsPerSec(min_rate);
    call_client_config_.transport.rates.start_rate=webrtc::DataRate::KilobitsPerSec(start_rate);
    call_client_config_.transport.rates.max_rate=webrtc::DataRate::KilobitsPerSec(max_rate);
    
    video_stream_config_.source.generator.height=height; 
    video_stream_config_.source.generator.width=width;

    webrtc::GoogCcFactoryConfig config;
    config.feedback_only = true;
    call_client_config_.transport.cc_factory=
    new webrtc::GoogCcNetworkControllerFactory(std::move(config));
}
WebrtcSessionManager::~WebrtcSessionManager(){
    webrtc::NetworkControllerFactoryInterface *cc_factory
    =call_client_config_.transport.cc_factory;
    if(cc_factory){
        call_client_config_.transport.cc_factory=nullptr;
        delete cc_factory;
    }
    if(m_running){
        Stop();
    }
    if(sender_client_){
        delete sender_client_;
        sender_client_=nullptr;
    }
    if(receiver_client_){
        delete receiver_client_;
        receiver_client_=nullptr;
    }
   video_streams_.clear();
}
void WebrtcSessionManager::CreateClients(){
    sender_client_=new webrtc::test::CallClient(time_controller_,nullptr,call_client_config_);
    receiver_client_=new webrtc::test::CallClient(time_controller_,nullptr,call_client_config_);
}
void WebrtcSessionManager::RegisterSenderTransport(webrtc::test::TransportBase *transport,bool own){
    if(sender_client_){
        sender_client_->SetCustomTransport(transport,own);
    }
}
void WebrtcSessionManager::RegisterReceiverTransport(webrtc::test::TransportBase *transport,bool own){
    if(receiver_client_){
        receiver_client_->SetCustomTransport(transport,own);
    }
    
}
void WebrtcSessionManager::CreateStreamPair(){
      video_streams_.emplace_back(
      new webrtc::test::VideoStreamPair2(sender_client_,receiver_client_, video_stream_config_));
}
void WebrtcSessionManager::Start(){
  m_running=true;
  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->Start();
  for (auto& stream_pair : video_streams_) {
    if (video_stream_config_.autostart) {
      stream_pair->send()->Start();
    }
  }
}
void WebrtcSessionManager::Stop(){
  if(!m_running){return ;}
  m_running=false;
  for (auto& stream_pair : video_streams_) {
    stream_pair->send()->Stop();
  }
  for (auto& stream_pair : video_streams_)
    stream_pair->receive()->Stop(); 
}
void test_match_active(){
    webrtc::test::VideoStreamConfig config=webrtc::test::VideoStreamConfig();
    config.source.generator.width=1280;
    config.source.generator.height=720;
    webrtc::test::VideoFrameMatcher matcher(config.hooks.frame_pair_handlers);
    if(!matcher.Active()){
        std::cout<<"in active"<<std::endl;
    }
}
bool IsEnabled(const webrtc::WebRtcKeyValueConfig* trials, absl::string_view key) {
  RTC_DCHECK(trials != nullptr);
  return absl::StartsWith(trials->Lookup(key), "Enabled");
}
void test_field_trial(){
    webrtc::FieldTrialBasedConfig trial;
    bool ret=IsEnabled(&trial,"WebRTC-TaskQueuePacer/Enabled");
    if(ret){
        std::cout<<"what the fuck"<<std::endl;
    }
    
}
webrtc::TimeController *CreateTimeController(TimeConollerType type,int64_t start_us,uint64_t stop_us){
    if(TimeConollerType::SIMU_CONTROLLER==type){
        return new webrtc::SimulationTimeController(start_us,stop_us);
    }else{
        return new webrtc::EmulationTimeController();
    }
    
}
}
