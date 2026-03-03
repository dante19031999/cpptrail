/**
 * @file base_logger.h
 * @brief Core logging interfaces and high-level handle abstractions.
 * @details This header defines the structural backbone of CppTrail. It provides
 * the BasicLoggerImpl abstract interface for concrete logger implementations
 * and the BasicLogger handle class which utilizes the Bridge pattern to provide
 * a thread-safe, RAII-compliant API for end-users.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */
#ifndef CPPTRAIL_BASE_LOGGER_H
#define CPPTRAIL_BASE_LOGGER_H

#include "cpptrail/message.h"

namespace CppTrail {
    /**
     * @class BasicLoggerImpl
     * @brief Abstract template interface defining the mandatory contract for all loggers.
     * @details This template serves as the foundation for both synchronous and asynchronous
     * implementations. It abstracts the character type to allow native support for multiple
     * encodings (UTF-8, UTF-16, etc.) across different logging sinks.
     * @tparam char_t The character type (e.g., char, char8_t, wchar_t) used for log data.
     * @note This base interface does not provide internal synchronization; thread-safety
     * depends on the specific derived implementation (e.g., AsyncLoggerImpl).
     */
    template<typename char_t>
    class BasicLoggerImpl {
    public:
        /// @brief Alias for the underlying character type used by this logger.
        using char_type = char_t;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = std::basic_string<char_type>;

        /// @brief Alias for the BasicMessage type associated to the logger.
        using message_type = BasicMessage<char_type>;

#if __cplusplus >= 201703L
        /// @brief Alias for the basic_string_view type associated with this logger's encoding.
        using string_view_type = std::basic_string_view<char_type>;
#endif

    public:
        /** @brief Virtual destructor to ensure proper cleanup of derived logger resources. */
        virtual ~BasicLoggerImpl() = default;

        /**
         * @brief Primary interface for submitting a log record.
         * @param oMessage The message to be processed.
         */
        virtual void log(message_type oMessage) = 0;

        /**
         * @brief Synchronously starts the logger service.
         * @details This call blocks the calling thread until the logger's internal status reaches Status::RUNNING.
         */
        virtual void start() = 0;

        /**
         * @brief Synchronously stops the logger service.
         * @details This call blocks until all workers are joined and the status reaches Status::STOPPED.
         * Implementation-defined behavior determines if the remaining queue is flushed before exit.
         */
        virtual void stop() = 0;

        /**
         * @brief Asynchronously signals the logger to initiate a shutdown sequence.
         * @details Unlike stop(), this returns immediately, allowing the logger to shut down
         * in the background (typically via a dedicated "killer" thread).
         */
        virtual void signalStop() = 0;

        /**
         * @brief Waits for the asynchronous end of the shutdown sequence.
         * @details This function is used in conjunction with signalStop() to ensure description safety.
         */
        virtual void join() = 0;

        /**
         * @brief Retrieves the current operational health and state of the logger.
         * @return The current Status (RUNNING, STOPPED, BROKEN, or TRASCENDENT).
         */
        virtual Status status() = 0;
    };

    /** @name Logger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based logger (ASCII/UTF-8).
    using LoggerImpl = BasicLoggerImpl<char>;

    /// @brief Wide-character logger (Platform dependent, typically UTF-16 on Windows).
    using wLoggerImpl = BasicLoggerImpl<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 logger using C++20 char8_t.
    using u8LoggerImpl = BasicLoggerImpl<char8_t>;
#endif

    /// @brief UTF-16 logger using char16_t.
    using u16LoggerImpl = BasicLoggerImpl<char16_t>;

    /// @brief UTF-32 logger using char32_t.
    using u32LoggerImpl = BasicLoggerImpl<char32_t>;

    /** @} */

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

