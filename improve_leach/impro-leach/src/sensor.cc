//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "sensor.h"
#define DBL_MAX 1.7976931348623158e+308 /* max value */
Define_Module(Sensor);

void Sensor::initialize()
{
    // init parameters
    P = getParentModule()->par("P");
    id = this->getIndex(); // return the index of current module
    N = getParentModule()->par("Nnodes");

    double edge = getParentModule()->par("edge");
    range = sqrt(2*pow(edge,2));

    bitrate = par("bitrate");

    Eelec = this->par("Eelec");
    Eamp = this->par("Eamp");
    Ecomp = this->par("Ecomp");
    gamma = this->par("gamma");

    energy = this->par("energy");
    WATCH(energy);

    BS = getParentModule()->getSubmodule("baseStation");

    // setup internal events
    startRound_e = new cMessage("start-round", START_ROUND);
    rcvdADV_e = new cMessage("received-ADV", RCVD_ADV);
    rcvdSCHED_e = new cMessage("check-SCHED", RCVD_SCHED);
    rcvdJoin_e = new cMessage("check-JOIN-or-DATA", RCVD_JOIN);
    rcvdData_e = new cMessage("received-DATA", RCVD_DATA);
    startTX_e = new cMessage("startTX", START_TX);


    // Setup position 防止位置重复
    bool noRepeatPos = true;
    do{
        noRepeatPos = true;
        x = intuniform(getParentModule()->par("minX"), (int)edge);
        y = intuniform(getParentModule()->par("minY"), (int)edge);
        // check that no other nodes has the same coordinates
        for(unsigned int n = 0; n < N; n++){
           cModule * mod = retrieveNode(n);
           unsigned int modx = mod->par("posX");
           unsigned int mody = mod->par("posY");
           if((modx == x) && (mody == y)){
               noRepeatPos = false;
           }
        }
    }while((!noRepeatPos));
    // 更新参数
    this->par("posX") = x;
    this->par("posY") = y;

    energySignal = registerSignal("energy");

    scheduleAt(0,startRound_e);
}

void Sensor::finish()
{
    cancelAndDelete(startRound_e);
    cancelAndDelete(rcvdADV_e);
    cancelAndDelete(rcvdJoin_e);
    cancelAndDelete(rcvdData_e);
    cancelAndDelete(startTX_e);
}

void Sensor::reset()
{
    getDisplayString().setTagArg("i", 0, "old/ball"); // UI feedback
    role = SENSOR;
    msgBuf.clear();
    CH_id = -1;         // Cluster-Head id
    clusterN = 0;  // used by CH to keep track of the num. of nodes in the cluster
    cancelEvent(rcvdADV_e);
    cancelEvent(rcvdJoin_e);
    cancelEvent(rcvdData_e);
    cancelEvent(startTX_e);

}

void Sensor::handleMessage(cMessage *msg)
{
    if(role != DEAD) // if the node is still alive, react to messages, otherwise just drop them
    {
        switch(msg->getKind())
        {
            case START_ROUND:
                // start a new round in LEACH
                selfElection(); // new election
                // schedule the next round after roundTime
                scheduleAt(simTime()+roundTime,startRound_e);
                break;

            /******** Non-CH cases *********/
            case ADV_M:
                if(role == SENSOR) // 非CH才接受
                    msgBuf.push_back(msg); // insert ADV into the message buffer
                break;

            case RCVD_ADV:
                // wake up after timeout to check received ADVs
                chooseCH();
                break;

            case SCHED_M:
                // setup data send interval
                setupDataTX((mSchedule *) msg);
                break;

            case START_TX:
                sendData();
                break;

            /******** CH cases *********/
            case JOIN_M:
                msgBuf.push_back(msg); // insert JOIN into the message buffer
                break;

            case RCVD_JOIN:
                // wake up after timeout to check received ADVs
                if(msgBuf.size() > 0)
                    createTXSched();
                else{

                    // if no JOIN/DATA has been received (i.e. no one joined or all nodes in the cluster died)
                    // just act as a normal node (i.e. Orphan)
                    reset();
                    //scheduleAt(simTime(), startTX_e);
                    initOrphan();
                }
                break;

            case DATA_M:
                handleData(msg);
                break;

            case RCVD_DATA:
                compressAndSendToBS();
                break;

            /********** alternative CH **************/

            case CENTER_M:
                //setup CH environment variables
                alreadyCH = true;   // node excludes itself from next election
                role = CH;
                clusterN = ((mCenterCH *) msg)->getClusterN();
                getDisplayString().setTagArg("i", 0, "old/ball2"); // UI feedback
                // setup a timer to keep radio in IDLE mode and receive all data (TDMA)
                // Timeout will take in account the propagation delay for SCHED msg to reach destination and to receive back all data sequentially
                scheduleAt(simTime() + (((mCenterCH *) msg)->getSCHEDDelay()) + (((mCenterCH *) msg)->getIDLETime()) + EPSILON, rcvdData_e);
                #ifdef ACCOUNT_CH_SETUP
                // account for energy during IDLE time
                EnergyMgmt(RX, 0, clusterN*DATA_M_SIZE);
                #endif
                break;



        }
    }
    else
    {
        // node is dead
        if(!msg->isSelfMessage()){
            // drop all the msg from other modules
            cancelAndDelete(msg);
        }
    }

}

