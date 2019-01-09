#include <string>
#include <sstream>

#ifndef __SKO_PACKETPARSER_H_
#define __SKO_PACKETPARSER_H_

class SKO_PacketParser
{
public:
    // Constructor takes a full packet
    // Byte 1 is length of the packet (0-255)
    // Byte 2 is packet type code (0-255)
    // The remaining bytes are data such as:
    //      string: regular text such as username, password, chat, clan name, etc.
    //      float, int, unsigned int: broken into 4 individual bytes as (unsigned char)
    //      short int, unsigned short int: broken into 2 individual bytes as (unsigned char)
    //      unsigned char as itself
    //      char as itself
    // Examples:
    //      [7][CHAT]['H']['E']['L']['L']['O']
    //      [5][VERSION_CHECK][VERSION_MAJOR][VERSION_MINOR][VERSION_PATCH]
    //      [2][PING]
    //      [2][PONG]
    SKO_PacketParser(std::string packet);

    ///
    // Getter functions
    //

    // Get the packet body, sometimes helpful for strings
    std::string getPacketBody();
    // Returns packet codes such as CHAT, REGISTER, LOGIN
    unsigned char getPacketType();
    // Returns packet length code valued between 0-255
    unsigned char getPacketLength();

    // Format packet as a readable string for logging purposes
    std::string toString(); 

private:
    // Cursor to keep track of position
    unsigned int position = 0;
    std::string packet = "";
    unsigned char packetType = 0;
    unsigned char packetLength = 0; 

    // Parse primitive types from binary
    char nextByte();
    short nextShort();
    int nextInt();
    float nextFloat();
    double nextDouble();
};

#endif