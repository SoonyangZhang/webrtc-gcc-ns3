#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <utility>
#include <algorithm>
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


#include <time.h>
#include <sys/time.h>

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("webrtc-static");

const uint32_t DEFAULT_PACKET_SIZE = 1500;
const uint32_t kBwUnit=1000000;

uint64_t get_os_millis()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

class TriggerRandomLoss{
public:
    TriggerRandomLoss(){}
    ~TriggerRandomLoss(){
        if(m_timer.IsRunning()){
            m_timer.Cancel();
        }
    }
    void RegisterDevice(Ptr<NetDevice> dev){
        m_dev=dev;
    }
    void Start(){
        Time next=Seconds(2);
        m_timer=Simulator::Schedule(next,&TriggerRandomLoss::ConfigureRandomLoss,this);
    }
    void ConfigureRandomLoss(){
        if(m_timer.IsExpired()){
            std::string errorModelType = "ns3::RateErrorModel";
            ObjectFactory factory;
            factory.SetTypeId (errorModelType);
            Ptr<ErrorModel> em = factory.Create<ErrorModel> ();
            m_dev->SetAttribute ("ReceiveErrorModel", PointerValue (em));            
            m_timer.Cancel();
        }
    }
private:
    Ptr<NetDevice> m_dev;
    EventId m_timer;
};
struct CompareV
{
    bool operator() (const std::pair<Time,int64_t> &a,const std::pair<Time,int64_t> &b)
    {
        return a.first<b.first;
    }
};
class BandwidthChanger
{
public:
    BandwidthChanger(){}
    ~BandwidthChanger(){
        if (m_timer.IsRunning()) {
            m_timer.Cancel();
        }
    }
    
    void RegisterDevice(Ptr<NetDevice> dev) {
        m_dev=dev;
    }
    
    void Config(int64_t initial_bps,std::vector<std::pair<Time,int64_t> >& info) {
        m_initialRate=initial_bps;
        m_info.swap(info);
    }
    
    void TotalThroughput(Time stop,int64_t &channel_bit){
        int index=lower_bound_index(stop);
        if (0 == index){
            channel_bit=0;
        }else{
            int64_t bit=0;
            for(int i=0;i<index;i++){
                if(0 == i){
                    bit+=m_initialRate*(m_info.at(i).first.GetMilliSeconds()/1000);
                }else{
                    bit+=m_info.at(i-1).second*((m_info.at(i).first-m_info.at(i-1).first).GetMilliSeconds()/1000);
                }
            }
            if (stop > m_info.back().first) {
                bit+=m_info.back().second*((stop-m_info.back().first).GetMilliSeconds()/1000);
            }
            channel_bit=bit;
        }
    }
    void Start()
    {
        if (m_info.size()>0) {
            Time next=m_info[m_index].first;
            m_timer=Simulator::Schedule(next,&BandwidthChanger::ResetBandwidth,this);
        }
    }
    void ResetBandwidth()
    {
        if (m_timer.IsExpired() && m_dev) {
            PointToPointNetDevice *device=static_cast<PointToPointNetDevice*>(PeekPointer(m_dev));
            device->SetDataRate(DataRate(m_info[m_index].second));
            NS_LOG_INFO(Simulator::Now().GetSeconds()<<" "<<m_info[m_index].second);
            m_index++;
            if (m_index < m_info.size()){
                NS_ASSERT(m_info[m_index].first>m_info[m_index-1].first);
                Time next=m_info[m_index].first-m_info[m_index-1].first;
                m_timer=Simulator::Schedule(next,&BandwidthChanger::ResetBandwidth,this);
            }
        }
    }

    int lower_bound_index(Time point){
        auto ele=std::make_pair(point,0);
        auto iter=std::lower_bound(m_info.begin(), m_info.end(),ele,CompareV());
        return iter-m_info.begin();
    }
private:
    int64_t m_initialRate=0;
    std::vector<std::pair<Time,int64_t> > m_info;
    uint32_t m_index{0};
    Ptr<NetDevice> m_dev;
    EventId m_timer;
};

struct LinkProperty{
    uint16_t nodes[2];
    uint32_t bandwidth;
    uint32_t propagation_ms;
};
uint32_t CalMaxRttInDumbbell(LinkProperty *topoinfo,int links){
    uint32_t rtt1=2*(topoinfo[0].propagation_ms+topoinfo[1].propagation_ms+topoinfo[2].propagation_ms);
    uint32_t rtt2=2*(topoinfo[1].propagation_ms+topoinfo[3].propagation_ms+topoinfo[4].propagation_ms);
    return std::max<uint32_t>(rtt1,rtt2);
}

