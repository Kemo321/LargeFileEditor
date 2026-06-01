/**
 * @file FileUtils.h
 * @author Jan Szwagierczak
 * @brief File-level helpers (binary detection) decoupled from the UI.
 */

#pragma once

#include <QString>

/**
 * @namespace FileUtils
 * @brief Stateless helpers for inspecting files before they are opened.
 */
namespace FileUtils {

/**
 * @brief Heuristically determines whether a file is binary (and thus unsupported).
 *
 * Reads the first 4 KiB and flags the file as binary if it contains NUL bytes or if the
 * ratio of control characters and invalid UTF-8 sequences exceeds a threshold.
 *
 * @param filePath Path to the file to inspect.
 * @return True if the file appears to be binary.
 */
[[nodiscard]] auto isBinaryFile( const QString& filePath ) -> bool;

}  // namespace FileUtils
