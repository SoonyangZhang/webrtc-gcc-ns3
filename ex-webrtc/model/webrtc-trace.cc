#include "webrtc-trace.h"
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h> // stat
#include <memory.h>
namespace ns3{
namespace{
  std::string WebrtcTracePath;
  const int32_t kTraceOwdInterval=25;
}
//https://stackoverflow.com/questions/675039/how-can-i-create-directory-tree-in-c-linux
static bool IsDirExist(const std::string& path)
{
#if defined(_WIN32)
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else 
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}

bool MakePath(const std::string& path)
{
#if defined(_WIN32)
    int ret = _mkdir(path.c_str());
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
#endif
    if (ret == 0)
        return true;

    switch (errno)
    {
    case ENOENT:
        // parent didn't exist, try to create it
        {
            int pos = path.find_last_of('/');
            if (pos == std::string::npos)
#if defined(_WIN32)
                pos = path.find_last_of('\\');
            if (pos == std::string::npos)
#endif
                return false;
            if (!MakePath( path.substr(0, pos) ))
                return false;
        }
        // now, try to create again
#if defined(_WIN32)
        return 0 == _mkdir(path.c_str());
#else 
        return 0 == mkdir(path.c_str(), mode);
#endif
    case EEXIST:
        // done!
        return IsDirExist(path);

    default:
        return false;
    }
}

void config_webrtc_trace_path(){
    if (0 == WebrtcTracePath.size()) {
        char buf[FILENAME_MAX];
        memset(buf,0,FILENAME_MAX);
        std::string path=std::string (getcwd(buf, FILENAME_MAX))+
                "/traces/";
        WebrtcTracePath=path;
        MakePath(WebrtcTracePath);
    }
}
void set_webrtc_trace_folder(const std::string &path){
    if(0 == WebrtcTracePath.size()) {
        int len=path.size();
        if (len && '/' != path.back()) {
                WebrtcTracePath=path+'/';
        }else{
            WebrtcTracePath=path;
        }
        MakePath(WebrtcTracePath);
    }
}
WebrtcTrace::~WebrtcTrace(){
    Close();
}
void WebrtcTrace::Log(const std::string &prefix,WebrtcTraceType flag){
    config_webrtc_trace_path();
    if (flag&E_WEBRTC_OWD) {
        OpenTraceOwdFile(prefix);
    }
    if (flag&E_WEBRTC_BW) {
        OpenTraceBwFile(prefix);
    }
    if (flag&E_WEBRTC_LOSS) {
        OpenTraceLossFile(prefix);
    }
    m_flag=flag;
}
void WebrtcTrace::OnReceiptPktInfo(uint32_t now,uint32_t seq,uint32_t owd){
    if (m_flag&E_WEBRTC_OWD && m_owd.is_open()) {
        if (0 == m_lastOutTime || now-m_lastOutTime >= kTraceOwdInterval) {
            float time=float(now)/1000;
            m_owd<<time<<"\t"<<owd<<std::endl;
            m_lastOutTime=now;
        }
    }
    if (seq > m_maxSeq) {
        m_maxSeq=seq;
    }
    if (seq >= m_expectSeq){
        m_lossPkts+=(seq-m_expectSeq);
        m_expectSeq=seq+1;
    }
}
void WebrtcTrace::OnBW(uint32_t now,uint32_t bps){
    char line [256];
    memset(line,0,256);
    if(m_bw.is_open()){
        float time=float(now)/1000;
        float kbps=float(bps)/1000;
        m_bw<<time<<"\t"<<kbps<<std::endl;
    }    
}
void WebrtcTrace::OpenTraceOwdFile(const std::string &name){
    std::string path =WebrtcTracePath+name+"_owd.txt";
    m_owd.open(path.c_str(), std::fstream::out);
}
void WebrtcTrace::OpenTraceBwFile(const std::string &name){
    std::string path =WebrtcTracePath+name+"_bw.txt";
    m_bw.open(path.c_str(), std::fstream::out);
}
void WebrtcTrace::OpenTraceLossFile(const std::string &name){
    std::string path =WebrtcTracePath+name+"_loss.txt";
    m_loss.open(path.c_str(), std::fstream::out);
}
void WebrtcTrace::CloseTraceOwdFile(){
    if(m_owd.is_open()){
        m_owd.close();
    }
}
void WebrtcTrace::CloseTraceBwFile(){
    if(m_bw.is_open()){
        m_bw.close();
    }
}
void WebrtcTrace::CloseTraceLossFile(){
    if (0 != m_maxSeq && m_loss.is_open()) {
        float loss = 100.0*m_lossPkts/m_maxSeq;
        m_loss<<loss<<std::endl;
        m_loss.close();
    }
}
void WebrtcTrace::Close(){
    CloseTraceOwdFile();
    CloseTraceBwFile();
    CloseTraceLossFile();
}
UtilCalculator* UtilCalculator::m_instance = nullptr;
UtilCalculator* UtilCalculator::Instance(){
    if (nullptr== m_instance) {
        m_instance=new UtilCalculator();
    }
    return m_instance;
}

void UtilCalculator::OnPacketInfo(int64_t event_time,size_t sz){
    if(m_enable){
        m_readBytes+=sz;
        m_receiptMillis=event_time;
    }
}

int64_t UtilCalculator::GetLastReceiptMillis() const{
    return m_receiptMillis;
}

void UtilCalculator::CalculateUtil(const std::string &prefix,int64_t channel_bit){
    if (m_enable && channel_bit > 0 && m_readBytes > 0) {
        config_webrtc_trace_path();
        std::string pathname =WebrtcTracePath+prefix+"_util.txt";
        float util=1.0*m_readBytes*8*100/(channel_bit);
        std::fstream  f_util;
        f_util.open(pathname.c_str(), std::fstream::out);
        f_util<<util<<std::endl;
        f_util.close();
        m_enable=false;
    }
}

void UtilCalculator::Enable(){
    m_enable=true;
}

}
