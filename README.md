# webrtc-gcc-ns3
test google congestion control on ns3.31  
## build webrtc
1 download webrtc m84.  
```
mkdir webrtc-checkout  
cd webrtc-checkout  
fetch --nohooks webrtc  
gclient sync  
cd src   
git checkout -b m84 refs/remotes/branch-heads/4147   
gclient sync  
```
2 Add a flag in basic_types.h (third_party/libyuv/include/libyuv):
```
#define LIBYUV_LEGACY_TYPES  
```
3 build it with GCC.  
```
gn gen out/m84 --args='is_debug=false is_component_build=false is_clang=false rtc_include_tests=false rtc_use_h264=true rtc_enable_protobuf=false use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false use_ozone=true'   
```
4 Add two files to rtc_base library(BUILD.gn):  
```
rtc_library("rtc_base") {
  sources = [
    "memory_stream.cc",
    "memory_stream.h",
    "memory_usage.cc",
    "memory_usage.h",]
}
```
And Remove them out of the original library(rtc_library("rtc_base_tests_utils")).  
I dont want to enable the build flag rtc_include_tests.  
delete code in webrtc:  
```
//third_party/webrtc/modules/rtp_rtcp/source/rtp_rtcp_impl.cc   
bool ModuleRtpRtcpImpl::TrySendPacket(RtpPacketToSend* packet,  
                                      const PacedPacketInfo& pacing_info) {  
  RTC_DCHECK(rtp_sender_);  
  //if (!rtp_sender_->packet_generator.SendingMedia()) {   
 //   return false;  
 // }  
  rtp_sender_->packet_sender.SendPacket(packet, pacing_info);  
  return true;  
}
```
5 Add code in webrtc(to get send bandwidth):  
```
//third_party/webrtc/call/call.h  
class Call {  
virtual uint32_t last_bandwidth_bps(){return 0;}  
};  
//third_party/webrtc/call/call.cc  
namespace internal {  
class Call{
uint32_t last_bandwidth_bps() override {return last_bandwidth_bps_;}  
}
}  
```
6  build:  
```
ninja -C out/m84  
```
7 add external header path to CPLUS_INCLUDE_PATH.  
gedit source /etc/profile  
```
//add  header path
export WEBRTC_INC=/home/zsy/webrtc/src  
export ABSL_INC=/home/zsy/webrtc/src/third_party/abseil-cpp  
export CPLUS_INCLUDE_PATH=CPLUS_INCLUDE_PATH:$WEBRTC_INC:$ABSL_INC  
```
8 The path (/home/zsy/webrtc/) is where I put webrtc source code.  
## the ns3 part.
1 Enable c++14 build flag (ns-allinone-3.31/ns-3.31/wscript).  
```
opt.add_option('--cxx-standard',
               help=('Compile NS-3 with the given C++ standard'),
               type='string', default='-std=c++14', dest='cxx_standard')  
```
2 put the folder ex-webrtc under  ns3-allinone-3.xx/ns-3.xx/src.  
3 put the file webrtc-static.cc under ns3-allinone-3.xx/ns-3.xx/scratch.
4 The wscript in ex-webrtc give hint on the configuration on libwebrtc.  
```
module.env.append_value("CXXFLAGS", "-I/home/zsy/webrtc/src/")
module.env.append_value("LINKFLAGS", ["-L/home/zsy/webrtc/src/out/m84/obj/"])
```
5 let the compiler find the header in external library:  
```
cd ns3-allinone-3.xx/  
sudo su  
source /etc/profile  
./build.py  
  
```
6 test:
```
cd ns3-allinone-3.xx/ns-3.xx/  
sudo su  
source /etc/profile   
./waf --run "scratch/webrtc-static"  
```
Results:  
![avatar](https://github.com/SoonyangZhang/webrtc-gcc-ns3/blob/master/results/gcc-rate.png)  

Reference:  
[1] [build webrtc with gcc](https://mediasoup.org/documentation/v3/libmediasoupclient/installation/)
[2] [the blog in chines to configure this code on ns3]https://blog.csdn.net/u010643777/article/details/107237315  