/** Network topology
 *       n0            n1
 *        |            | 
 *        | l0         | l2
 *        |            | 
 *        n2---l1------n3
 *        |            | 
 *        |  l3        | l4
 *        |            | 
 *        n4           n5
 */

int ip=1;
static NodeContainer BuildDumbbellTopo(LinkProperty *topoinfo,int links,int bottleneck_i,
                                    uint32_t buffer_ms,TriggerRandomLoss *trigger=nullptr)
{
    int hosts=links+1;
    NodeContainer topo;
    topo.Create (hosts);
    InternetStackHelper stack;
    stack.Install (topo);
    for (int i=0;i<links;i++){
        uint16_t src=topoinfo[i].nodes[0];
        uint16_t dst=topoinfo[i].nodes[1];
        uint32_t bps=topoinfo[i].bandwidth;
        uint32_t owd=topoinfo[i].propagation_ms;
        NodeContainer nodes=NodeContainer (topo.Get (src), topo.Get (dst));
        auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * buffer_ms / 8000);
        int packets=bufSize/DEFAULT_PACKET_SIZE;
        std::cout<<bps<<std::endl;
        PointToPointHelper pointToPoint;
        pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
        pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (owd)));
        if(bottleneck_i==i){
            pointToPoint.SetQueue ("ns3::DropTailQueue","MaxSize", StringValue (std::to_string(20)+"p"));
        }else{
            pointToPoint.SetQueue ("ns3::DropTailQueue","MaxSize", StringValue (std::to_string(packets)+"p"));   
        }
        NetDeviceContainer devices = pointToPoint.Install (nodes);
        if(bottleneck_i==i){
            TrafficControlHelper pfifoHelper;
            uint16_t handle = pfifoHelper.SetRootQueueDisc ("ns3::FifoQueueDisc", "MaxSize", StringValue (std::to_string(packets)+"p"));
            pfifoHelper.AddInternalQueues (handle, 1, "ns3::DropTailQueue", "MaxSize",StringValue (std::to_string(packets)+"p"));
            pfifoHelper.Install(devices);  
        }
        Ipv4AddressHelper address;
        std::string nodeip="10.1."+std::to_string(ip)+".0";
        ip++;
        address.SetBase (nodeip.c_str(), "255.255.255.0");
        address.Assign (devices);
        if(bottleneck_i==i&&trigger){
            trigger->RegisterDevice(devices.Get(1));
        }
    }
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    return topo;
}
static NodeContainer BuildP2PTopo(TriggerRandomLoss *trigger,
                                  BandwidthChanger *changer,
                                  uint64_t bps,
                                  uint32_t msDelay,
                                  uint32_t msQdelay)
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    
    int packets=bufSize/DEFAULT_PACKET_SIZE;
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "MaxSize", StringValue (std::to_string(5)+"p"));
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
    if(trigger){
        trigger->RegisterDevice(devices.Get(0));
    }
    if(changer){
        changer->RegisterDevice(devices.Get(0));
    }
    return nodes;
}
static void InstallWebrtcApplication( Ptr<Node> sender,
                        Ptr<Node> receiver,
                        uint16_t send_port,
                        uint16_t recv_port,
                        Time start_app,
                        Time stop_app,
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
        if (trace->LogFlag()&WebrtcTrace::E_WEBRTC_BW) {
            sendApp->SetBwTraceFuc(MakeCallback(&WebrtcTrace::OnBW,trace));
        }
        if (trace->LogFlag()&WebrtcTrace::E_WEBRTC_OWD || trace->LogFlag()&WebrtcTrace::E_WEBRTC_LOSS) {
            recvApp->SetTraceReceiptPktInfo(MakeCallback(&WebrtcTrace::OnReceiptPktInfo,trace));
        }
    }
    sendApp->SetStartTime(start_app);
    sendApp->SetStopTime(stop_app);
    recvApp->SetStartTime(start_app);
    recvApp->SetStopTime(stop_app+Seconds(1));
}
uint64_t kMillisPerSecond=1000;
uint64_t kMicroPerMillis=1000;
std::unique_ptr<WebrtcSessionManager> CreateWebrtcSessionManager(webrtc::TimeController *controller,
uint32_t max_rate=1000,uint32_t min_rate=300,uint32_t start_rate=500,uint32_t h=720,uint32_t w=1280){
    std::unique_ptr<WebrtcSessionManager> webrtc_manager(new WebrtcSessionManager(controller,min_rate,start_rate,max_rate,h,w));
    webrtc_manager->CreateClients();
    return webrtc_manager;
}
typedef struct {
    float start;
    float stop;
}client_config_t;
void test_app_on_p2p(std::string &instance,TimeConollerType controller_type,
                    float app_start,float app_stop,
                    client_config_t *config,int num,
                    TriggerRandomLoss *trigger_loss,BandwidthChanger *changer){
    uint64_t bps=6*kBwUnit;
    uint32_t link_delay=30;//milliseconds;
    uint32_t buffer_delay=30;//ms
    if (0 == instance.compare("2")) {
        link_delay=30;
        buffer_delay=60;
    }else if (0 == instance.compare("3")) {
        link_delay=30;
        buffer_delay=90;
    }else if (0 == instance.compare("4")) {
        link_delay=50;
        buffer_delay=50;
    }else if (0 == instance.compare("5")) {
        link_delay=50;
        buffer_delay=100;
    }else if (0 == instance.compare("6")) {
        link_delay=50;
        buffer_delay=150;
    }else if (0 == instance.compare("7")) {
        bps=10*kBwUnit;
        link_delay=50;
        buffer_delay=50;
    }else if (0 == instance.compare("8")) {
        bps=10*kBwUnit;
        link_delay=50;
        buffer_delay=100;
    }else if (0 == instance.compare("9")) {
        bps=10*kBwUnit;
        link_delay=50;
        buffer_delay=150;
    }else if (0 == instance.compare("10")) {
        bps=10*kBwUnit;
        link_delay=50;
        buffer_delay=100;
        for(int i=0;i<num;i++){
            config[i].start=app_start;
        }
    }else if (0 == instance.compare("11")){
        bps=5*kBwUnit;
        link_delay=50;
        buffer_delay=100;
        if(changer){
            std::vector<std::pair<Time,int64_t> > bw_info;
            bw_info.push_back(std::make_pair(Seconds(20),4*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(40),3*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(60),2*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(80),1*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(100),2*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(120),3*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(140),4*kBwUnit));
            bw_info.push_back(std::make_pair(Seconds(180),5*kBwUnit));
            changer->Config(bps,bw_info);
            changer->Start();
        }
    }

    NodeContainer nodes = BuildP2PTopo(trigger_loss,changer,bps,link_delay,buffer_delay);
    std::string webrtc_log_com("_gcc_");
    int64_t webrtc_start_us=1;
    int64_t webrtc_stop_us=app_stop*kMillisPerSecond*kMicroPerMillis;
    webrtc::TimeController* time_controller=CreateTimeController(controller_type,webrtc_start_us,webrtc_stop_us);
    uint32_t max_rate=bps/1000;
    
    std::vector<std::unique_ptr<WebrtcSessionManager>> sesssion_manager;
    for (int i=0;i<num;i++) {
        std::unique_ptr<WebrtcSessionManager> m(CreateWebrtcSessionManager(time_controller,max_rate));
        sesssion_manager.push_back(std::move(m)); 
    }
    UtilCalculator *calculator=UtilCalculator::Instance();
    calculator->Enable();
    
    uint16_t sendPort=5432;
    uint16_t recvPort=5000;
    
    std::string prefix=instance+webrtc_log_com;
    std::vector<WebrtcTrace*> trace_vec;
    for (int i=0;i<num;i++) {
        
        std::string log=prefix+std::to_string(i+1);
        WebrtcTrace *trace=new WebrtcTrace();
        trace_vec.push_back(trace);
        trace->Log(log,WebrtcTrace::E_WEBRTC_ALL);
        InstallWebrtcApplication(nodes.Get(0),nodes.Get(1),sendPort,recvPort,
                                Seconds(config[i].start),Seconds(config[i].stop),
                               sesssion_manager.at(i).get(),trace);
        sendPort++;
        recvPort++;
    }


    Simulator::Stop (Seconds(app_stop+10));
    uint64_t last=get_os_millis();
    Simulator::Run ();
    Simulator::Destroy();
    if(time_controller){
        delete time_controller;
    }
    {
        int64_t last_stamp=calculator->GetLastReceiptMillis();
        int64_t channnel_bit=0;
        if (changer){
            changer->TotalThroughput(MilliSeconds(last_stamp),channnel_bit);
        }else{
            if(last_stamp>app_start*1000){
                int64_t duration=last_stamp-app_start*1000;
                channnel_bit=bps*(duration/1000);
            }
        }
        NS_LOG_INFO("channel byte "<<(uint32_t)channnel_bit/8);
        calculator->CalculateUtil(prefix,channnel_bit);
    }
    for(auto it=trace_vec.begin();it!=trace_vec.end();it++){
        WebrtcTrace *trace=(*it);
        delete trace;
    }
    trace_vec.clear();
    uint32_t elapse=(get_os_millis()-last);
    std::cout<<"run time millis: "<<elapse<<std::endl;
    exit(0);
    return ;
}

