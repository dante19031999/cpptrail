/**
 * @file logger.h
 * @brief High-level wrapper for the CppTrail logging system.
 * @details This file provides the BasicLogger handle, which serves as the
 * primary user-facing interface for interacting with various BasicLoggerImpl
 * backends.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_LOGGER_H
#define CPPTRAIL_LOGGER_H

#include "cpptrail/logger_impl.h"

namespace CppTrail {
    /**
     * @class BasicLogger
     * @brief A high-level handle for logging operations.
     * @details BasicLogger uses a pImpl-like approach via std::shared_ptr to
     * manage a BasicLoggerImpl. It provides a flexible API including
     * method chaining, exception handling, and operator overloading.
     * @tparam char_t The character type (e.g., char, char8_t) used for logging.
     * @warning All information needs to be managed by the underlying BasicLoggerImpl.
     * Any form of extension over this class must be immune to object splicing.
     */
    template<typename char_t>
    class BasicLogger {
    public:
        /** @brief Prevents construction from a null pointer. */
        BasicLogger(nullptr_t) = delete;

    protected:
        /**
         * @brief Protected constructor for derived factories.
         * @param pLogger A shared pointer to a concrete implementation.
         */
        BasicLogger(std::shared_ptr<BasicLoggerImpl<char_t> > pLogger)
            : m_pLogger(std::move(pLogger)) {
        }

    public:
        /// @brief Copy constructor (defaults to shared ownership).
        BasicLogger(const BasicLogger &pLogger) = default;

        /// @brief Move constructor.
        BasicLogger(BasicLogger &&pLogger) = default;

        /// @brief Assignment operator.
        BasicLogger &operator=(const BasicLogger &pLogger) = default;

        /// @brief Assignment operator.
        BasicLogger &operator=(BasicLogger &&pLogger) = default;

    public:
        /// @brief Alias for the underlying character type used by this logger.
        using char_type = typename BasicLoggerImpl<char_t>::char_type;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = typename BasicLoggerImpl<char_t>::string_type;

        /// @brief Alias for the basic_string_view type associated with this logger's encoding.
        using string_view_type = typename BasicLoggerImpl<char_t>::string_view_type;

    public:
        /**
         * @brief Starts the underlying logging service.
         * @details Depending on the implementation, this may initialize resources,
         * open files, or spawn background worker threads.
         */
        void start() {
            this->m_pLogger->start();
        }

        /**
         * @brief Performs a synchronous shutdown of the logger.
         * @details This method triggers the stop sequence and waits until the
         * service has fully terminated and all resources are released.
         */
        void stop() {
            this->m_pLogger->stop();
        }

        /**
         * @brief Initiates an asynchronous shutdown request.
         * @details Signals the logger to begin closing, allowing the calling
         * thread to continue execution immediately. Use join() later to
         * ensure completion.
         */
        void signalStop() {
            this->m_pLogger->signalStop();
        }

        /**
         * @brief Blocks until the asynchronous shutdown sequence is complete.
         * @details This method safely checks if the underlying implementation
         * is an asynchronous type via dynamic_cast. If it is, it waits for
         * background threads to finish. For synchronous loggers, this is a no-op.
         */
        void join() {
            auto pLogger = dynamic_cast<BasicAsyncLoggerImpl<char_t> *>(this->m_pLogger.get());
            if (pLogger != nullptr)
                pLogger->join();
        }

        /**
         * @brief Retrieves the current operational status of the logger.
         * @return The current Status (e.g., STARTING, RUNNING, STOPPED, etc.).
         */
        [[nodiscard]] Status status() {
            return this->m_pLogger->status();
        }

    public:
        /**
         * @brief Logs a message with a predefined severity level.
         * @param nLevel The log level (e.g., Level::INFO).
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const Level nLevel, string_type sMessage) {
            this->m_pLogger->log(tgetName<char_t>(nLevel), std::move(sMessage));
            return *this;
        }

        /**
         * @brief Logs a message with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(string_type sLevel, string_type sMessage) {
            this->m_pLogger->log(std::move(sLevel), std::move(sMessage));
            return *this;
        }

        /**
         * @brief Logs a message with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const string_view_type sLevel, string_type sMessage) {
            this->m_pLogger->log(string_type{sLevel}, std::move(sMessage));
            return *this;
        }

        /**
         * @brief Logs an exception with a specific severity level.
         * @param nLevel The log level.
         * @param sMessage The exception object to extract the message from.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const Level nLevel, const std::exception &sMessage) {
            this->m_pLogger->log(tgetName<char_t>(nLevel), sMessage.what());
            return *this;
        }

        /**
         * @brief Logs an exception with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The exception object to extract the message from.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(string_type sLevel, const std::exception &sMessage) {
            this->m_pLogger->log(std::move(sLevel), sMessage.what());
            return *this;
        }

        /**
         * @brief Logs an exception with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The exception object to extract the message from.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const string_view_type sLevel, const std::exception &sMessage) {
            this->m_pLogger->log(string_type{sLevel}, sMessage.what());
            return *this;
        }

        /**
         * @brief Logs an exception using Level::ERROR by default.
         * @param sMessage The exception object to extract the message from.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const std::exception &sMessage) {
            this->log(Level::ERROR, sMessage.what());
            return *this;
        }

    public:
        /**
         * @brief Functor overload for logging with a predefined level.
         * @param nLevel The log level (e.g., Level::INFO).
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(const Level nLevel, string_type sMessage) {
            this->m_pLogger->log(tgetName<char_t>(nLevel), std::move(sMessage));
            return *this;
        }

        /**
         * @brief Functor overload for logging with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(string_type sLevel, string_type sMessage) {
            this->m_pLogger->log(std::move(sLevel), std::move(sMessage));
            return *this;
        }

        /**
         * @brief Functor overload for logging with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The message content.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(const string_view_type sLevel, string_type sMessage) {
            this->m_pLogger->log(string_type{sLevel}, std::move(sMessage));
            return *this;
        }

        /**
         * @brief Functor overload for logging an exception with a specific level.
         * @param nLevel The log level.
         * @param sMessage The exception object.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(const Level nLevel, const std::exception &sMessage) {
            this->m_pLogger->log(tgetName<char_t>(nLevel), sMessage.what());
            return *this;
        }

        /**
         * @brief Functor overload for logging an exception with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The exception object.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(string_type sLevel, const std::exception &sMessage) {
            this->m_pLogger->log(std::move(sLevel), sMessage.what());
            return *this;
        }

        /**
         * @brief Functor overload for logging an exception with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The exception object.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(const string_view_type sLevel, const std::exception &sMessage) {
            this->m_pLogger->log(string_type{sLevel}, sMessage.what());
            return *this;
        }

        /**
         * @brief Functor overload for logging an exception with Level::ERROR.
         * @param sMessage The exception object.
         * @return A reference to this logger to allow method chaining.
         */
        BasicLogger &operator()(const std::exception &sMessage) {
            this->log(Level::ERROR, sMessage.what());
            return *this;
        }

    private:
        /** @brief Shared pointer to the thread-safe implementation. */
        std::shared_ptr<BasicLoggerImpl<char_t> > m_pLogger;
    };

    /**
     * @name Logger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based logger (ASCII/UTF-8).
    using Logger = BasicLogger<char>;

    /// @brief Wide-character logger (Platform dependent, typically UTF-16 on Windows).
    using wLogger = BasicLogger<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 logger using C++20 char8_t.
    using u8Logger = BasicLogger<char8_t>;
#endif

    /// @brief UTF-16 logger using char16_t.
    using u16Logger = BasicLogger<char16_t>;

    /// @brief UTF-32 logger using char32_t.
    using u32Logger = BasicLogger<char32_t>;

    /** @} */
}

#endif
