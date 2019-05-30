/**
 * @short Implementation of a simple packets forward function for IEEE 802.15.4 star network
 *  support device <-> PAN coordinator <-> device transmission
    MAC address translation will be done in MAC layer (refer to Ieee802154Mac::handleUpperMsg())
 * @author Feng Chen
*/

#include <stdio.h>
#include "DataCentricNetworkLayer.h"
#include "Ieee802Ctrl_m.h"

//#include "InterfaceTableAccess.h"
//#include "MACAddress.h"
//#include "Ieee802Ctrl_m.h"
//#include "Ieee802154Phy.h"
//#include "csma802154.h"



//#undef EV
//#define EV (ev.isDisabled()||!m_debug) ? std::cout : ev ==> EV is now part of <omnetpp.h>

Define_Module( DataCentricNetworkLayer );

/////////////////////////////// PUBLIC ///////////////////////////////////////

// Framework values mostly extern
static unsigned char forwardingRole[14] = {14, 4, 4, 8, 2, 3, 0, 0, 0, 0, 9, 4, 6, 0};
int nodeConstraint;
NEIGHBOUR_ADDR thisAddress;

static std::ofstream myfile;


bool DataCentricNetworkLayer::justStartedInitialising = true;
static int currentModuleId;

static char messageName[9][24] =
{
"ADVERT                 ",
"INTEREST               ",
"REINFORCE              ",
"DATA                   ",
"NEIGHBOR_BCAST         ",
"NEIGHBOR_UCAST         ",
"REINFORCE_INTEREST     ",
"COLLABORATION          ",
"REINFORCE_COLLABORATION"
};




static void cb_send_message(NEIGHBOUR_ADDR _interface, unsigned char* _msg);
static void cb_bcast_message(unsigned char* _msg);
static void cb_handle_application_data(unsigned char* _msg);
static void write_one_connection(State* s, unsigned char* _data, NEIGHBOUR_ADDR _if);


//============================= LIFECYCLE ===================================
/**
 * Initialization routine
 */
void DataCentricNetworkLayer::initialize(int aStage)
{
    cSimpleModule::initialize(aStage); //DO NOT DELETE!!
    if (0 == aStage)
    {
        mpStartMessage = new cMessage("StartMessage");
        // WirelessMacBase stuff...
        mUpperLayerIn  = findGate("upperLayerIn");
        mUpperLayerOut = findGate("upperLayerOut");
        mLowerLayerIn  = findGate("lowerLayerIn");
        mLowerLayerOut = findGate("lowerLayerOut");
        controlPackets.setName("ControlPackets");

        m_moduleName    = getParentModule()->getFullName();

        m_debug                     = par("debug");
        isPANCoor                   = par("isPANCoor");
        numForward      = 0;

        // ORIGINAL DATA CENTRIC STUFF
        moduleRD.grTree = NULL;
        moduleRD.interfaceTree = NULL;
        moduleRD.stateTree = NULL;
        moduleRD.kdRoot = NULL;
        moduleRD.role[0] = NULL;
        moduleRD.role[1] = NULL;
        moduleRD.role[2] = NULL;
        moduleRD.role[3] = NULL;
        moduleRD.top_context = trie_new();
        moduleRD.top_state = trie_new();
        rd = &(moduleRD);
        rd->role[0] = (unsigned char*)malloc(forwardingRole[0]);
        memcpy(rd->role[0], forwardingRole, forwardingRole[0]);
        nodeConstraintValue         = par("nodeConstraint");
        mRoutingDelay         = par("routingDelay");
        setMessageCallBack(cb_send_message);
        setBroadcastCallBack(cb_bcast_message);
        setApplicationCallBack(cb_handle_application_data);

        cMessage* rcMessage = new cMessage("RegularCheck");
        scheduleAt(simTime()+2.0, rcMessage);
        scheduleAt(simTime() + StartTime(), mpStartMessage);



    }

    if (1 == aStage)
    {
        cModule* nicModule = this->getParentModule()->getSubmodule("nic");
        cModule* macModule = check_and_cast<cModule*>(nicModule->getSubmodule("mac"));
        mPhyModule = check_and_cast<Ieee802154Phy*>(nicModule->getSubmodule("phy"));
        string tempAddressString = macModule->par("address");
        MACAddress addrObj(tempAddressString.c_str());
        //MACAddress addrObj(mTheAddressString.c_str());
        //mTheAddressString = tempAddressString;
        mTheAddressString = addrObj.str();
        mAddress = addrObj.getInt();
        //mAddress &= 0x0000FFFFFFFFFFFF;


        WriteModuleListFile();
    }


}

