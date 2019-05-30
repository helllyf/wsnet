package datacentric.DataCentricTestApp;
import  inet.applications.trafgen.TrafGenPar;

simple DataCentricTestApp extends TrafGenPar
{
    parameters:
        @class(DataCentricTestApp);
        bool debug;
        
        string sinkFor;
        string sourceFor;// = "\x48\x49\x54";
        string nodeContext;
        //sourceFor[5]: string;
        //int x[5];
        double startTime;
        
        //sinks[numSinks] : 
        //byte y;
        //char x;
        
   
}
