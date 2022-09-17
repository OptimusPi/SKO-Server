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

#include <cryptopp/hex.h>
using CryptoPP::HexEncoder;

#include <cryptopp/hmac.h>
using CryptoPP::HMAC;
using CryptoPP::SHA256;
using CryptoPP::HashFilter;

class OPI_Crypto
{
    public:
        /// @brief 
        /// @param aesKey 
        /// @param cleartext 
        /// @return 
        std::string Encrypt(std::string aesKey, std::string cleartext);
        std::string Decrypt(std::string aesKey, std::string cipher);
        static CryptoPP::SecByteBlock GetKey(std::string aesKeyStr);

    private:

};

#endif