void DataCentricNetworkLayer::finish()
{
    recordScalar("num of pkts forwarded", numForward);
}

/////////////////////////////// Msg handler ///////////////////////////////////////
void DataCentricNetworkLayer::handleMessage(cMessage* msg)
{
    // only necessary in this simulation due to combined C++ & C
    nodeConstraint = nodeConstraintValue;
    currentModuleId = this->getId();
    thisAddress = mAddress;


    rd = &(moduleRD);
    ////////////////////////////////////////////

    std::string fName = this->getParentModule()->getFullName();

    if (msg == mpStartMessage)
    {
        StartUp();
        return;

    }


    if ( msg->isSelfMessage() )
    {
        regular_checks();

        string s;
        ostringstream ss;
        ss.clear();
        ss.str(s);
        ss << ".\\" << hex << uppercase << thisAddress << "Connections.txt";

        int remove_failure = std::remove(ss.str().c_str());
        if ( remove_failure )
        {
            EV << "File removal failure " << remove_failure << " \n";
        }
        myfile.open (ss.str().c_str(), std::ios::app);

        // need to improve this
        write_connections(write_one_connection);
        myfile.close();

#ifdef ANDREW_DEBUG
        // perhaps new public method not best way to get noise
        // maybe regular write back to a param, then here readthat param
        EV << "[SNR]" << this->getParentModule()->getFullName()
            << ": Current Phy.noiseLevel: " << mPhyModule->getNoiseLevel();
#endif

        scheduleAt(simTime()+2.0, msg);
        return;
    }

    DataCentricAppPkt* appPkt = check_and_cast<DataCentricAppPkt *>(msg);


    // coming from App layer
    if (msg->getArrivalGateId() == mUpperLayerIn)
    {

        //incoming_packet.message_type = DATA;
        //incoming_packet.length = strlen((char*)_data);
        //incoming_packet.data = _data;
        //incoming_packet.path_value = 0;
        //handle_data(SELF_INTERFACE);




        //int size = sizeof(outgoing_packet.message_type) + sizeof(outgoing_packet.length)
        //        + outgoing_packet.length +   sizeof(outgoing_packet.path_value);

        int datalen = appPkt->getPktData().size();
        unsigned char* pkt = (unsigned char*)malloc(datalen+4);
        appPkt->getPktData().size();

        pkt[0] = DATA;
        pkt[1] = datalen;
        //memcpy(&(pkt[2]), data, datalen);

        std::copy(appPkt->getPktData().begin(), appPkt->getPktData().end(), &(pkt[2]));


        //static MACAddress broadcastAddr("FF:FF:FF:FF:FF:FF");
        // NEW
        //Ieee802Ctrl *controlInfo = new Ieee802Ctrl();
        //controlInfo->s
        //controlInfo->setDest(broadcastAddr);
        //controlInfo->setEtherType(ETHERTYPE_IPv4);
        //msg->setControlInfo(controlInfo);
        //send(appPkt, mLowerLayerOut);
        //Ieee802154NetworkCtrlInfo *control_info = new Ieee802154NetworkCtrlInfo();
        //control_info->
        // next line temp - to be deleted
        //appPkt->getPktData().insert(appPkt->getPktData().end(), _msg, _msg+(_msg[1] + 4))


        //handle_message((unsigned char *)x, SELF_INTERFACE);
        handle_message(pkt, SELF_INTERFACE);
        free(pkt);


    }
    // coming from MAC layer
    else if (msg->getArrivalGateId() == mLowerLayerIn)
    {
        //RSSI
        int temp = 0;
        Ieee802Ctrl *incomingControlInfo = check_and_cast<Ieee802Ctrl*>(appPkt->getControlInfo());
        //incomingControlInfo-
        //string prevAddressString = incomingControlInfo->getSrc().str();
        uint64 previousAddress = incomingControlInfo->getSrc().getInt();
        //previousAddress &= 0x0000FFFFFFFFFFFF;
        unsigned char* pkt = (unsigned char*)malloc(appPkt->getPktData().size());
        std::copy(appPkt->getPktData().begin(), appPkt->getPktData().end(), pkt);

        if ( this->getParentModule()->getIndex() == 15 )
        {
            temp = 1;

        }

        handle_message(pkt, previousAddress);
        free(pkt);
    }
    else
    {
        // not defined
    }
}