/******************* SENSOR functions **********************/
double Sensor::T(unsigned int n)    // T(n) threshold function
{
    int r = par("round"); // get current round

    if(!alreadyCH)
        return P/(1-P*(r % 1/P));
    else
        return 0;
}

void Sensor::selfElection()
{

    int r = par("round"); // NOTE: par("round") starts at -1
    par("round") = r+1;
    if ((r+1) == 0) roundTime = getParentModule()->par("roundTime");
    if(r+1 > 0) reset(); //reset all the structures before starting new round

    r = par("round");
    if((r % 1/P) == 0) alreadyCH = false; // reset current node status

    //compute Threshold function
    double th = T(id);
    double chance = uniform(0,1);
    if (chance < th)
    {
        // self-elected as Cluster-Head (CH)
        //proceed to Advertisement Phase
        EV << "I am Cluster-Head!\n";
        advertisementPhase();
    }
    else
    {
        // not CH.
        // start waiting for ADVs (consider max distance for timeout)
        scheduleAt(simTime() + propagationDelay(ADV_M_SIZE, MAX_DIST(range))+EPSILON, rcvdADV_e);
#ifdef ACCOUNT_CH_SETUP  //选择CH在仿真中是否消耗能量
        // add ENERGY CONSUMPTION FOR THE AMOUNT OF TIME WE ARE IN IDLE STATE
        EnergyMgmt(RX, 0, ADV_M_SIZE);
#endif
    }
}

void Sensor::chooseCH()
{
    CH_dist = std::numeric_limits<double>::infinity();//返回编译器允许的double类型最大值
    CH_id = -1;

    for(unsigned int i = 0; i < msgBuf.size(); i++){
        mAdvertisement *ADV = (mAdvertisement *) msgBuf.at(i);
        // check distance of sender
        // use euclidean distance to simulate RSSI
        double dist = distance(ADV->getId());
        if(dist < CH_dist){
            CH_dist = dist;
            CH_id = ADV->getId(); // select CH based on distance/RSSI
        }
        EV << "ADV received from " << ADV->getId() << " distance is " << dist << "\n";
        cancelAndDelete(ADV);
    }

    if(CH_id > -1){
        // CH has been chosen
        EV << "CH designed is " << CH_id << "\n";
        msgBuf.clear(); // empty the msg buffer

        double delay = propagationDelay(JOIN_M_SIZE, CH_dist);
        // notify CH
        mJoin *JOIN = new mJoin("join-cluster", JOIN_M);
        JOIN->setId(id);
        cModule *CH = retrieveNode(CH_id);
        sendDirect(JOIN, delay, 0, CH->gate("in"));
#ifdef ACCOUNT_CH_SETUP
        // account for energy transmission based on distance
        EnergyMgmt(TX, CH_dist, JOIN_M_SIZE);
#endif

    } else {

        EV << "[ORPHAN NODE] No ADV has been received. \n";

        initOrphan();//初始化孤儿节点
        //scheduleAt(simTime(), startTX_e);
    }
}