        /**
         * @brief Destructor that ensures a graceful shutdown of the logger.
         * @details BasicLogger follows an RAII (Resource Acquisition Is Initialization) pattern.
         * If this specific instance is the last remaining handle (use_count == 1) to the
         * underlying implementation, it will automatically call stop() and join()
         * to ensure all pending logs are processed and resources are freed before
         * the implementation is destroyed.
         * @note If other copies of this BasicLogger exist, the destructor will simply
         * decrement the shared reference count without stopping the service.
         */
        ~BasicLogger() {
            if (this->m_pLogger && this->m_pLogger.use_count() == 1) {
                this->m_pLogger->stop();
                this->m_pLogger->join();
            }
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

        /// @brief Alias for the BasicMessage type associated to the logger.
        using message_type = typename BasicLoggerImpl<char_t>::message_type;

#if __cplusplus >= 201703L
        /// @brief Alias for the basic_string_view type associated with this logger's encoding.
        using string_view_type = typename BasicLoggerImpl<char_t>::string_view_type;
#endif

    public:
        /**
         * @brief Starts the underlying logging service.
         * @details Depending on the implementation, this may initialize resources,
         * open files, or spawn background worker threads.
         */
        void start() {
            if (this->m_pLogger) this->m_pLogger->start();
        }

        /**
         * @brief Performs a synchronous shutdown of the logger.
         * @details This method triggers the stop sequence and waits until the
         * service has fully terminated and all resources are released.
         */
        void stop() {
            if (this->m_pLogger) this->m_pLogger->stop();
        }

        /**
         * @brief Initiates an asynchronous shutdown request.
         * @details Signals the logger to begin closing, allowing the calling
         * thread to continue execution immediately. Use join() later to
         * ensure completion.
         */
        void signalStop() {
            if (this->m_pLogger) this->m_pLogger->signalStop();
        }

        /**
         * @brief Blocks until the asynchronous shutdown sequence is complete.
         * @details This method safely checks if the underlying implementation
         * is an asynchronous type via dynamic_cast. If it is, it waits for
         * background threads to finish. For synchronous loggers, this is a no-op.
         */
        void join() {
            if (this->m_pLogger) this->m_pLogger->join();
        }

        /**
         * @brief Retrieves the current operational status of the logger.
         * @return The current Status (e.g., STARTING, RUNNING, STOPPED, etc.).
         */
        [[nodiscard]] Status status() {
            if (this->m_pLogger) return this->m_pLogger->status();
        }

        /** @name Primary Logging Interface
           * @{ */

    public:
        /**
         * @brief Submits a pre-constructed message object to the logger.
         * @param oMessage The message implementation to be processed.
         * @return BasicLogger& A reference to this logger to allow method chaining.
         */
        BasicLogger &log(message_type oMessage) {
            if (this->m_pLogger) this->m_pLogger->log(std::move(oMessage));
            return *this;
        }

        /**
         * @brief Logs a message with a specific severity level.
         * @param nLevel The severity level (from the Level enum).
         * @param sMessage The string content of the log.
         * @return BasicLogger& A reference to this logger to allow method chaining.
         */
        BasicLogger &log(const Level nLevel, string_type sMessage) {
            return this->log(BasicStringMessage<char_t>(
                tgetName<char_t>(nLevel), std::move(sMessage)
            ));
        }

        /**
         * @brief Logs a message with a custom string-based level.
         * @param sLevel A custom string to represent the log category or level.
         * @param sMessage The string content of the log.
         * @return BasicLogger& A reference to this logger to allow method chaining.
         */
        BasicLogger &log(string_type sLevel, string_type sMessage) {
            return this->log(BasicStringMessage<char_t>(
                std::move(sLevel), std::move(sMessage)
            ));
        }

        /** @} */

    public:
        /** @name Convenience Severity Wrappers
         * @{ */

        /**
         * @brief Submits a SUCCESS level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &success(string_type sMessage) { return this->log(Level::SUCCESS, std::move(sMessage)); }

        /**
         * @brief Submits a TRACE level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &trace(string_type sMessage) { return this->log(Level::TRACE, std::move(sMessage)); }

        /**
         * @brief Submits a DEBUG level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &debug(string_type sMessage) { return this->log(Level::DEBUG, std::move(sMessage)); }

        /**
         * @brief Submits an INFO level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &info(string_type sMessage) { return this->log(Level::INFO, std::move(sMessage)); }

        /**
         * @brief Submits a MESSAGE level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &message(string_type sMessage) { return this->log(Level::MESSAGE, std::move(sMessage)); }

        /**
         * @brief Submits a WARNING level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &warning(string_type sMessage) { return this->log(Level::WARNING, std::move(sMessage)); }

        /**
         * @brief Submits an ERROR level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &error(string_type sMessage) { return this->log(Level::ERROR, std::move(sMessage)); }

        /**
         * @brief Submits a CRITICAL level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &critical(string_type sMessage) { return this->log(Level::CRITICAL, std::move(sMessage)); }

        /**
         * @brief Submits a FATAL level log message.
         * @param sMessage The message content.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &fatal(string_type sMessage) { return this->log(Level::FATAL, std::move(sMessage)); }
        /** @} */

#if __cplusplus >= 201703L
        /** @name String View Overloads (C++17)
         * @{ */

    public:
        /**
         * @brief Logs a message view with a specific severity level.
         * @param nLevel The severity level.
         * @param sMessage The string view to log.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &log(const Level nLevel, string_view_type sMessage) {
            return this->log(BasicStringMessage<char_t>(tgetName<char_t>(nLevel), sMessage));
        }

        /**
         * @brief Logs a message view with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The string view to log.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &log(string_type sLevel, string_view_type sMessage) {
            return this->log(BasicStringMessage<char_t>(std::move(sLevel), sMessage));
        }

    public:
        /** @name Convenience Severity Wrappers
        * @{ */
        /** @brief Success wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &success(string_view_type sMsg) { return this->log(Level::SUCCESS, sMsg); }
        /** @brief Trace wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &trace(string_view_type sMsg) { return this->log(Level::TRACE, sMsg); }
        /** @brief Debug wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &debug(string_view_type sMsg) { return this->log(Level::DEBUG, sMsg); }
        /** @brief Info wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &info(string_view_type sMsg) { return this->log(Level::INFO, sMsg); }
        /** @brief Message wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &message(string_view_type sMsg) { return this->log(Level::MESSAGE, sMsg); }
        /** @brief Warning wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &warning(string_view_type sMsg) { return this->log(Level::WARNING, sMsg); }
        /** @brief Error wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &error(string_view_type sMsg) { return this->log(Level::ERROR, sMsg); }
        /** @brief Critical wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &critical(string_view_type sMsg) { return this->log(Level::CRITICAL, sMsg); }
        /** @brief Fatal wrapper for string views. @param sMsg Message view. @return Logger reference. */
        BasicLogger &fatal(string_view_type sMsg) { return this->log(Level::FATAL, sMsg); }
        /** @} */

        /** @} */

        /** @name String View Overloads (C++17)
        * @{ */

    public:
        /**
         * @brief Logs a message view with a specific severity level.
         * @param nLevel The severity level.
         * @param sMessage The string litteral to log.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &log(const Level nLevel, const char_t *sMessage) {
            return this->log(BasicStringMessage<char_t>(tgetName<char_t>(nLevel), string_type{sMessage}));
        }

        /**
         * @brief Logs a message view with a custom string level.
         * @param sLevel The custom level name.
         * @param sMessage The string litteral to log.
         * @return BasicLogger& A reference to this logger.
         */
        BasicLogger &log(string_type sLevel, const char_t *sMessage) {
            return this->log(BasicStringMessage<char_t>(std::move(sLevel), string_type{sMessage}));
        }

    public:
        /** @name Convenience Severity Wrappers
        * @{ */
        /** @brief Success wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &success(const char_t *sMsg) { return this->log(Level::SUCCESS, sMsg); }
        /** @brief Trace wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &trace(const char_t *sMsg) { return this->log(Level::TRACE, sMsg); }
        /** @brief Debug wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &debug(const char_t *sMsg) { return this->log(Level::DEBUG, sMsg); }
        /** @brief Info wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &info(const char_t *sMsg) { return this->log(Level::INFO, sMsg); }
        /** @brief Message wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &message(const char_t *sMsg) { return this->log(Level::MESSAGE, sMsg); }
        /** @brief Warning wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &warning(const char_t *sMsg) { return this->log(Level::WARNING, sMsg); }
        /** @brief Error wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &error(const char_t *sMsg) { return this->log(Level::ERROR, sMsg); }
        /** @brief Critical wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &critical(const char_t *sMsg) { return this->log(Level::CRITICAL, sMsg); }
        /** @brief Fatal wrapper for string litterals. @param sMsg Message view. @return Logger reference. */
        BasicLogger &fatal(const char_t *sMsg) { return this->log(Level::FATAL, sMsg); }
        /** @} */

        /** @} */


#endif

    public:
        /** @name Exception Overloads
         * @{ */

        /**
         * @brief Logs an exception with the default ERROR level.
         * @tparam T Internal type for SFINAE check.
         * @param oException The exception object.
         * @return BasicLogger& A reference to this logger.
         * @note Only available for char-based loggers.
         */
        template<typename T = char_t, typename std::enable_if<std::is_same<T, char>::value, int>::type = 0>
        BasicLogger &log(const std::exception &oException) {
            return this->log(BasicStringMessage<char_t>(tgetName<char_t>(Level::ERROR), oException));
        }

        /**
         * @brief Logs an exception with a specific severity level.
         * @tparam T Internal type for SFINAE check.
         * @param nLevel The severity level.
         * @param oException The exception object.
         * @return BasicLogger& A reference to this logger.
         * @note Only available for char-based loggers.
         */
        template<typename T = char_t, typename std::enable_if<std::is_same<T, char>::value, int>::type = 0>
        BasicLogger &log(const Level nLevel, const std::exception &oException) {
            return this->log(BasicStringMessage<char_t>(tgetName<char_t>(nLevel), oException));
        }

        /**
         * @brief Logs an exception with a custom string level.
         * @tparam T Internal type for SFINAE check.
         * @param sLevel The custom level name.
         * @param oException The exception object.
         * @return BasicLogger& A reference to this logger.
         * @note Only available for char-based loggers.
         */
        template<typename T = char_t, typename std::enable_if<std::is_same<T, char>::value, int>::type = 0>
        BasicLogger &log(string_type sLevel, const std::exception &oException) {
            return this->log(BasicStringMessage<char_t>(std::move(sLevel), oException));
        }

        /** @} */

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