double DataCentricNetworkLayer::StartTime()
{
    return par("startTime").doubleValue();
}


void DataCentricNetworkLayer::WriteModuleListFile()
{
    if ( justStartedInitialising )
    {
        std::remove(".\\ModuleList.txt");
        justStartedInitialising = false;
    }
    std::ofstream myfile;
    myfile.open (".\\ModuleList.txt", std::ios::app);
    myfile << "NODE" << std::endl;
    //std::string fp = getFullPath();
    //std::string fp = this->mAddress;
    myfile << hex << uppercase << this->mAddress << std::endl;
    cModule* pm = this->getParentModule();

    char image[100];
    char* image_i = image;
    std::sprintf(image_i, "%s", pm->getDisplayString().getTagArg("i", 0));
    myfile << "IMAGE" << std::endl;
    myfile << IMAGEPATH << image_i << ".png" << std::endl;

    char x[10];
    char y[10];
    char* x_i = x;
    char* y_i = y;
    std::sprintf(x_i, "%s", pm->getDisplayString().getTagArg("p", 0));
    std::sprintf(y_i, "%s", pm->getDisplayString().getTagArg("p", 1));
    myfile << "POSITION" << std::endl;
    myfile << x_i << std::endl;
    myfile << y_i << std::endl;

    //int n = gateSize("gate");
    //for ( int i = 0; i < n; i++ )
    //{
    //    cGate* g = gate("gate$o",i);
    //    cGate*  ng = g->getNextGate();
    //    cModule* connectedMod = ng->getOwnerModule();
    //    std::string connectedModFullPath = connectedMod->getFullPath();
    //    myfile << "CONNECTION" << std::endl;
    //    myfile << fp << std::endl;
    //    myfile << connectedModFullPath << std::endl;
    //}

    myfile.close();

}



static void write_one_connection(State* s, unsigned char* _data, NEIGHBOUR_ADDR _if)
{

    myfile << "CONNECTION" << std::endl;

    for ( int i = 0; _data[i] != 0; i++ )
    {
        myfile << hex << uppercase << _data[i];
    }
    myfile << std::endl;

    myfile << hex << uppercase << thisAddress << std::endl;
    myfile << "best_deliver_to" << std::endl;
    if (s->bestGradientToDeliver)
    {
        myfile << hex << uppercase << s->bestGradientToDeliver->key2->iName << std::endl;
        myfile << s->bestGradientToDeliver->costToDeliver << std::endl;
    }
    else
    {
        myfile << "no_best_deliver_to" << std::endl;
        myfile << "not applicable" << std::endl;
    }

    myfile << "best_obtain_from" << std::endl;
    if (s->bestGradientToObtain)
    {
        myfile << hex << uppercase << s->bestGradientToObtain->key2->iName << std::endl;
        myfile << s->bestGradientToObtain->costToObtain << std::endl;
    }
    else
    {
        myfile << "no_best_obtain_from" << std::endl;
        myfile << "not applicable" << std::endl;
    }

}


