
#include "DataCentricAppPkt_m.h"



class DataCentricAppPkt : public DataCentricAppPkt_Base
{
public:
    DataCentricAppPkt(const char *name=NULL) : DataCentricAppPkt_Base(name) {}
    DataCentricAppPkt(const DataCentricAppPkt& other) : DataCentricAppPkt_Base(other) {}
    DataCentricAppPkt& operator=(const DataCentricAppPkt& other)
    {DataCentricAppPkt_Base::operator=(other); return *this;}
    virtual DataCentricAppPkt *dup() const {return new DataCentricAppPkt(*this);}

    // ADD CODE HERE to redefine and implement pure virtual functions from DataCentricAppPkt_Base
    const char *getDisplayString() const;
};




/*
class DataCentricAppPkt : public DataCentricAppPkt_Base
{
  private:
    //void copy(const DataCentricAppPkt& other) {
    //    this->pktData_var = other.pktData_var;
    //    this->creationTime_var = other.creationTime_var;

    //     }

  public:
    DataCentricAppPkt(const char *name=NULL, int kind=0) : DataCentricAppPkt_Base(name,kind) {}
    DataCentricAppPkt(const DataCentricAppPkt& other) : DataCentricAppPkt_Base(other) {copy(other);}
    DataCentricAppPkt& operator=(const DataCentricAppPkt& other) {if (this==&other) return *this; DataCentricAppPkt_Base::operator=(other); copy(other); return *this;}
    virtual DataCentricAppPkt *dup() const {return new DataCentricAppPkt(*this);}
    // ADD CODE HERE to redefine and implement pure virtual functions from DataCentricAppPkt_Base
    const char *getDisplayString() const;

};
*/
