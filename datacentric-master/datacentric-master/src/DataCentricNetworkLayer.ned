package datacentric;
simple DataCentricNetworkLayer
{
    parameters:
        // debug switch
        bool debug; 				// debug switch
        bool isPANCoor;
        int nodeConstraint;
        double startTime;
		volatile double routingDelay @unit("s") = default(uniform(0s,0.005s));        
    gates:
        input upperLayerIn; 			// from the upper layer
        output upperLayerOut; 		// to the upper layer
        input lowerLayerIn; 			// from the physical layer
        output lowerLayerOut; 			// to the physical layer
}