static void cb_send_message(NEIGHBOUR_ADDR _interface, unsigned char* _msg)
{
    /*
     * This is a generic call back for the data centric routing framework
     * It's implementation in this case is specifically tailored to Omnet++
     */
    DataCentricNetworkLayer* currentModule = check_and_cast<DataCentricNetworkLayer *>(cSimulation::getActiveSimulation()->getModule(currentModuleId));

    //char msgname[20];
    //sprintf(msgname, "%s", messageName[*_msg]);
    //RoutingMessage *msg = new RoutingMessage(msgname);
    //msg->setKind(ROUTING_MESSAGE);
    //unsigned int messageSize = _msg[1] + 4;
    //for( unsigned i = 0; i < messageSize; i++ )
    //{
    //    msg->setData(i, _msg[i]);
    //}


    DataCentricAppPkt* appPkt = new DataCentricAppPkt("DataCentricAppPkt");
    //appPkt->setDataName("");

    appPkt->getPktData().insert(appPkt->getPktData().end(), _msg, _msg+(_msg[1] + 4));

    //appPkt->setSendingMAC(currentModule->mAddressString); // awaiting msg compilation
    appPkt->setCreationTime(simTime());
    Ieee802Ctrl *controlInfo = new Ieee802Ctrl();
    MACAddress destAddr(_interface);
    controlInfo->setDest(destAddr);
    controlInfo->setEtherType(ETHERTYPE_IPv4);
    appPkt->setControlInfo(controlInfo);

    //appPkt->setKind(8+_msg[0]);
    currentModule->mRoutingDelay = currentModule->par("routingDelay");
    //currentModule->send(appPkt, currentModule->mLowerLayerOut);
    ev << "UCAST   to " << currentModule->getParentModule()->getFullName() << endl;
    currentModule->sendDelayed(appPkt, currentModule->mRoutingDelay, currentModule->mLowerLayerOut);
}

static void cb_bcast_message(unsigned char* _msg)
{
    /*
     * This is a generic call back for the data centric routing framework
     * It's implementation in this case is specifically tailored to Omnet++
     */
//check_and_cast<Txc20 *>
    //static cSimulation* cSimulation::getActiveSimulation()->getModule(currentModuleId)

    //cSimpleModule* currentModule = check_and_cast<cSimpleModule *>(cSimulation::getActiveSimulation()->getModule(currentModuleId));

    DataCentricNetworkLayer* currentModule = check_and_cast<DataCentricNetworkLayer *>(cSimulation::getActiveSimulation()->getModule(currentModuleId));



    //int n = currentModule->gateSize("gate");
    //char msgname[20];
    //sprintf(msgname, "%s", messageName[*_msg]);

    //for ( int i = 0; i < n; i++ )
    //{
    //    RoutingMessage *msg = new RoutingMessage(msgname);
    //    msg->setKind(ROUTING_MESSAGE);
    //    for( unsigned i = 0; i < MESSAGE_SIZE; i++ )
    //    {
    //        msg->setData(i, _msg[i]);
    //    }
    //    EV << "Send to gate " << i << " \n";
    //    currentModule->send(msg, "gate$o",i);
    //}

    //controlPackets
    static double numControlPackets = 0;
    numControlPackets++;
    currentModule->controlPackets.record(numControlPackets);

    DataCentricAppPkt* appPkt = new DataCentricAppPkt("DataCentricAppPkt");
    appPkt->getPktData().insert(appPkt->getPktData().end(), _msg, _msg+(_msg[1] + 4));
    appPkt->setCreationTime(simTime());

    Ieee802Ctrl *controlInfo = new Ieee802Ctrl();
    static MACAddress broadcastAddr("FF:FF:FF:FF:FF:FF");
    controlInfo->setDest(broadcastAddr);
    controlInfo->setEtherType(ETHERTYPE_IPv4);
    appPkt->setControlInfo(controlInfo);

    //appPkt->setKind(8+_msg[0]);

    //appPkt->
    //appPkt->setd
    //        currentModule->getd

    currentModule->mRoutingDelay = currentModule->par("routingDelay");

    //currentModule->send(appPkt, currentModule->mLowerLayerOut);
    ev << "BRDCAST to " << currentModule->getParentModule()->getFullName() << endl;
    currentModule->sendDelayed(appPkt, currentModule->mRoutingDelay, currentModule->mLowerLayerOut);
}


