#include <iostream>
#include <string>
#include "ns3/webrtc-defines.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/log.h"
#include "ns3/ex-webrtc-module.h"

#include <deque>
using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("Webrtc-Static");
const uint32_t TOPO_DEFAULT_BW     = 3000000;  
const uint32_t TOPO_DEFAULT_PDELAY =100;
const uint32_t TOPO_DEFAULT_QDELAY =300;
const uint32_t DEFAULT_PACKET_SIZE = 1000;
static NodeContainer BuildExampleTopo (uint64_t bps,
                                       uint32_t msDelay,
                                       uint32_t msQdelay,
                                       bool lossy=false){
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    int packets=bufSize/DEFAULT_PACKET_SIZE;
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "MaxSize", StringValue (std::to_string(1)+"p"));
    //pointToPoint.SetQueue ("ns3::DropTailQueue");
    NetDeviceContainer devices = pointToPoint.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);

    TrafficControlHelper pfifoHelper;
    uint16_t handle = pfifoHelper.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (std::to_string(packets)+"p"));
    pfifoHelper.AddInternalQueues (handle, 1, "ns3::DropTailQueue", "MaxSize",StringValue (std::to_string(packets)+"p"));
    pfifoHelper.Install(devices);


    Ipv4AddressHelper address;
    std::string nodeip="10.1.1.0";
    address.SetBase (nodeip.c_str(), "255.255.255.0");
    address.Assign (devices);
    if(lossy){
    std::string errorModelType = "ns3::RateErrorModel";
    ObjectFactory factory;
    factory.SetTypeId (errorModelType);
    Ptr<ErrorModel> em = factory.Create<ErrorModel> ();
    devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));
    }
    return nodes;
}
static void InstallWebrtcApplication( Ptr<Node> sender,
                        Ptr<Node> receiver,
                        uint16_t send_port,
                        uint16_t recv_port,
                        int64_t start_ms,
                        int64_t stop_ms,
                        WebrtcSessionManager *manager,
                        WebrtcTrace *trace=nullptr)
{
    Ptr<WebrtcSender> sendApp = CreateObject<WebrtcSender> (manager);
    Ptr<WebrtcReceiver> recvApp = CreateObject<WebrtcReceiver>(manager);
    sender->AddApplication (sendApp);
    receiver->AddApplication (recvApp);
    sendApp->Bind(send_port);
    recvApp->Bind(recv_port);
    Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4> ();
    Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal ();
    sendApp->ConfigurePeer(addr,recv_port);
    ipv4=sender->GetObject<Ipv4> ();
    addr=ipv4->GetAddress (1, 0).GetLocal ();
    recvApp->ConfigurePeer(addr,send_port);
    if(trace){
        if(trace->LogFlag()&WebrtcTrace::E_WEBRTC_BW){
            sendApp->SetBwTraceFuc(MakeCallback(&WebrtcTrace::OnBW,trace));
        }
        if(trace->LogFlag()&WebrtcTrace::E_WEBRTC_OWD){
            recvApp->SetOwdTraceFuc(MakeCallback(&WebrtcTrace::OnOwd,trace));
        }
    }
    sendApp->SetStartTime (MilliSeconds(start_ms));
    sendApp->SetStopTime (MilliSeconds(stop_ms));
    recvApp->SetStartTime (MilliSeconds(start_ms));
    recvApp->SetStopTime (MilliSeconds(stop_ms));
}
static int64_t simStopMilli=100000;
int64_t appStartMills=1;
float appStopMills=simStopMilli-1;
uint64_t kMillisPerSecond=1000;
uint64_t kMicroPerMillis=1000;
std::unique_ptr<WebrtcSessionManager> CreateWebrtcSessionManager(webrtc::TimeController *controller,
uint32_t max_rate=1000,uint32_t min_rate=300,uint32_t start_rate=500,uint32_t h=720,uint32_t w=1280){
    std::unique_ptr<WebrtcSessionManager> webrtc_manaager(new WebrtcSessionManager(controller,min_rate,start_rate,max_rate,h,w));
    webrtc_manaager->CreateClients();
    return webrtc_manaager;
}
int main(int argc, char *argv[]){
    LogComponentEnable("WebrtcSender",LOG_LEVEL_ALL);
    LogComponentEnable("WebrtcReceiver",LOG_LEVEL_ALL);
    TimeConollerType controller_type=TimeConollerType::EMU_CONTROLLER;
    //TimeConollerType controller_type=TimeConollerType::SIMU_CONTROLLER;
    if(TimeConollerType::EMU_CONTROLLER==controller_type){
        GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));        
    }else if(TimeConollerType::SIMU_CONTROLLER==controller_type){
        webrtc_register_clock();
    }
	uint64_t linkBw   = TOPO_DEFAULT_BW;
    uint32_t msDelay  = TOPO_DEFAULT_PDELAY;
    uint32_t msQDelay = TOPO_DEFAULT_QDELAY;
    CommandLine cmd;
    std::string instance=std::string("1");
    std::string loss_str("0");
    cmd.AddValue ("it", "instacne", instance);
	cmd.AddValue ("lo", "loss",loss_str);
    cmd.Parse (argc, argv);
    int loss_integer=std::stoi(loss_str);
    double loss_rate=loss_integer*1.0/1000;
    std::string webrtc_log_com;
    if(loss_integer>0){
        webrtc_log_com="_gccl"+std::to_string(loss_integer)+"_";
    }else{
        webrtc_log_com="_gcc_";
    }
    bool random_loss=false;
    if(loss_integer>0){
        Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (loss_rate));
        Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
        Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (loss_rate));
        Config::SetDefault ("ns3::BurstErrorModel::BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=3]"));
        random_loss=true;
    }
    int64_t webrtc_start_us=appStartMills*kMicroPerMillis+1;
    int64_t webrtc_stop_us=appStopMills*kMicroPerMillis;
    webrtc::TimeController* time_controller=CreateTimeController(controller_type,webrtc_start_us,webrtc_stop_us);
    uint32_t max_rate=linkBw/1000;
    std::unique_ptr<WebrtcSessionManager> webrtc_manaager1=std::move(CreateWebrtcSessionManager(
    time_controller,max_rate)); 
    std::unique_ptr<WebrtcSessionManager> webrtc_manaager2=std::move(CreateWebrtcSessionManager(
    time_controller,max_rate));
    std::unique_ptr<WebrtcSessionManager> webrtc_manaager3=std::move(CreateWebrtcSessionManager(
    time_controller,max_rate));

    NodeContainer nodes = BuildExampleTopo(linkBw, msDelay, msQDelay,random_loss);

    uint16_t sendPort=5432;
    uint16_t recvPort=5000;
    
    int test_pair=1;
    std::string log=instance+webrtc_log_com+std::to_string(test_pair);    
    WebrtcTrace trace1;
    trace1.Log(log,WebrtcTrace::E_WEBRTC_BW|WebrtcTrace::E_WEBRTC_OWD);
    InstallWebrtcApplication(nodes.Get(0),nodes.Get(1),sendPort,recvPort,appStartMills,appStopMills,
    webrtc_manaager1.get(),&trace1);
    sendPort++;
    recvPort++;
    test_pair++;


    log=instance+webrtc_log_com+std::to_string(test_pair);    
    WebrtcTrace trace2;
    trace2.Log(log,WebrtcTrace::E_WEBRTC_BW);
    InstallWebrtcApplication(nodes.Get(0),nodes.Get(1),sendPort,recvPort,appStartMills+5*kMillisPerSecond,appStopMills,
    webrtc_manaager2.get(),&trace2);
    sendPort++;
    recvPort++;
    test_pair++;
    
    log=instance+webrtc_log_com+std::to_string(test_pair);    
    WebrtcTrace trace3;
    trace3.Log(log,WebrtcTrace::E_WEBRTC_BW);
    InstallWebrtcApplication(nodes.Get(0),nodes.Get(1),sendPort,recvPort,appStartMills+10*kMillisPerSecond,appStopMills,
    webrtc_manaager3.get(),&trace3);
    sendPort++;
    recvPort++;
    test_pair++;    
    
    Simulator::Stop (MilliSeconds(simStopMilli));
    Simulator::Run ();
    Simulator::Destroy();
    if(time_controller){
        delete time_controller;
    }
    std::cout<<"run out"<<std::endl;
    exit(0);
    return 0;
}
