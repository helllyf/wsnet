//
// Copyright (C) 2008 Feng Chen
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//

//package inet.examples.wpan.StarNet;
//package datacentric;

import datacentric.DataCentricNode;
import inet.world.radio.ChannelControl;

network DataCentricNet
{
    parameters:
        double numHosts;
        double nodeSeparation @unit("m")=default (3m);
        //string STRINGEND = "\x0";
        //string DEMAND_QUERYBASED = "\x2";
        //string DEMAND_COLLABERATIONBASED = "\x42";
        //string ENVIRONMENTAL_EVENTBASED = "\x83";
        //string CURRENT = "\x2";
        //string BID = "\x1";
        //string OCCUPANCY = "\x1";
		host[*].app.startTime = 1.0;
		host[*].app.nodeContext = "\x06";
		//host[4].app.sourceFor = DEMAND_QUERYBASED+CURRENT+STRINGEND;
//		host[*].app.sourceFor = "";
		//host{1..24}.app.sourceFor = "";
		//host[20].app.sinkFor = DEMAND_QUERYBASED+CURRENT+STRINGEND;
		//host{0..19}.app.sinkFor = "";
		//host{21..24}.app.sinkFor = "";
                submodules:
        host[numHosts]: DataCentricNode {
            parameters:
                  y =  (floor(index/sqrt(numHosts))*nodeSeparation)+(nodeSeparation);
                  x = ((index%floor(sqrt(numHosts)))*nodeSeparation)+(nodeSeparation);
                  //ss = 15 + (floor(fmod(index,4)/3)*20);//(fmod(index,4)*10)+10;
                  //@display("b=$ss;i=device/pocketpc_s;r=0,,,0");//;r=,,#707070");
                  @display("i=device/pocketpc_s;r=,,#707070");
                  //@display("i=device/pocketpc_s;r=,,#707070;p=84,57");
        }
        channelControl: ChannelControl {
            parameters:
                @display("p=-65,20;i=misc/sun");
        }
        connections allowunconnected:
}

