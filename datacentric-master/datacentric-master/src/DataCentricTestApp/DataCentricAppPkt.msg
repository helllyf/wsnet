
cplusplus {{
	#include<vector>
	typedef std::vector<char> PktDataType;
}}


class PktDataType extends void;

packet DataCentricAppPkt 
{
    @customize(true);
    PktDataType pktData; 	// name of the data
    simtime_t creationTime; // creation time of the packet
}
