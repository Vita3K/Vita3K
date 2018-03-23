// From psvpfstools by motoharu-gosuto

#pragma once

#include <memory>

#include "crypto_operations_interface.h"

enum class CryptoOperationsTypes
{
   libtomcrypt
};

class CryptoOperationsFactory
{
public:
   static std::shared_ptr<ICryptoOperations> create(CryptoOperationsTypes type);
};
