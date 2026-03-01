/**
 * @file def.h
 * @brief Global definitions, enumerations, and string mapping utilities for CppTrail.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_DEF_H
#define CPPTRAIL_DEF_H

#include <array>
#include <string>
#include <string_view>

#include "cpptrail/utf42.h"

/**
 * @namespace CppTrail
 * @brief Root namespace for the CppTrail logging library.
 */
namespace CppTrail {
    /**
     * @enum Status
     * @brief Represents the current operational state of a Logger implementation.
     */
    enum class Status {
        RUNNING, ///< The logger is active and processing entries.
        STOPPED, ///< The logger has been gracefully shut down.
        BROKEN, ///< The logger encountered a fatal error (e.g., IO failure).
        TRASCENDENT, ///< The logger operates outside standard lifecycle management.
    };

    /**
     * @enum Level
     * @brief Severity levels for log entries.
     */
    enum class Level {
        SUCCESS, ///< Positive confirmation of a successful operation.
        TRACE, ///< Extremely fine-grained diagnostic events (wire-level).
        DEBUG, ///< Information useful for application debugging.
        INFO, ///< General operational messages about application progress.
        MESSAGE, ///< High-level communication or protocol-specific messages.
        WARNING, ///< Indications of potential issues or non-critical failures.
        ERROR, ///< Significant issues that require attention but allow continued execution.
        CRITICAL, ///< Severe failures that may lead to localized component shutdown.
        FATAL, ///< Terminal errors that require immediate application termination.
    };

    /**
     * @namespace Detail
     * @brief Internal implementation details. Not intended for direct public use.
     */
    namespace Detail {
        /**
         * @brief Template utility to map Level enums to string representations.
         * @tparam char_t The character type (char, char8_t, char16_t, char32_t).
         */
        template<typename char_t>
        struct LevelName {
            /**
             * @brief Retrieves the level name as a basic_string.
             * @param nLevel The log level.
             * @return A string containing the level name, or an empty string if invalid.
             */
            static std::basic_string<char_t> getName(Level nLevel) {
                if (nLevel > Level::FATAL) return std::basic_string<char_t>{};
                return std::basic_string<char_t>{NAMES[static_cast<std::size_t>(nLevel)]};
            }

            /**
             * @brief Retrieves a view of the level name.
             * @param nLevel The log level.
             * @return A string_view pointing to the static name buffer.
             */
            static std::basic_string_view<char_t> getNameView(Level nLevel) noexcept {
                if (nLevel > Level::FATAL) return std::basic_string_view<char_t>{};
                return NAMES[static_cast<std::size_t>(nLevel)];
            }

            /// Static array of localized/encoded level names.
            constexpr static std::array<std::basic_string_view<char_t>, 9> NAMES = {
                make_poly_enc(char_t, "SUCCESS"),
                make_poly_enc(char_t, "TRACE"),
                make_poly_enc(char_t, "DEBUG"),
                make_poly_enc(char_t, "INFO"),
                make_poly_enc(char_t, "MESSAGE"),
                make_poly_enc(char_t, "WARNING"),
                make_poly_enc(char_t, "ERROR"),
                make_poly_enc(char_t, "CRITICAL"),
                make_poly_enc(char_t, "FATAL"),
            };
        };
    }

    /** @name String Getters (Allocating)
     * Functions that return a heap-allocated copy of the level name.
     * @{ */

    /**
     * @brief Gets the level name as a std::string.
     * @param nLevel The severity level to convert.
     * @return A string containing the name.
     */
    inline std::string getName(const Level nLevel) {
        return Detail::LevelName<char>::getName(nLevel);
    }

#if __cplusplus >= 202002L
    /**
     * @brief Gets the level name as a std::u8string.
     * @param nLevel The severity level to convert.
     * @return A UTF-8 string containing the name.
     */
    inline std::u8string u8getName(const Level nLevel) {
        return Detail::LevelName<char8_t>::getName(nLevel);
    }
#endif

    /**
     * @brief Gets the level name as a std::u16string.
     * @param nLevel The severity level to convert.
     * @return A UTF-16 string containing the name.
     */
    inline std::u16string u16getName(const Level nLevel) {
        return Detail::LevelName<char16_t>::getName(nLevel);
    }

    /**
     * @brief Gets the level name as a std::u32string.
     * @param nLevel The severity level to convert.
     * @return A UTF-32 string containing the name.
     */
    inline std::u32string u32getName(const Level nLevel) {
        return Detail::LevelName<char32_t>::getName(nLevel);
    }

    /**
     * @brief Template version for generic character types (allocating).
     * @tparam char_t The character type to use.
     * @param nLevel The severity level to convert.
     * @return A basic string of type char_t containing the name.
     */
    template<typename char_t>
    std::basic_string<char_t> tgetName(const Level nLevel) {
        return Detail::LevelName<char_t>::getName(nLevel);
    }

    /** @} */

    /** @name StringView Getters (Non-allocating)
     * High-performance functions that return views to static memory.
     * @{ */

    /**
     * @brief Gets a view of the level name.
     * @param nLevel The severity level to convert.
     * @return A string view containing the name.
     */
    inline std::string_view getNameView(const Level nLevel) {
        return Detail::LevelName<char>::getNameView(nLevel);
    }

#if __cplusplus >= 202002L
    /**
     * @brief Gets a UTF-8 view of the level name.
     * @param nLevel The severity level to convert.
     * @return A UTF-8 string view containing the name.
     */
    inline std::u8string_view u8getNameView(const Level nLevel) {
        return Detail::LevelName<char8_t>::getNameView(nLevel);
    }
#endif

    /**
     * @brief Gets a UTF-16 view of the level name.
     * @param nLevel The severity level to convert.
     * @return A UTF-16 string view containing the name.
     */
    inline std::u16string_view u16getNameView(const Level nLevel) {
        return Detail::LevelName<char16_t>::getNameView(nLevel);
    }

    /**
     * @brief Gets a UTF-32 view of the level name.
     * @param nLevel The severity level to convert.
     * @return A UTF-32 string view containing the name.
     */
    inline std::u32string_view u32getNameView(const Level nLevel) {
        return Detail::LevelName<char32_t>::getNameView(nLevel);
    }

    /**
     * @brief Template version for generic character types (non-allocating).
     * @tparam char_t The character type to use.
     * @param nLevel The severity level to convert.
     * @return A basic string view of type char_t containing the name.
     */
    template<typename char_t>
    std::basic_string_view<char_t> tgetNameView(const Level nLevel) {
        return Detail::LevelName<char_t>::getNameView(nLevel);
    }

    /** @} */
}

#endif
