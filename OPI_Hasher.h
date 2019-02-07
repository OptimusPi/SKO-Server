#ifndef _HASHER_CPP_
#define _HASHER_CPP_

#include <argon2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include "base64.h"

#define HASHLEN 32

class OPI_Hasher
{
  public:
  static std::string hash(std::string _password, std::string _salt)
  {
    std::string hashResult = "";
    uint8_t SALTLEN = _salt.length();
    uint8_t hash[HASHLEN];
    uint8_t salt[SALTLEN];

    for (int i = 0; i < SALTLEN; i++)
      salt[i] = (uint8_t)_salt[i];

      uint8_t *pwd = (uint8_t *)strdup(_password.c_str());
      uint32_t pwdlen = strlen((char *)pwd);
      uint32_t t_cost = 2;            // 1-pass computation
      uint32_t m_cost = (1<<16);      // 64 mebibytes memory usage
      uint32_t parallelism = 1;       // number of threads and lanes

      // high-level API
      argon2d_hash_raw(t_cost, m_cost, parallelism, pwd, pwdlen, salt, SALTLEN, hash, HASHLEN);

      for (int i = 0; i < HASHLEN; ++i) 
      hashResult += hash[i]; 
    
      return base64_encode(hashResult);	
  }
};

#endif