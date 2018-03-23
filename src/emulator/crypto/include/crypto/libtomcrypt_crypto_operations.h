// From psvpfstools by motoharu-gosuto

#pragma once

#include "crypto_operations_interface.h"

class LibTomCryptCryptoOperations : public ICryptoOperations
{
public:
   LibTomCryptCryptoOperations();

   ~LibTomCryptCryptoOperations();

public:
   int aes_cbc_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const override;
   int aes_cbc_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const override;

   int aes_ctr_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) override;
   int aes_ctr_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) override;

   int aes_ecb_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const override;
   int aes_ecb_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const override;

   int aes_cmac(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const override;

   int sha1(const unsigned char* src, unsigned char* dst, int size) const override;
   int sha256(const unsigned char* src, unsigned char* dst, int size) const override;

   int hmac_sha1(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const override;
   int hmac_sha256(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const override;
};
