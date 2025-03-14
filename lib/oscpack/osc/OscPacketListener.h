#ifdef OSC_ON

/*
	oscpack -- Open Sound Control (OSC) packet manipulation library
    http://www.rossbencina.com/code/oscpack

    Copyright (c) 2004-2013 Ross Bencina <rossb@audiomulch.com>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
	The text above constitutes the entire oscpack license; however, 
	the oscpack developer(s) also make the following non-binding requests:

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version. It is also 
	requested that these non-binding requests be included whenever the
	above license is reproduced.
*/
#ifndef INCLUDED_OSCPACK_OSCPACKETLISTENER_H
#define INCLUDED_OSCPACK_OSCPACKETLISTENER_H

#include "OscReceivedElements.h"
#include "../ip/PacketListener.h"


namespace osc{

class OscPacketListener : public PacketListener{ 
protected:
    virtual void ProcessBundle( const osc::ReceivedBundle& b, 
				const IpEndpointName& remoteEndpoint )
    {
        // ignore bundle time tag for now

        for( ReceivedBundle::const_iterator i = b.ElementsBegin(); 
				i != b.ElementsEnd(); ++i ){
            if( i->IsBundle() )
                ProcessBundle( ReceivedBundle(*i), remoteEndpoint );
            else
                ProcessMessage( ReceivedMessage(*i), remoteEndpoint );
        }
    }

    virtual void ProcessMessage( const osc::ReceivedMessage& m, 
				const IpEndpointName& remoteEndpoint ) = 0;
    
public:
	virtual void ProcessPacket( const char *data, int size, 
			const IpEndpointName& remoteEndpoint )
    {
        osc::ReceivedPacket p( data, size );
        if( p.IsBundle() )
            ProcessBundle( ReceivedBundle(p), remoteEndpoint );
        else
            ProcessMessage( ReceivedMessage(p), remoteEndpoint );
    }
};

} // namespace osc

#endif /* INCLUDED_OSCPACK_OSCPACKETLISTENER_H */
#endif