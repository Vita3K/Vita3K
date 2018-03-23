// From psvpfstools by motoharu-gosuto

#pragma once

class ICryptoOperations
{
public:
   virtual ~ICryptoOperations(){}

public:
   virtual int aes_cbc_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const = 0;
   virtual int aes_cbc_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const = 0;
   
   virtual int aes_ctr_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) = 0;
   virtual int aes_ctr_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) = 0;

   virtual int aes_ecb_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const = 0;
   virtual int aes_ecb_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const = 0;
   
   virtual int aes_cmac(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const = 0;
   
   virtual int sha1(const unsigned char* src, unsigned char* dst, int size) const = 0;
   virtual int sha256(const unsigned char* src, unsigned char* dst, int size) const = 0;

   virtual int hmac_sha1(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const = 0;
   virtual int hmac_sha256(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const = 0;
};
