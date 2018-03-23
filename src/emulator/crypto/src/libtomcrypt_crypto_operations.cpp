#include <crypto/libtomcrypt_crypto_operations.h>

#include <tomcrypt.h>

#undef aes_ecb_encrypt
#undef aes_ecb_decrypt

LibTomCryptCryptoOperations::LibTomCryptCryptoOperations()
{
   register_cipher(&aes_desc);

   register_hash(&sha1_desc);

   register_hash(&sha256_desc);
}

LibTomCryptCryptoOperations::~LibTomCryptCryptoOperations()
{
   unregister_cipher(&aes_desc);

   unregister_hash(&sha1_desc);

   unregister_hash(&sha256_desc);
}

int LibTomCryptCryptoOperations::aes_cbc_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_CBC cbc;
   if(cbc_start(idx, iv, key, key_size / 8, 0, &cbc) != CRYPT_OK)
      return -1;

   if(cbc_encrypt(src, dst, size, &cbc) != CRYPT_OK)
      return -1;

   if(cbc_done(&cbc) != CRYPT_OK)
      return -1;

   unsigned long len = 0x10;
   if(cbc_getiv(iv, &len, &cbc) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::aes_cbc_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv) const
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_CBC cbc;
   if(cbc_start(idx, iv, key, key_size / 8, 0, &cbc) != CRYPT_OK)
      return -1;

   if(cbc_decrypt(src, dst, size, &cbc) != CRYPT_OK)
      return -1;

   if(cbc_done(&cbc) != CRYPT_OK)
      return -1;

   unsigned long len = 0x10;
   if(cbc_getiv(iv, &len, &cbc) != CRYPT_OK)
      return -1;

   return 0;
}
   
int LibTomCryptCryptoOperations::aes_ctr_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv)
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_CTR ctr;
   if(ctr_start(idx, iv, key, key_size / 8, 0, 0, &ctr) != CRYPT_OK)
      return -1;

   if(ctr_encrypt(src, dst, size, &ctr) != CRYPT_OK)
      return -1;

   if(ctr_done(&ctr) != CRYPT_OK)
      return -1;

   unsigned long len = 0x10;
   if(ctr_getiv(iv, &len, &ctr) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::aes_ctr_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size, unsigned char* iv)
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_CTR ctr;
   if(ctr_start(idx, iv, key, key_size / 8, 0, 0, &ctr) != CRYPT_OK)
      return -1;

   if(ctr_decrypt(src, dst, size, &ctr) != CRYPT_OK)
      return -1;

   if(ctr_done(&ctr) != CRYPT_OK)
      return -1;

   unsigned long len = 0x10;
   if(ctr_getiv(iv, &len, &ctr) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::aes_ecb_encrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_ECB ecb;
   if(ecb_start(idx, key, key_size / 8, 0, &ecb) != CRYPT_OK)
      return -1;

   if(ecb_encrypt(src, dst, size, &ecb) != CRYPT_OK)
      return -1;

   if(ecb_done(&ecb) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::aes_ecb_decrypt(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   symmetric_ECB ecb;
   if(ecb_start(idx, key, key_size / 8, 0, &ecb) != CRYPT_OK)
      return -1;

   if(ecb_decrypt(src, dst, size, &ecb) != CRYPT_OK)
      return -1;

   if(ecb_done(&ecb) != CRYPT_OK)
      return -1;

   return 0;
}
   
int LibTomCryptCryptoOperations::aes_cmac(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const
{
   int idx = find_cipher("aes");
   if(idx < 0)
      return -1;

   omac_state os;
   if(omac_init(&os, idx, key, key_size / 8) != CRYPT_OK)
      return -1;

   if(omac_process(&os, src, size) != CRYPT_OK)
      return -1;

   unsigned long dstlen = 0x10;
   if(omac_done(&os, dst, &dstlen) != CRYPT_OK)
      return -1;

   return 0;
}
   
int LibTomCryptCryptoOperations::sha1(const unsigned char* src, unsigned char* dst, int size) const
{
   int idx = find_hash("sha1");
   if(idx < 0)
      return -1;
  
   hash_state hs;
   if(sha1_init(&hs) != CRYPT_OK)
      return -1;

   if(sha1_process(&hs, src, size) != CRYPT_OK)
      return -1;

   if(sha1_done(&hs, dst) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::sha256(const unsigned char* src, unsigned char* dst, int size) const
{
   int idx = find_hash("sha256");
   if(idx < 0)
      return -1;

   hash_state hs;
   if(sha256_init(&hs) != CRYPT_OK)
      return -1;

   if(sha256_process(&hs, src, size) != CRYPT_OK)
      return -1;

   if(sha256_done(&hs, dst) != CRYPT_OK)
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::hmac_sha1(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const
{
   int idx = find_hash("sha1");
   if(idx < 0)
      return -1;
  
   hmac_state hmac;
   if (hmac_init(&hmac, idx, key, key_size) != CRYPT_OK) 
      return -1;

   if(hmac_process(&hmac, src, size) != CRYPT_OK)
      return -1;

   unsigned long dstlen = 0x14;

   if (hmac_done(&hmac, dst, &dstlen) != CRYPT_OK) 
      return -1;

   return 0;
}

int LibTomCryptCryptoOperations::hmac_sha256(const unsigned char* src, unsigned char* dst, int size, const unsigned char* key, int key_size) const
{
   int idx = find_hash("sha256");
   if(idx < 0)
      return -1;
  
   hmac_state hmac;
   if (hmac_init(&hmac, idx, key, key_size) != CRYPT_OK) 
      return -1;

   if(hmac_process(&hmac, src, size) != CRYPT_OK)
      return -1;

   unsigned long dstlen = 0x20;

   if (hmac_done(&hmac, dst, &dstlen) != CRYPT_OK) 
      return -1;

   return 0;
}
