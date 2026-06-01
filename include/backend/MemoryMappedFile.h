/**
 * @file MemoryMappedFile.h
 * @author Tomasz Okon
 * @brief RAII wrapper around POSIX memory-mapping of a read-only file.
 */

#pragma once

#include <cstdint>
#include <string>

/**
 * @class MemoryMappedFile
 * @brief Owns a read-only `mmap` of a file, exposing it as a contiguous byte buffer.
 *
 * Encapsulates `open`/`fstat`/`mmap`/`munmap`/`close`. A failed mapping leaves the object
 * invalid (`data() == nullptr`, `size() == 0`) rather than exposing `MAP_FAILED`.
 */
class MemoryMappedFile {
public:
    /// Constructs an empty (invalid) mapping.
    MemoryMappedFile() = default;

    /**
     * @brief Memory-maps the file at @p filePath read-only.
     * @param filePath Path to the file to map.
     */
    explicit MemoryMappedFile( const std::string& filePath );

    ~MemoryMappedFile();

    MemoryMappedFile( const MemoryMappedFile& ) = delete;
    auto operator=( const MemoryMappedFile& ) -> MemoryMappedFile& = delete;

    /// Move constructor: takes over @p other's mapping, leaving it invalid.
    MemoryMappedFile( MemoryMappedFile&& other ) noexcept;
    /// Move assignment: releases the current mapping and takes over @p other's.
    auto operator=( MemoryMappedFile&& other ) noexcept -> MemoryMappedFile&;

    /**
     * @brief Pointer to the mapped bytes.
     * @return The buffer, or nullptr if the mapping is invalid.
     */
    [[nodiscard]] auto data() const -> const char*;

    /**
     * @brief Size of the mapped region in bytes.
     * @return The size, or 0 if the mapping is invalid.
     */
    [[nodiscard]] auto size() const -> uint64_t;

    /**
     * @brief Whether the mapping is usable.
     * @return True if a non-empty buffer is mapped.
     */
    [[nodiscard]] auto isValid() const -> bool;

private:
    auto closeMmap() -> void;

    const char* data_{ nullptr };
    uint64_t size_{ 0 };
    int fd_{ -1 };
};
