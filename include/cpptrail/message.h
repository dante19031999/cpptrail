/**
 * @file message.h
 * @brief Log message definitions.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_MESSAGE_H
#define CPPTRAIL_MESSAGE_H

#include <memory>
#include <string>
#include <exception>

#if __cplusplus >= 201703L
#include <string_view>
#endif

#include "cpptrail/def.h"

namespace CppTrail {
    /**
     * @brief Abstract base class for message implementations.
     * @tparam char_t The character type (char, char8_t, char16_t, char32_t).
     */
    template<typename char_t>
    class BasicMessageImpl {
    public:
        /// @brief Alias for the underlying character type.
        using char_type = char_t;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = std::basic_string<char_type>;

    public:
        virtual ~BasicMessageImpl() = default;

    public:
        /**
         * @brief Returns the string message to log.
         * @return The message as a string.
         * @note The implementations enters a non defined state after this call.
         */
        virtual string_type stealString() = 0;
    };

    /**
     * @name BasicMessageImpl Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based message (ASCII/UTF-8).
    using MessageImpl = BasicMessageImpl<char>;

    /// @brief Wide-character message (Platform dependent, typically UTF-16 on Windows).
    using wMessageImpl = BasicMessageImpl<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 message using C++20 char8_t.
    using u8MessageImpl = BasicMessageImpl<char8_t>;
#endif

    /// @brief UTF-16 message using char16_t.
    using u16MessageImpl = BasicMessageImpl<char16_t>;

    /// @brief UTF-32 message using char32_t.
    using u32MessageImpl = BasicMessageImpl<char32_t>;

    /** @} */

    /**
     * @brief Container for log message implementations.
     * @tparam char_t The character type.
     */
    template<typename char_t>
    class BasicMessage {
    public:
        /// @brief Alias for the implementation type.
        using impl_type = BasicMessageImpl<char_t>;

        /// @brief Alias for the underlying character type.
        using char_type = char_t;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = std::basic_string<char_type>;

    protected:
        /**
          * @brief Internal constructor for derived message types.
          * @param sLevel The string representation of the log level (e.g., "INFO", "ERROR").
          * @param pImpl Shared pointer to the concrete implementation that manages the message data.
          */
        explicit BasicMessage(
            string_type sLevel,
            std::shared_ptr<impl_type> pImpl
        ) : m_sLevel(std::move(sLevel)),
            m_pImpl(std::move(pImpl)) {
        }

    public:
        /**
         * @brief Default constructor.
         */
        BasicMessage() = default;

        /**
         * @brief Returns the string message to log.
         * @return The message string.
         * @note The implementations enters a non defined state.
         */
        string_type stealString() {
            if (!this->m_pImpl) return string_type{};
            return this->m_pImpl->stealString();
        }

        /**
         * @brief Extracts the string representation of the log level.
         * @return The log level name as a string.
         * @note This operation uses move semantics; the internal level string will be empty after this call.
         */
        string_type stealLevel() noexcept {
            return std::move(this->m_sLevel);
        }

        /**
         * @brief Retrieves a weak pointer to the internal implementation.
         * @return A weak_ptr to the implementation.
         */
        std::weak_ptr<impl_type> getImplementation() noexcept {
            return this->m_pImpl;
        }

    private:
        /// The message log level.
        string_type m_sLevel;
        /// Message implementation
        std::shared_ptr<impl_type> m_pImpl;
    };

    /**
     * @name BasicMessage Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based message (ASCII/UTF-8).
    using Message = BasicMessage<char>;

    /// @brief Wide-character message (Platform dependent, typically UTF-16 on Windows).
    using wMessage = BasicMessage<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 message using C++20 char8_t.
    using u8Message = BasicMessage<char8_t>;
#endif

    /// @brief UTF-16 message using char16_t.
    using u16Message = BasicMessage<char16_t>;

    /// @brief UTF-32 message using char32_t.
    using u32Message = BasicMessage<char32_t>;

    /** @} */

    /**
     * @brief A standard string-based log message.
     * @tparam char_t The character type.
     */
    template<typename char_t>
    class BasicStringMessage : public BasicMessage<char_t> {
    public:
        /// @brief Alias for the implementation type.
        using impl_type = BasicMessageImpl<char_t>;

        /// @brief Alias for the underlying character type.
        using char_type = char_t;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = std::basic_string<char_type>;

#if __cplusplus >= 201703L
        /// @brief Alias for the basic_string_view type associated with this logger's encoding.
        using string_view_type = std::basic_string_view<char_type>;
#endif

    public:
        /**
          * @brief Constructs a message from a string and a level.
          * @param sLevel The string representation of the log level.
          * @param sMessage The message content to be stored.
          */
        BasicStringMessage(
            string_type sLevel,
            string_type sMessage
        ) : BasicMessage<char_t>(
            std::move(sLevel),
            std::make_shared<Impl>(std::move(sMessage))
        ) {
        }

#if __cplusplus >= 201703L
        /**
         * @brief Constructs a message from a string view and a level.
         * @param sLevel The string representation of the log level.
         * @param sMessage The string view content to copy into the message.
         */
        BasicStringMessage(
            string_type sLevel,
            string_view_type sMessage
        ) : BasicMessage<char_t>(
            std::move(sLevel),
            std::make_shared<Impl>(string_type{sMessage})
        ) {
        }
#endif

        /**
         * @brief Constructs a message from a standard exception and a level.
         * @tparam T Internal type for SFINAE check.
         * @param sLevel The string representation of the log level.
         * @param oMessage The exception object to extract the message from.
         * @note This constructor is only available for char-based messages.
         */
        template<typename T = char_t,
            typename std::enable_if<std::is_same<T, char>::value, int>::type = 0>
        BasicStringMessage(
            string_type sLevel,
            const std::exception &oMessage
        ) : BasicMessage<char_t>(
            std::move(sLevel),
            std::make_shared<Impl>(oMessage.what())
        ) {
        }

    private:
        /**
         * @brief Internal implementation that holds the physical string data.
         */
        class Impl : public BasicMessageImpl<char_t> {
        public:
            /**
             * @brief Explicit constructor for the string storage.
             * @param sString The string to store.
             */
            explicit Impl(string_type sString)
                : m_sString(std::move(sString)) {
            }

        public:
            /**
             * @brief Implementation of stealString.
             * @return The internal string via move semantics.
             */
            string_type stealString() override {
                return std::move(this->m_sString);
            }

        private:
            /// The stored message string.
            string_type m_sString;
        };
    };

    /**
     * @name BasicStringMessage Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based string message (ASCII/UTF-8).
    using StringMessage = BasicStringMessage<char>;

    /// @brief Wide-character string message (Platform dependent, typically UTF-16 on Windows).
    using wStringMessage = BasicStringMessage<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 string message using C++20 char8_t.
    using u8StringMessage = BasicStringMessage<char8_t>;
#endif

    /// @brief UTF-16 string message using char16_t.
    using u16StringMessage = BasicStringMessage<char16_t>;

    /// @brief UTF-32 string message using char32_t.
    using u32StringMessage = BasicStringMessage<char32_t>;

    /** @} */
}

#endif
