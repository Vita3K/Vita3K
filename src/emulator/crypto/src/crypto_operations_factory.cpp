#include <stdexcept>

#include <crypto/crypto_operations_factory.h>
#include <crypto/libtomcrypt_crypto_operations.h>

#include <memory>

std::shared_ptr<ICryptoOperations> CryptoOperationsFactory::create(CryptoOperationsTypes type)
{
   switch(type)
   {
   case CryptoOperationsTypes::libtomcrypt:
      return std::make_shared<LibTomCryptCryptoOperations>();
   default:
      throw std::runtime_error("unexpected CryptoOperationsTypes value");
   }
}
