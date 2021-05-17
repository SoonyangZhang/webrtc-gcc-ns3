# webrtc-gcc-ns3
test google congestion control algorithm on ns3.31  
## Preparation 
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
3 Add two files to rtc_base library(BUILD.gn):  
```
rtc_library("rtc_base") {
  sources = [
    ...
    "memory_stream.cc",
    "memory_stream.h",
    "memory_usage.cc",
    "memory_usage.h",
    ....
    ]
}
```
Remove them out of the original library(rtc_library("rtc_base_tests_utils")).  
The build flag rtc_include_tests is diabled, or else there will be errors when compiling.  
4 Delete code in webrtc:  
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
6  webrtc is built with clang. [Get clang installed first](https://www.jianshu.com/p/3c7eae5c0c68).   
## Build webrtc  
1 first step:    
```
gn gen out/m84 --args='is_debug=false is_component_build=false is_clang=true rtc_include_tests=false rtc_use_h264=true rtc_enable_protobuf=false use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false use_ozone=true'   
```
2 second step:  
```
ninja -C out/m84  
```
## Build ns3.31
1 Add environment variable   
```
sudo gedit source /etc/profile   
export WEBRTC_SRC_DIR=/xx/xx/xx/webrtc/src   
```
WEBRTC_SRC_DIR is the path where you put webrtc source code.  
The reason for this can be found in ex-webrtc/wscript.. 
```
webrtc_code_path = os.environ['WEBRTC_SRC_DIR']  
webrtc_lib_path = os.path.join(webrtc_code_path, 'out', 'm84', 'obj')  
webrtc_absl_path = os.path.join(webrtc_code_path, 'third_party', 'abseil-cpp')  
```
2 Enable c++14 build flag (ns-allinone-3.31/ns-3.31/wscript).  
```
opt.add_option('--cxx-standard',
               help=('Compile NS-3 with the given C++ standard'),
               type='string', default='-std=c++14', dest='cxx_standard')  
```
3 Change the warning flags in ns3 (ns-allinone-3.31/ns-3.31/waf-tools/cflags.h).  
```
self.warnings_flags = [['-Wall'], ['-Wno-unused-parameter'], ['-Wextra']]
```
The origin content can be seen [here](https://github.com/nsnam/ns-3-dev-git/blob/ns-3.31/waf-tools/cflags.py#L22).  
4 Put the folder ex-webrtc under ns-allinone-3.31/ns-3.31/src.  
5 Build ns3 with clang++  
```
cd ns-allinone-3.31/ns-3.31  
source /etc/profile  
CXX="clang++" ./waf configure  
./waf build  
```
## Run example:
1 Put the file webrtc-static.cc under ns-allinone-3.31/ns-3.31/scratch/.  
2 Rebuild ns3  
```
cd ns-allinone-3.31/ns-3.31  
source /etc/profile  
CXX="clang++" ./waf configure  
./waf build  

```
3 Create a folder named traces under ns-allinone-3.31/ns-3.31/. The traced data can be found there.  
4 Run the example in simulation mode (ns3 event time):
```
./waf run "scratch/webrtc-static --m=simu"  
```
5 The code can work in simulation or emulation mode. In emulaiton mode (real clock):  
```
./waf run "scratch/webrtc-static --m=emu"  
```
The code here is used by [gym](https://github.com/OpenNetLab/gym) to build reinforce learning based congestion controller.  
## extra help
If you cannot get the code sucessfully running, let me know and I will upload webrtc source code.    
## Results:  
In simulation mode:  
![avatar](https://github.com/SoonyangZhang/webrtc-gcc-ns3/blob/main/results/gcc-simu-bw.png)  
In Emulation mode:  
![avatar](https://github.com/SoonyangZhang/webrtc-gcc-ns3/blob/main/results/gcc-emu-bw.png)  
The difference is clear.  
## Reference:  
[1] [build webrtc with gcc](https://mediasoup.org/documentation/v3/libmediasoupclient/installation/)   
[2] [the blog in chinese to configure this code on ns3](https://blog.csdn.net/u010643777/article/details/107237315)   
[3] [gym-Build reinforce learning congestion control controller for webrtc](https://github.com/OpenNetLab/gym)
