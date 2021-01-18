#pragma once
#include <stdint.h>
#include "ns3/tag.h"
namespace ns3{
class WebrtcTag:public Tag{
public:
    WebrtcTag(){}
    WebrtcTag(uint64_t seq,uint64_t time):seq_(seq),time_(time){}
    static TypeId GetTypeId (void);
    virtual TypeId GetInstanceTypeId (void) const override;
    virtual uint32_t GetSerializedSize (void) const override;
    virtual void Serialize (TagBuffer i) const override;
    virtual void Deserialize (TagBuffer i) override;
    virtual void Print (std::ostream &os) const override {}
    uint64_t GetSequence() const {return seq_;}
    void Sequence(uint64_t seq) {seq_=seq;}
    uint64_t GetTime() const {return time_;}
    void Time(uint64_t now) {time_=now;}
private:
    void VarintEncode(TagBuffer &i,uint64_t value) const;
    void VarientDecode(TagBuffer &i,uint64_t *value);
    uint64_t seq_=0;
    uint64_t time_=0;
};
}
