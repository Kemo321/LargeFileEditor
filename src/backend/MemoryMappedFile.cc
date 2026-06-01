// Author: Tomasz Okon

#include "backend/MemoryMappedFile.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <utility>

MemoryMappedFile::MemoryMappedFile( const std::string& filePath )
{
    fd_ = open( filePath.c_str(), O_RDONLY );
    if( fd_ == -1 ) {
        return;
    }

    struct stat statBuffer {};
    if( fstat( fd_, &statBuffer ) == -1 ) {
        close( fd_ );
        fd_ = -1;
        return;
    }

    size_ = static_cast<uint64_t>( statBuffer.st_size );
    const void* mapped = mmap( nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0 );
    if( mapped == MAP_FAILED ) {
        data_ = nullptr;
        size_ = 0;
    } else {
        data_ = static_cast<const char*>( mapped );
    }
}

MemoryMappedFile::~MemoryMappedFile()
{
    closeMmap();
}

auto MemoryMappedFile::closeMmap() -> void
{
    if( data_ != nullptr ) {
        munmap( const_cast<char*>( data_ ), size_ );
        data_ = nullptr;
    }
    size_ = 0;
    if( fd_ != -1 ) {
        close( fd_ );
        fd_ = -1;
    }
}

MemoryMappedFile::MemoryMappedFile( MemoryMappedFile&& other ) noexcept
    : data_( other.data_ ), size_( other.size_ ), fd_( other.fd_ )
{
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_ = -1;
}

auto MemoryMappedFile::operator=( MemoryMappedFile&& other ) noexcept -> MemoryMappedFile&
{
    if( this != &other ) {
        closeMmap();
        data_ = other.data_;
        size_ = other.size_;
        fd_ = other.fd_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.fd_ = -1;
    }
    return *this;
}

auto MemoryMappedFile::data() const -> const char*
{
    return data_;
}

auto MemoryMappedFile::size() const -> uint64_t
{
    return size_;
}

auto MemoryMappedFile::isValid() const -> bool
{
    return data_ != nullptr && size_ > 0;
}