void Sensor::initOrphan()
{
    // set BS as CH
    CH_id = BS_ID;
#ifdef USE_BS_DIST
    CH_dist = BS_DIST(x,y);
#else
    CH_dist = MAX_DIST(range);
#endif
    // notify the BS that we are going to join it's cluster
    mJoin *JOIN = new mJoin("join-cluster", JOIN_M);
    double delay = propagationDelay(JOIN_M_SIZE, CH_dist);
    JOIN->setId(id);
    sendDirect(JOIN, delay, 0, BS->gate("in"));
#ifdef ACCOUNT_CH_SETUP
    // account for energy transmission based on distance
    EnergyMgmt(TX, CH_dist, JOIN_M_SIZE);
#endif
}
//设置普通节点的发送簇节点
void Sensor::setupDataTX(mSchedule *SCHED){

    int r = par("round");
    if(r == SCHED->getRound()){

        if(par("DistAwareCH")){
            if(CH_id != SCHED->getCHId()){
                CH_id = SCHED->getCHId();   // re-set the CH information if a better one has been designed by original CH
                CH_dist = distance(CH_id);
            }
        }

        // setup transmission time as the slot duration times my turn
        scheduleAt(simTime()+(SCHED->getDuration()*SCHED->getTurn()), startTX_e);
        cancelAndDelete(SCHED);
    }

}

void Sensor::sendData(){
    mData *DATA = new mData("data", DATA_M);
    DATA->setId(id);
    DATA->setRound(par("round"));
    if(CH_id > -1){
        // if node has CH
        double delay = propagationDelay(DATA_M_SIZE, CH_dist);
        cModule *CH;
        if(CH_id != BS_ID)
            CH = retrieveNode(CH_id);
        else
            CH = BS;
        sendDirect(DATA, delay, 0, CH->gate("in"));
        // ACCOUNT FOR DATA TRANSMISSION
        EnergyMgmt(TX, CH_dist, DATA_M_SIZE);

#ifndef ONE_TX_PER_ROUND
        // setup a timeout to receive a SCHED message for the next transmission
        double tout = propagationDelay(SCHED_M_SIZE,CH_dist);
        scheduleAt(simTime()+2*tout, rcvdSCHED_e);
        //TODO FINISH TO SETUP THE TIMEOUT (ALSO FOR THE FIRST SCHED) (only needed if multiple TX per round happens)
#endif
    }
    /*else
    {
        // if node doesn't have CH, just send directly to BS
        double delay = propagationDelay(DATA_M_SIZE, MAX_DIST(range));
        EnergyMgmt(TX, MAX_DIST(range), DATA_M_SIZE);
        scheduleAt(simTime()+delay, startTX_e); // schedule next autonomous send

    }*/
}


/**************** CLUSTER HEAD (CH) functions *********************/
void Sensor::broadcastADV()
{
    double ADV_delay = propagationDelay(ADV_M_SIZE, MAX_DIST(range)); // we consider maximum distance to reach all possible nodes

    for(unsigned int n = 0; n < N; n++){
        if(n != id){
            cModule * sensor = retrieveNode(n);
            mAdvertisement *ADV = new mAdvertisement("CH_advertisement", ADV_M);
            ADV->setId(id);
            sendDirect(ADV, ADV_delay, 0,  sensor->gate("in"));
        }
    }

#ifdef ACCOUNT_CH_SETUP
    // in this case, we consider an amount of energy to send a signal that
    // covers the entire sensed area
    EnergyMgmt(TX, MAX_DIST(range), ADV_M_SIZE);
#endif
    // set a timeout to receive JOIN messages
    // we consider a timeout equal to the maximum distance (i.e. range*2) propagation delay for both ADV to reach sensors
    // and for the JOIN msg to reach back at CH
    double JOIN_delay = propagationDelay(JOIN_M_SIZE, MAX_DIST(range));
    scheduleAt(simTime() + ADV_delay+JOIN_delay+EPSILON, rcvdJoin_e);

#ifdef ACCOUNT_CH_SETUP
    // ACCOUNT FOR ENERGY SPENT WHILE IN IDLE STATE to receive JOIN messages
    EnergyMgmt(RX, 0, JOIN_M_SIZE);
#endif


}

