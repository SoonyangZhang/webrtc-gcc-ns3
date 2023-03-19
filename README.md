# webrtc-gcc-ns3
test google congestion control algorithm on ns3.31
The ex-webrtc module depends on libwebrtc.a  
## Preparation 
There are three ways to get libwebtc.a  
### The first approach  
Download offcial webrtc. This approch is not suggested. ex-webrtc is referred the code under /webrtc/test/scenario. It is small discrete event simulator and only suports to build two nodes topology (webrtc/test/scenario/network_node.cc). I copy some source files in test and api under ex-webrtc. Once these files are changed by webrtc, building error will encountered.    
1 download webrtc.  
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
6  Built webrtc  with clang. [Get clang installed first](https://www.jianshu.com/p/3c7eae5c0c68).   
- first step:    

```
cd webrtc/src  
gn gen out/m84 --args='is_debug=false is_component_build=false is_clang=true rtc_include_tests=false rtc_use_h264=true rtc_enable_protobuf=false use_rtti=true use_custom_libcxx=false treat_warnings_as_errors=false use_ozone=true'   
```
- second step:  

```
cd webrtc/src  
ninja -C out/m84  
```
### The second approach  
Download the webrtc I upload. This approach to build libwertc.a is suggested for these intending to make some change in webrtc source code.    
url: https://pan.baidu.com/s/18F26BAmZhj_CAQzKNUeDSA  
auth code: j4ts  
After download the code, decompress it:  
```
cat webrtc.tar.gz* |tar zx  
```
Built webrtc  with clang (refer to step 6 above).  
### The third approach
I upload webrtc header and libwebrtc.a on github for fast prototype.  
No bother is needed to build webrtc.  
download it here: https://github.com/SoonyangZhang/webrtc-header-lib  
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
2 Change the warning flags in ns3 (ns-allinone-3.31/ns-3.31/waf-tools/cflags.h).  
```
self.warnings_flags = [['-Wall'], ['-Wno-unused-parameter'], ['-Wextra']]
```
The origin content can be seen [here](https://github.com/nsnam/ns-3-dev-git/blob/ns-3.31/waf-tools/cflags.py#L22).  
4 Put the folder ex-webrtc under ns-allinone-3.31/ns-3.31/src.  
5 Build ns3 with clang++  
```
cd ns-allinone-3.31/ns-3.31  
source /etc/profile  
CXX="clang++"  
CXXFLAGS="-std=c++14" 
./waf configure  
./waf build  
```
## Run example:
1 Put the file webrtc-static.cc under ns-allinone-3.31/ns-3.31/scratch/.  
2 Rebuild ns3  
```
cd ns-allinone-3.31/ns-3.31  
source /etc/profile  
CXX="clang++"  
./waf configure  
./waf build  

```
3 Create a folder named traces under ns-allinone-3.31/ns-3.31/. The traced data can be found there.  
4 Run the example in simulation mode (ns3 event time):
```
./waf --run "scratch/webrtc-static --m=simu --it=1"  
```
5 Run the example in emulation mode (real clock):   
```
 ./waf --run "scratch/webrtc-static --m=emu --it=1"  
```
The code here is used by [gym](https://github.com/OpenNetLab/gym) to build reinforce learning based congestion controller.  
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
