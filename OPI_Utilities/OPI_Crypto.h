#ifndef __OPI_CRYPTO_H_
#define __OPI_CRYPTO_H_

#include <cryptopp/aes.h>
using CryptoPP::AES;

#include <cryptopp/osrng.h>
using CryptoPP::AutoSeededRandomPool;

#include <cryptopp/ccm.h>
using CryptoPP::CBC_Mode;

#include <cryptopp/base64.h>
using CryptoPP::Base64Encoder;
using CryptoPP::Base64Decoder;

#include <cryptopp/filters.h>
using CryptoPP::StringSink;
using CryptoPP::StringSource;
using CryptoPP::StreamTransformationFilter;
using CryptoPP::AuthenticatedEncryptionFilter;
using CryptoPP::BlockPaddingSchemeDef;
using CryptoPP::ArraySink;

#include <cryptopp/hex.h>
using CryptoPP::HexEncoder;
using CryptoPP::HexDecoder;

#include <cryptopp/hmac.h>
using CryptoPP::HMAC;
using CryptoPP::SHA256;
using CryptoPP::HashFilter;
using CryptoPP::HashVerificationFilter;



class OPI_Crypto
{
    public:

        OPI_Crypto(std::string aesKey);
        ~OPI_Crypto();

        std::string Encrypt(std::string cleartext);
        std::string Decrypt(std::string cipher);

    private:
        void GetKey_Hex(std::string aesKeyHexStr);
        std::string ValidateHMAC(std::string cipher);
        std::string GenerateHMAC(std::string cipher);
        CryptoPP::SecByteBlock key;
};

#endif