void Sensor::advertisementPhase()
{
    alreadyCH = true;   // node excludes itself from next election
    role = CH;
    broadcastADV(); // broadcast ADV message
    getDisplayString().setTagArg("i", 0, "old/ball2"); // UI feedback
}


double normailized(double x,double max,double min)
{
    if(max - min == 0) return 0;
    return (x-min)/(max - min);
}

bool pairCompareBoth( const std::pair<double, double> &firstEl, const std::pair<double,double> &secondEl)
{

    double x1 = firstEl.first*0.5 + firstEl.second*0.5;
    double x2 = secondEl.first*0.5 + secondEl.second*0.5;
    return (x1 < x2);
}

bool pairCompareDist( const std::pair<double, double> &firstEl, const std::pair<double,double> &secondEl)
{
    return (firstEl.first < secondEl.first);
}

bool pairCompareEnergy( const std::pair<double, double> &firstEl, const std::pair<double,double> &secondEl)
{
    return (firstEl.second < secondEl.second);
}

void Sensor::createTXSched()
{
    clusterN = msgBuf.size();


    sensor_max_dist = -1 * std::numeric_limits<double>::infinity();
    for(unsigned int i = 0; i < msgBuf.size(); i++){
        mJoin *JOIN = (mJoin *) msgBuf.at(i);
        double dist = distance(JOIN->getId());
        if(dist > sensor_max_dist){
            sensor_max_dist = dist;
        }
    }

#ifdef CH_SLOT_MAXDIST_IN_CLUSTER
    // 首先要计算这个簇中节点之间的最大距离


    // 根据每个节点发送的msg数据大小和集群中的最大传播延迟，必须为每个节点分配一个时间槽
    double slot = propagationDelay(DATA_M_SIZE, sensor_max_dist);
    double SCHED_delay = propagationDelay(SCHED_M_SIZE, sensor_max_dist);
#else
    // TDMA对于所有集群都是相等的(不取决于集群中的最大节点距离，而是取决于网络中的最大传播延迟)
    // 这是为了更好地比较能源效率与直接输电方式
    double slot = propagationDelay(DATA_M_SIZE, MAX_DIST(range));
    double SCHED_delay = propagationDelay(SCHED_M_SIZE, MAX_DIST(range));
#endif


    // ****************************************************
    // ***************AVOID TOO CLOSE CH STRATEGY**********
    // ****************************************************
    if(par("DistAwareCH") || par("EnergyAwareCH"))
    {
        int center_id = id;
        //初始化容量，用于排序
        std::vector<std::pair<double, double>> DistBatt;
        std::vector<std::pair<int, std::pair<double, double>>> List_IDFeat;

        double sumDist = 0;
        // 首先计算簇头的总距离
        for(unsigned int y = 0; y < msgBuf.size(); y++){
            mJoin *JOIN = (mJoin *) msgBuf.at(y);
            sumDist += distance(JOIN->getId()); //计算总距离
        }
        double max_energy = par("energy");
        std::pair<double, double> me(sumDist,max_energy - energy);
        DistBatt.push_back(me);
        std::pair<int, std::pair<double, double>> meSupport(id,me);
        List_IDFeat.push_back(meSupport);

        //然后检查集群中的其他节点中是否有更好的居中节点
        // 以避免过密CH和更均匀的传输
        double maxDist = 0,minDist = DBL_MAX,maxEn = 0,minEn = max_energy;
        for(unsigned int i = 0; i < msgBuf.size(); i++){
            sumDist = 0;
            mJoin *JOIN1 = (mJoin *) msgBuf.at(i);
            for(unsigned int y = 0; y < msgBuf.size(); y++){
                mJoin *JOIN2 = (mJoin *) msgBuf.at(y);
                sumDist += distance2s(JOIN1->getId(),JOIN2->getId());
            }

            Sensor *sensor = check_and_cast<Sensor *>(retrieveNode(JOIN1->getId()));


            EV << "SumDist for " << JOIN1->getId() << " = " << sumDist << " - energy = " << sensor->getEnergy() << "\n";
            maxDist = maxDist > sumDist ? maxDist : sumDist;
            minDist = minDist < sumDist ? minDist : sumDist;
            maxEn   = maxEn > max_energy - sensor->getEnergy() ? maxEn: max_energy - sensor->getEnergy();
            minEn   = minEn < max_energy - sensor->getEnergy() ? minEn: max_energy - sensor->getEnergy();
            std::pair<double,double> s(sumDist, max_energy - sensor->getEnergy());
            DistBatt.push_back(s);
            std::pair<int, std::pair<double, double>> sSupport(JOIN1->getId(),s);
            List_IDFeat.push_back(sSupport);

        }
        int count = DistBatt.size();
        for (int i = 0; i < count;i++)
        {
            DistBatt[i].first = normailized(DistBatt[i].first,maxDist,minDist);
            DistBatt[i].second = normailized(DistBatt[i].second,maxEn,minEn);

            EV <<"normilized dist:" <<DistBatt[i].first <<" || costed energy:"<< DistBatt[i].second << "\n";
        }

        if(par("DistAwareCH") && par("EnergyAwareCH"))
            // 根据能量和距离进行排序
            std::sort(DistBatt.begin(),DistBatt.end(),pairCompareBoth);
        else if(par("DistAwareCH"))
            //只根据距离
            std::sort(DistBatt.begin(),DistBatt.end(),pairCompareDist);
        else if(par("EnergyAwareCH"))
            //只根据能量
            std::sort(DistBatt.begin(),DistBatt.end(),pairCompareEnergy);


        // 让我们通过比较第一个新元素来找到原始id
        for(unsigned l = 0; l < List_IDFeat.size(); l++)
        {
            if(List_IDFeat.at(l).second == DistBatt.at(0)) center_id = List_IDFeat.at(l).first;
        }

        EV << "min SumDist is " << center_id << "\n";

        if(center_id != id)
        {
            // 如果我们最终选择了另一个簇头，则重置CH角色

            alreadyCH = false;   //将自己的CH设置成false
            role = SENSOR;
            getDisplayString().setTagArg("i", 0, "old/ball"); //界面显示

            //设置新的簇头
            CH_id = center_id;
            CH_dist = distance(center_id);

            // 让新的CH意识到它的新角色，并处理传入的数据
            // 发送一条消息(包含簇头中传感器的数量)
            //     为了收集TDMA调度后接收到的数据，压缩并发送到BS
            mCenterCH *CENTER = (mCenterCH *) new mCenterCH("alternative-CH", CENTER_M);
            CENTER->setClusterN(clusterN);
            CENTER->setIDLETime(clusterN*slot);
            CENTER->setSCHEDDelay(SCHED_delay);

            cModule *sensor = retrieveNode(CH_id);
            EV << "informing new CH \n";
            sendDirect(CENTER, 0, 0, sensor->gate("in"));

            // 将新的簇头模式发送给簇中其他节点
            for(unsigned int i = 0; i < msgBuf.size(); i++){
                mJoin *JOIN = (mJoin *) msgBuf.at(i);
                mSchedule *SCHED = (mSchedule *) new mSchedule("schedule-info", SCHED_M);
                SCHED->setTurn(i);
                SCHED->setDuration(slot);
                SCHED->setRound(par("round"));
                SCHED->setCHId(center_id); // 数据中包含第几回合以及谁是新的簇头

                if(JOIN->getId() != center_id){ //发送给其他节点
                    cModule *sensor = retrieveNode(JOIN->getId());
                    EV << "sending schedule to " << JOIN->getId() << "\n";
                    sendDirect(SCHED, SCHED_delay, 0, sensor->gate("in"));
                }else{ // 发送给簇头
                    EV << "sending schedule to MYSELF (NOT CH ANYMORE)\n";
                    scheduleAt(simTime()+SCHED_delay, SCHED);
                }
                cancelAndDelete(JOIN);
            }

            msgBuf.clear();
            #ifdef ACCOUNT_CH_SETUP
            //如果定义了这个值，那么就考虑建立CH需要消耗能量
            EnergyMgmt(TX, sensor_max_dist, SCHED_M_SIZE);
            #endif

        }
        else
        {
            // 和LEACH一样，不进行优化
            for(unsigned int i = 0; i < msgBuf.size(); i++){
                mJoin *JOIN = (mJoin *) msgBuf.at(i);
                mSchedule *SCHED = (mSchedule *) new mSchedule("schedule-info", SCHED_M);
                SCHED->setTurn(i);
                SCHED->setDuration(slot);
                SCHED->setRound(par("round"));
                SCHED->setCHId(id);
                cModule *sensor = retrieveNode(JOIN->getId());
                EV << "sending schedule to " << JOIN->getId() << "\n";
                sendDirect(SCHED, SCHED_delay, 0, sensor->gate("in"));
                cancelAndDelete(JOIN);
            }

            msgBuf.clear();
            #ifdef ACCOUNT_CH_SETUP
            //如果定义了这个值，那么就考虑建立CH需要消耗能量
            EnergyMgmt(TX, sensor_max_dist, SCHED_M_SIZE);
            #endif


            double IDLE_duration = clusterN*slot;
            //设置一个计时器，使无线电处于空闲模式，并接收所有数据(TDMA)
            // 超时将考虑SCHED msg到达目的地并按顺序接收所有的数据。
            scheduleAt(simTime() + SCHED_delay + IDLE_duration + EPSILON, rcvdData_e);
            #ifdef ACCOUNT_CH_SETUP
            //如果定义了这个值，那么就考虑建立CH需要消耗能量
            EnergyMgmt(RX, 0, clusterN*DATA_M_SIZE);
            #endif
        }

    }
    else
    // ****************************************************
    // *************** TRADITIONAL LEACH *****************
    // ****************************************************
    {
        // 和LEACH一样，不进行优化
        for(unsigned int i = 0; i < msgBuf.size(); i++){
            mJoin *JOIN = (mJoin *) msgBuf.at(i);
            mSchedule *SCHED = (mSchedule *) new mSchedule("schedule-info", SCHED_M);
            SCHED->setTurn(i);
            SCHED->setDuration(slot);
            SCHED->setRound(par("round"));
            SCHED->setCHId(id);
            cModule *sensor = retrieveNode(JOIN->getId());
            EV << "sending schedule to " << JOIN->getId() << "\n";
            sendDirect(SCHED, SCHED_delay, 0, sensor->gate("in"));
            cancelAndDelete(JOIN);
        }

        msgBuf.clear();
        #ifdef ACCOUNT_CH_SETUP
        //如果定义了这个值，那么就考虑建立CH需要消耗能量
        EnergyMgmt(TX, sensor_max_dist, SCHED_M_SIZE);
        #endif


        double IDLE_duration = clusterN*slot;
        scheduleAt(simTime() + SCHED_delay + IDLE_duration + EPSILON, rcvdData_e);
        #ifdef ACCOUNT_CH_SETUP
        EnergyMgmt(RX, 0, clusterN*DATA_M_SIZE);
        #endif
    }
}

