#include "OPI_Crypto.h"
#include "OPI_Utilities.h"
#include <exception>
#include <iostream>
#include <sstream>

CryptoPP::SecByteBlock OPI_Crypto::GetKey(std::string aesKeyStr)
{
    std::string aesKeyBytes;
    CryptoPP::StringSource aesKeySS(aesKeyStr, true, new CryptoPP::HexDecoder(new StringSink(aesKeyBytes)));
	CryptoPP::SecByteBlock key((const byte*)aesKeyBytes.data(), 256/8);

	return key;
}

std::string OPI_Crypto::Encrypt(std::string aesKeyStr, std::string clearText)
{
	CryptoPP::CBC_Mode<AES>::Encryption enc;
    std::string cipherText;
	auto key = OPI_Crypto::GetKey(aesKeyStr);


    // Unique initilization vector for each message
    // Sometimes called a NONCE.
    byte iv[AES::BLOCKSIZE];
    AutoSeededRandomPool prng;
    prng.GenerateBlock(iv, sizeof(iv));

    enc.SetKeyWithIV( key, 256/8, iv);

    CryptoPP::StringSource ss1( clearText, true,
        new StreamTransformationFilter( enc,
            new Base64Encoder(
                new StringSink(cipherText), false
            ) // Base64Encoder
        ) // StreamTransformationFilter
    ); // StringSource

    std::string ivString;
    CryptoPP::StringSource ss2( iv, sizeof(iv), true,
        new HexEncoder(
            new StringSink(ivString), false
        ) // Base64Encoder
    ); // StringSource

    // calculate and attach HMAC
    std::string hmacStr;
    std::stringstream cipher;
    cipher << ivString << ":" << cipherText; 

    HMAC<SHA256> hmac(key, key.size());

    CryptoPP::StringSource ssMac(cipher.str(), true, 
        new HashFilter(hmac,
            new HexEncoder(
                new StringSink(hmacStr), false
            )
        ) // HashFilter
    ); // StringSource

	cipher << ":" << hmacStr;

    return cipher.str();
}

std::string OPI_Crypto::Decrypt(std::string aesKeyStr, std::string cipher)
{
	// get secure key from key string
	auto key = OPI_Crypto::GetKey(aesKeyStr);

    // Split cipher text into IV, cipherText, aand 
    auto cipherSplit = OPI_Utilities::split(cipher, ':');
    if (cipherSplit.size() < 3)
	{
		perror("cipherText is not legit.");
		return "";
	}

    std::string ivStr = cipherSplit[0];
	std::string cipherText = cipherSplit[1];
	std::string hmacStr = cipherSplit[2];
	std::string aesKeyHex;
	std::string ivHex;

    //first check hmac
	std::string calculatedHmac;
	std::stringstream ssMessage;
	ssMessage << ivStr << ":" << cipherText;
	std::string messageStr = ssMessage.str();

	HMAC<SHA256> hmac(key, key.size());

	StringSource ssMac(messageStr, true, 
		new HashFilter(hmac,
			new HexEncoder(
				new StringSink(calculatedHmac), false
			)
		) // HashFilter      
	); // StringSource

    // need time constant...?
	if (hmacStr != calculatedHmac)
	{
		perror("BAD CIPHER!");
		return "";
	} 
	else if (hmacStr.length() < 1) {
		perror("BAD CIPHER!");
		return "";
	}

	// Parse IV as HEX and get raw bytes
	CryptoPP::StringSource ivSS(ivStr,
		true, new CryptoPP::HexDecoder(new StringSink(ivHex)));
	CryptoPP::SecByteBlock iv((const byte*)ivHex.data(), 128/8);

    std::string clearText;
    CryptoPP::CBC_Mode<AES>::Decryption dec;
    dec.SetKeyWithIV( key, 32, iv ); // 256 bit / 8 bits per byte = 32

    // The StreamTransformationFilter removes
    //  padding as required.
    StringSource ss3(cipherText, true, 
        new Base64Decoder(
            new StreamTransformationFilter( dec,
                new StringSink( clearText ),
                StreamTransformationFilter::NO_PADDING,
                true
            ) // StreamTransformationFilter
        )
    ); // StringSource

    return clearText;
}