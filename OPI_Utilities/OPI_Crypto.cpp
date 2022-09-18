#include "OPI_Crypto.h"
#include "OPI_Utilities.h"
#include <exception>
#include <iostream>
#include <sstream>

OPI_Crypto::OPI_Crypto(std::string aesKeyHex)
{
	if (aesKeyHex.length() < 64)
	{
		std::cerr << "!!FATAL ERROR!! AES KEY LENGTH IS: " 
			<< aesKeyHex.length() 
			<< std::endl;
		exit(1);
	}
	this->SetKey_WithHex(aesKeyHex);
}

OPI_Crypto::~OPI_Crypto()
{
	//this->key.deallocate();
}

void OPI_Crypto::SetKey_WithHex(std::string aesKeyHexStr)
{
	if (aesKeyHexStr.length() != 64) {
		printf("the aes hex key is the wrong size.\r\n");
		return;
	}

	std::string keyBytes;
    CryptoPP::StringSource aesKeySS(aesKeyHexStr, true, 
		new CryptoPP::HexDecoder(
			new StringSink(keyBytes)
		)
	);
	CryptoPP::SecByteBlock keySecByteBlock(reinterpret_cast<const byte*>(&keyBytes[0]), keyBytes.size());
	keyBytes.clear();
	this->key = keySecByteBlock;
}

std::string OPI_Crypto::Encrypt(std::string clearText)
{
	CryptoPP::CBC_Mode<AES>::Encryption enc;
    std::string cipherText;

    // Unique initilization vector for each message
    // Sometimes called a NONCE.
    byte iv[AES::BLOCKSIZE];
    AutoSeededRandomPool prng;
    prng.GenerateBlock(iv, sizeof(iv));

    enc.SetKeyWithIV( this->key, 256/8, iv);

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
    std::string hmacHexStr;
    std::stringstream cipher;
    cipher << ivString << ":" << cipherText; 
	
    HMAC<SHA256> hmac(this->key, this->key.size());

    CryptoPP::StringSource ssMac(cipher.str(), true, 
        new HashFilter(hmac,
            new HexEncoder(
                new StringSink(hmacHexStr), false
            )
        ) // HashFilter
    ); // StringSource

	cipher << ":" << hmacHexStr;

    return cipher.str();
}


std::string OPI_Crypto::Decrypt(std::string cipher)
{
    // Split cipher text into IV, cipherText, aand 
    auto cipherSplit = OPI_Utilities::split(cipher, ':');
    if (cipherSplit.size() < 3)
	{
		perror("cipherText is not legit.");
		return "";
	}

	// Parse cypher into iv, cypherText, and hmac
    std::string ivStr = cipherSplit[0];
	std::string cipherText = cipherSplit[1];
	std::string hmacHexStr = cipherSplit[2];
	

    //first check hmac
	try
	{
		// Initialize SHA-256 HMAC 
		HMAC<SHA256> hmac(this->key, this->key.size());
		
		// Decode Hex hmac into raw bytes
		std::string hmacBytes;
		CryptoPP::StringSource ssMac(hmacHexStr, true, 
				new CryptoPP::HexDecoder(
					new StringSink(hmacBytes)
				)
    	);

		// The message that generated the HMAC was "{iv}:{cypherText}"
		std::stringstream ssMessage;
		ssMessage << ivStr << ":" << cipherText;
		
		// Securely validate HMAC with constant-time
		// input bytes + hmac
		StringSource ss(ssMessage.str() + hmacBytes, 
			true, /* Pump All */
			new HashVerificationFilter(hmac, 
				NULL, 
				HashVerificationFilter::THROW_EXCEPTION | HashVerificationFilter::HASH_AT_END
			)
		); // StringSource

		std::cout << "Verified message" << std::endl;
	}
	catch(const CryptoPP::Exception& e)
	{
		std::cerr << e.what() << std::endl;
		return "";
	}

	// Parse IV as HEX and get raw bytes
	std::string ivHex;
	CryptoPP::StringSource ivSS(ivStr,
		true, new CryptoPP::HexDecoder(new StringSink(ivHex)));
	CryptoPP::SecByteBlock iv((const byte*)ivHex.data(), 128/8);
	ivHex.clear();

    std::string clearText;
    CryptoPP::CBC_Mode<AES>::Decryption dec;
    dec.SetKeyWithIV( this->key, 32, iv ); // 256 bit / 8 bits per byte = 32

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