void Sensor::compressAndSendToBS()
{
    //compress all data received
    EnergyMgmt(COMPRESS, 0, clusterN*DATA_M_SIZE);

    //send to base station
    //compute energy to send data considering maximum distance (i.e. highest energy)

    //unsigned int data_aggr_size = ceil((clusterN*DATA_M_SIZE)/COMP_FACTOR);
    unsigned int data_aggr_size = DATA_M_SIZE; // we just assume all the same packet size transmitted to BS after compression

#ifdef USE_BS_DIST
    EnergyMgmt(TX, BS_DIST(x,y), data_aggr_size);
#else
    EnergyMgmt(TX, MAX_DIST(range), data_aggr_size);
#endif

#ifndef ONE_TX_PER_ROUND
    // set-up the next transmission
    // in this case, we avoid to clear the buffer. We're gonna exploit polymorphism to
    // use DATA packets as JOIN packets in the new schedule creation (they both have id field).
    // This is useful for keeping track of nodes that are still sending DATA (in case someone died) and adjust
    // the TDMA schedule accordingly
#ifdef USE_BS_DIST
    double delay = propagationDelay(data_aggr_size, BS_DIST(x,y));
#else
    double delay = propagationDelay(data_aggr_size, MAX_DIST(range));
#endif
    scheduleAt(simTime()+delay,rcvdJoin_e); // schedule next transmission after Aggregated data has been (virtually) sent
#endif
}

