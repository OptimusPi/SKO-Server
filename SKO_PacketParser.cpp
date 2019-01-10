#include "SKO_PacketParser.h"

SKO_PacketParser::SKO_PacketParser(std::string packet)
{
    this->packet = packet;
    
    // Cannot proceed without the packet length and packet type bytes.
    if (packet.length() < 2)
        return;

    this->packetLength = packet[0];
    this->packetType = packet[1];
}

std::string SKO_PacketParser::getPacketBody()
{
    if (this->packet.length() <= 2)
        return "";

    return this->packet.substr(2);
}

unsigned char SKO_PacketParser::getPacketType()
{
    return this->packetType;
}

unsigned char SKO_PacketParser::getPacketLength()
{
    return this->packetLength; 
}

std::string SKO_PacketParser::toString() 
{
    std::stringstream prettyString;

    // Print such as {5, 251, 1, 2, 1}
    prettyString << "{";
    for (int i = 0; i < packet.length(); i++) 
    {
        prettyString << (int)this->packet[i];

        if (i < packet.length() - 1)
            prettyString << ", ";
    }
    prettyString << "}";

    return prettyString.str();
}

// Parsing functions
char SKO_PacketParser::nextByte()
{ 
    return this->packet[position++];
}
short SKO_PacketParser::nextShort()
{
    short value = 0;
    ((char *)&value)[0] = this->nextByte();
    ((char *)&value)[1] = this->nextByte();
    return value;
}
int SKO_PacketParser::nextInt()
{
    int value = 0;
    ((char *)&value)[0] = this->nextByte();
    ((char *)&value)[1] = this->nextByte();
    ((char *)&value)[2] = this->nextByte();
    ((char *)&value)[3] = this->nextByte();
    return value;
}
float SKO_PacketParser::nextFloat()
{
    float value = 0;
    ((char *)&value)[0] = this->nextByte();
    ((char *)&value)[1] = this->nextByte();
    ((char *)&value)[2] = this->nextByte();
    ((char *)&value)[3] = this->nextByte();
    return value;
}
double SKO_PacketParser::nextDouble()
{
    double value = 0;
    ((char *)&value)[0] = this->nextByte();
    ((char *)&value)[1] = this->nextByte();
    ((char *)&value)[2] = this->nextByte();
    ((char *)&value)[3] = this->nextByte();
    ((char *)&value)[4] = this->nextByte();
    ((char *)&value)[5] = this->nextByte();
    ((char *)&value)[6] = this->nextByte();
    ((char *)&value)[7] = this->nextByte();
    return value;
}