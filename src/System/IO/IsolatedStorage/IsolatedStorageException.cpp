#include "System/IO/IsolatedStorage/IsolatedStorageException.hpp"

namespace System::IO::IsolatedStorage
{
    IsolatedStorageException::IsolatedStorageException(const std::string& message)
        : System::Exception(message)
    {
    }
}