void Sensor::handleData(cMessage *msg)
{
    int r = par("round");
    mData *DATA = (mData *) msg;
    if ((role == CH) && (r == DATA->getRound())){
        msgBuf.push_back(msg); // insert DATA into the message buffer
        EV << "received data from " << msg->getSenderModuleId() - 2 << "\n";
    }
}

/********* ENERGY functions **********/
// energy consumption to transmit k bit ad distance d
double Sensor::EnergyTX(unsigned int k, double d)
{
    return ((Eelec * k) + Eamp * k * pow(d,2));
}

// energy consumption to receive k bit
double Sensor::EnergyRX(unsigned int k)
{
    return Eelec * k;
}

// energy consumption to aggregate n messages of k bits (kN = k* n)
double Sensor::EnergyCompress(unsigned int kN)
{
    return Ecomp * kN;
}



void Sensor::EnergyMgmt(compState state, double d, unsigned int k)
{
    double cost = 0;    // cost of operation init

    switch(state)
    {
        case TX:
            cost = EnergyTX(k,d);
            EV << "TX cost is " << cost << " and energy is " << energy << " " << (cost < energy) << "\n";
            break;
        case RX:
            cost = EnergyRX(k);
            EV << "RX cost is " << cost << " and energy is " << energy << " " << (cost < energy) << "\n";
            break;
        case COMPRESS:
            cost = EnergyCompress(k);
            EV << "Compression cost is " << cost << " and energy is " << energy << " " << (cost < energy) << "\n";
            break;
    }

    emit(energySignal, energy);

    if (cost < energy)
    {
        // if we have enough energy, subtract the cost of operation from the actual energy
        energy -= cost;
        char buf[256];
        sprintf(buf, "energy %.2f\n", energy);
        getDisplayString().setTagArg("t", 0, buf);
    }
    else
    {
        //this operation will make the node die, so we can simply declare it as dead
        role = DEAD;
        EV << "Node " << id << " is DEAD.\n";
        getDisplayString().setTagArg("i", 0, "old/ball"); // UI feedback
        getDisplayString().setTagArg("i2", 0, "old/x_cross");
        cancelEvent(startRound_e);
        unsigned int Ndead = getParentModule()->par("Ndead");
        getParentModule()->par("Ndead") = Ndead+1;
        if (Ndead+1 == N) endSimulation(); // stop simulation if all nodes are dead
        int r = getParentModule()->par("round");
        if (Ndead+1 == 1) recordScalar("firstNodeDead", r);
    }
}