static const float startTime=0.001;
static const float simDuration=200.0;

/*
 ./waf --run "scratch/webrtc-static --m=simu --it=1"
 ./waf --run "scratch/webrtc-static --m=simu --topo=change --it=11 --folder=change"
 ./waf --run "scratch/webrtc-static --m=simu --topo=random --it=1  --folder=losszero"
 ./waf --run "scratch/webrtc-static --m=simu --topo=random --it=1 --lo=50 --folder=loss5"
*/
int main(int argc, char *argv[]){
    LogComponentEnable("webrtc-static",LOG_LEVEL_ALL);
    LogComponentEnable("WebrtcSender",LOG_LEVEL_ALL);
    LogComponentEnable("WebrtcReceiver",LOG_LEVEL_ALL);
    std::string mode("simu");
    std::string topo("p2p");
    std::string instance=std::string("1");
    std::string loss_str("0");
    std::string data_folder("no-one");
    CommandLine cmd;
    cmd.AddValue("m","mode",mode);
    cmd.AddValue ("topo", "topology", topo);
    cmd.AddValue ("it", "instacne", instance);
    cmd.AddValue ("lo", "loss",loss_str);
    cmd.AddValue ("folder", "folder name to collect data", data_folder);
    cmd.Parse (argc, argv);
    TimeConollerType controller_type=TimeConollerType::SIMU_CONTROLLER;
    if (0==mode.compare("simu")){
        webrtc_register_clock();
    }else if(0==mode.compare("emu")){
        controller_type=TimeConollerType::EMU_CONTROLLER;
        GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl")); 
    }else{
        return -1;
    }
    {
        char buffer[128] = {0};
        if (getcwd(buffer,sizeof(buffer)) != buffer) {
            NS_LOG_ERROR("path error");
            return -1;
        }
        std::string ns3_path(buffer,::strlen(buffer));
        if ('/'!=ns3_path.back()){
            ns3_path.push_back('/');
        }
        std::string pathname=ns3_path+"traces/"+data_folder;
        set_webrtc_trace_folder(pathname);
    }
    int loss_integer=std::stoi(loss_str);
    std::unique_ptr<TriggerRandomLoss> triggerloss=nullptr;
    std::unique_ptr<BandwidthChanger> changer=nullptr; 
    if (loss_integer>0) {
        double random_loss=loss_integer*1.0/1000;
        Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (random_loss));
        Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
        Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (random_loss));
        Config::SetDefault ("ns3::BurstErrorModel::BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=3]"));
        triggerloss.reset(new TriggerRandomLoss());
        triggerloss->Start();
    }
    if (0 == topo.compare("dumbbell")) {
        
    }else if (0 == topo.compare("change")) {
        client_config_t config[1]={
            [0]={.start=startTime,.stop=simDuration},
        };
        changer.reset(new BandwidthChanger());
        test_app_on_p2p(instance,controller_type,startTime,simDuration,config,1,
                        triggerloss.get(),changer.get());
    }else if (0 == topo.compare("random")) {
        client_config_t config[3]={
            [0]={.start=startTime,.stop=simDuration},
            [1]={.start=startTime,.stop=simDuration},
            [2]={.start=startTime,.stop=simDuration},
        };
        test_app_on_p2p(instance,controller_type,startTime,simDuration,config,3,
                        triggerloss.get(),changer.get());
    }else {
        client_config_t config[3]={
            [0]={.start=startTime,.stop=simDuration},
            [1]={.start=startTime+35,.stop=simDuration},
            [2]={.start=startTime+80,.stop=simDuration},
        };
        test_app_on_p2p(instance,controller_type,startTime,simDuration,config,3,
                        triggerloss.get(),changer.get());
    }
    return 0;
}
