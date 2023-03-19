#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <stdint.h>
namespace ns3{
typedef uint8_t WebrtcTraceType;
class WebrtcTrace{
public:
enum WebrtcTraceEnable:uint8_t{
    E_WEBRTC_OWD=0x01,
    E_WEBRTC_BW=0x02,
    E_WEBRTC_LOSS=0x04,
    E_WEBRTC_ALL=E_WEBRTC_OWD|E_WEBRTC_BW|E_WEBRTC_LOSS,
};
    WebrtcTrace(){};
    ~WebrtcTrace();
    void Log(const std::string &prefix,WebrtcTraceType flag);
    void OnBW(uint32_t now,uint32_t bps);
    void OnReceiptPktInfo(uint32_t now,uint32_t seq,uint32_t owd);
    WebrtcTraceType LogFlag() const {return m_flag;}
private:
    void Close();
    void OpenTraceOwdFile(const std::string &name);
    void OpenTraceBwFile(const std::string &name);
    void OpenTraceLossFile(const std::string &name);
    void CloseTraceOwdFile();
    void CloseTraceBwFile();
    void CloseTraceLossFile();
    WebrtcTraceType m_flag=0;
    std::fstream m_owd;
    std::fstream m_bw;
    std::fstream m_loss;
    int64_t m_lastOutTime=0;
    uint32_t m_lossPkts=0;
    uint32_t m_expectSeq=1;
    int64_t m_maxSeq=0;
};
void set_webrtc_trace_folder(const std::string &path);

class UtilCalculator{
public:
    static UtilCalculator *Instance();
    void OnPacketInfo(int64_t event_time,size_t sz);
    int64_t GetLastReceiptMillis() const;
    void CalculateUtil(const std::string &prefix,int64_t channel_bit);
    void Enable();
private:
    UtilCalculator(){}
    UtilCalculator(const UtilCalculator&)=delete;
    UtilCalculator& operator=(const UtilCalculator&)=delete;
    bool m_enable=false;
    int64_t m_readBytes=0;
    int64_t m_receiptMillis=0;
    static UtilCalculator* m_instance;
};
}