/********* Utilities ************/
cModule* Sensor::retrieveNode(unsigned int n)
{
   char modName[32];
   sprintf(modName,"node[%d]", n);
   return (cModule *) getModuleByPath(modName);
}

double Sensor::propagationDelay(unsigned int msg_size, double dist)
{
    // Compute the propagation delay based on packet size and distance
    // that is the time when the last bit of message is received
    double Dp = msg_size / bitrate; // packet duration
    return dist/C + Dp;  // propagation delay
}

double Sensor::distance(unsigned int id)
{
    cModule *sensor = retrieveNode(id);
    int sx = sensor->par("posX");
    int sy = sensor->par("posY");
    double dx = x - ((double) sx);
    double dy = y - ((double) sy);
    return sqrt( pow(dx,2) + pow(dy,2));
}

double Sensor::distance2s(unsigned int id1, unsigned int id2)
{
    cModule *sensor1 = retrieveNode(id1);
    int sx1 = sensor1->par("posX");
    int sy1 = sensor1->par("posY");
    cModule *sensor2 = retrieveNode(id2);
    int sx2 = sensor2->par("posX");
    int sy2 = sensor2->par("posY");
    double dx = ((double) sx1) - ((double) sx2);
    double dy = ((double) sy1) - ((double) sy2);
    return sqrt( pow(dx,2) + pow(dy,2));
}

double Sensor::getEnergy()
{
    return energy;
}

