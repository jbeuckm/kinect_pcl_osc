#include "kpoOscSender.h"
#include <iostream>



kpoOscSender::kpoOscSender()
{
    setup_ = false;
}

kpoOscSender::~kpoOscSender()
{
    delete transmitSocket;
}

void kpoOscSender::setNetworkTarget(const char *ip, int port)
{
    transmitSocket = new UdpTransmitSocket( IpEndpointName( ip, port ) );
    setup_ = true;
}


void kpoOscSender::send(const char *path, int value)
{
    if (!setup_) return;

    char buffer[OUTPUT_BUFFER_SIZE];

    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );

    p << osc::BeginBundleImmediate
      << osc::BeginMessage( path );

    p << value;

    p << osc::EndMessage;

    transmitSocket->Send( p.Data(), p.Size() );
}



void kpoOscSender::sendObject(int object_id, float x, float y, float z)
{
    if (!setup_) return;

    char buffer[OUTPUT_BUFFER_SIZE];

    osc::OutboundPacketStream p( buffer, OUTPUT_BUFFER_SIZE );

    p << osc::BeginBundleImmediate
      << osc::BeginMessage( "/object" );

    p << object_id << x << y << z;

    p << osc::EndMessage;

    transmitSocket->Send( p.Data(), p.Size() );
}


