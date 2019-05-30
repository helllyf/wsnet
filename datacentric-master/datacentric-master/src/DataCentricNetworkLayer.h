/**
 * @short Implementation of a simple packets forward function for IEEE 802.15.4 star network
 *  support device <-> PAN coordinator <-> device transmission
    MAC address translation will be done in MAC layer (refer to Ieee802154Mac::handleUpperMsg())
 * @author Feng Chen
*/

#ifndef DataCentricNetworkLayer_H
#define DataCentricNetworkLayer_H

#include <omnetpp.h>

#include "DataCentricTestApp/DataCentricAppPkt_D.h"
#include "Ieee802154NetworkCtrlInfo_m.h"
#include "RoutingAndAggregation.h"

#include "Ieee802154Phy.h"

#define IMAGEPATH "C:/omnetpp-4.2.1/images/"



class DataCentricNetworkLayer : public cSimpleModule
{
  public:
    virtual int    numInitStages    () const { return 2; }
    virtual void initialize(int);
    virtual void finish();
    void WriteModuleListFile();

    double StartTime();

    // DataCentric 'C' associations
    RoutingData moduleRD;
    static bool justStartedInitialising;
    cOutVector controlPackets;

    // sibling module IDs
    //cModule* nicModule;
    //cModule* macModule;
    //cModule* mPhyModule;
    Ieee802154Phy* mPhyModule;

    // accessed from C function call back
    int             mLowerLayerOut;
    double          mRoutingDelay;

  protected:
    // Message handle functions
    void                handleMessage           (cMessage*);

    // debugging enabled for this node? Used in the definition of EV
    bool                m_debug;
    bool                isPANCoor;
    int                 nodeConstraintValue;
    std::string         mTheAddressString;
    uint64              mAddress;
    const char*     m_moduleName;

    // module gate ID
    int             mUpperLayerIn;
    int             mUpperLayerOut;
    int             mLowerLayerIn;

    // for statistical data
    double          numForward;

    cMessage *mpStartMessage;


};
#endif