static void cb_handle_application_data(unsigned char* _msg)
{
    // work still to do here
    // packetbuf_copyto(_msg, MESSAGE_SIZE);
    cSimpleModule* currentModule = check_and_cast<cSimpleModule *>(cSimulation::getActiveSimulation()->getModule(currentModuleId));

    char bubbleText[40];
    char* bubbleTextPtr = bubbleText;
    while (*_msg)
    {
        int numChar = std::sprintf(bubbleTextPtr, "%d-", (unsigned int)*_msg);
        bubbleTextPtr += numChar;
        _msg++;
    }
    currentModule->getParentModule()->bubble(bubbleText);
    //currentModule->bubble(bubbleText);
    //currentModule->bubble("Data received");



}




/*

void DataCentricNetworkLayer::handleMessage(cMessage* msg)
{
    // only necessary in this simulation due to combined C++ & C
    nodeConstraint = nodeConstraintValue;
    currentModuleId = this->getId();
    rd = &(moduleRD);
    ////////////////////////////////////////////

    DataCentricAppPkt* appPkt = check_and_cast<DataCentricAppPkt *>(msg);
    //appPkt->getDataName();
    //appPkt->getNextHopMAC();


    //InterfaceEntry *ie = ift->getInterfaceById(controlInfo->getInterfaceId());

    //uint64 getInt() const { return address; }



    // coming from App layer
    if (msg->getArrivalGateId() == mUpperLayerIn)
    {
        static MACAddress broadcastAddr("FF:FF:FF:FF:FF:FF");

        // NEW
        Ieee802Ctrl *controlInfo = new Ieee802Ctrl();
        controlInfo->s
        controlInfo->setDest(broadcastAddr);
        controlInfo->setEtherType(ETHERTYPE_IPv4);
        msg->setControlInfo(controlInfo);

        // send out
        //send(msg, nicOutBaseGateId + ie->getNetworkLayerGateIndex());
        //send(msg, nicOutBaseGateId + ie->getNetworkLayerGateIndex());
        send(appPkt, mLowerLayerOut);
        ///////////////////////////////////////////

        Ieee802154NetworkCtrlInfo *control_info = new Ieee802154NetworkCtrlInfo();
        control_info->

        //handle_message((unsigned char *)appPkt->getDataName(), previousAddress);
        handle_message((unsigned char *)appPkt->getDataName(), SELF_INTERFACE);



        Ieee802154NetworkCtrlInfo *control_info = new Ieee802154NetworkCtrlInfo();

        if (isPANCoor)      // I'm PAN coordinator, msg is destined for a device
        {
            control_info->setToParent(false);
            control_info->setDestName(appPkt->getDestName());
        }
        else        // should always be sent to PAN coordinator first
        {
            control_info->setToParent(true);
            control_info->setDestName("PAN Coordinator");
        }

        appPkt->setControlInfo(control_info);
        send(appPkt, mLowerLayerOut);

    }

    // coming from MAC layer
    else if (msg->getArrivalGateId() == mLowerLayerIn)
    {
        Ieee802Ctrl *incomingControlInfo = appPkt->getControlInfo();
        uint64 previousAddress = incomingControlInfo->getSrc().getInt();

        handle_message((unsigned char *)appPkt->getDataName(), previousAddress);
    }
    else
    {
        // not defined
    }
}

 */
