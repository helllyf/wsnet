/**
 * @simple traffic generator to test IEEE 802.15.4 protocols
*/

#ifndef DATACENTRIC_TEST_APP
#define DATACENTRIC_TEST_APP

#include "TrafGenPar.h"
#include "DataCentricAppPkt_m.h"
#include "DataCentricNetworkLayer.h"
//#include "Ieee802154UpperCtrlInfo_m.h"


// DataCentric 'C' associations
#include "RoutingAndAggregation.h"

class DataCentricTestApp : public TrafGenPar
{
  public:

    // LIFECYCLE
    // this takes care of constructors and destructors

    virtual void initialize(int);
    virtual void finish();
    double StartTime();

  protected:

    // OPERATIONS
    virtual void handleSelfMsg(cMessage*);
    virtual void handleLowerMsg(cMessage*);

    virtual void SendTraf(cPacket *msg, const char*);

    // sibling module IDs
    DataCentricNetworkLayer* netModule;

  private:
    bool    m_debug;        // debug switch
    std::vector<std::string> sourcesData;
    std::vector<std::string> sinksData;
    std::string contextData;
    int     mLowerLayerIn;
    int     mLowerLayerOut;

    int     mCurrentTrafficPattern;

    double  mNumTrafficMsgs;
    double  mNumTrafficMsgRcvd;
    double  mNumTrafficMsgNotDelivered;

    const char* m_moduleName;
    simtime_t   sumE2EDelay;
    double  numReceived;
    double  totalByteRecv;
    cOutVector e2eDelayVec;
    cOutVector meanE2EDelayVec;

    cMessage *mpStartMessage;


};

#endif
