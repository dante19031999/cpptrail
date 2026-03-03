/**
 * @file sync_logger.h
 * @brief Synchronous logger implementation base.
 * @details Defines the BasicSyncLoggerImpl template, which provides a thread-safe,
 * mutex-protected foundation for loggers that execute I/O operations directly
 * on the calling thread.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */
#ifndef CPPTRAIL_SYNC_LOGGER_H
#define CPPTRAIL_SYNC_LOGGER_H

#include <mutex>

#include "cpptrail/base_logger.h"

namespace CppTrail {
    /**
     * @class BasicSyncLoggerImpl
     * @brief A synchronous, thread-safe logger implementation template.
     * This class ensures that every log call is processed immediately on the calling thread.
     * Access to the 'work' function is protected by a mutex, making it safe for
     * multi-threaded environments, though it may introduce latency in the calling thread.
     * @tparam char_t The character type (e.g., char, char8_t) used for the log content.
     * @warning Ensure that derived classes are stopped before destruction
     * completes to prevent calls to pure virtual methods.
     */
    template<typename char_t>
    class BasicSyncLoggerImpl : public BasicLoggerImpl<char_t> {
    public:
        /// @brief Alias for the underlying character type used by this logger.
        using char_type = typename BasicLoggerImpl<char_t>::char_type;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = typename BasicLoggerImpl<char_t>::string_type;

        /// @brief Alias for the BasicMessage type associated to the logger.
        using message_type = typename BasicLoggerImpl<char_t>::message_type;

    protected:
        /**
         * @brief Destructor.
         * @warning Ensure that derived classes are stopped before destruction
         * completes to prevent calls to pure virtual methods.
         */
        ~BasicSyncLoggerImpl() override = default;

    protected:
        /**
         * @brief Abstract method where the actual logging I/O occurs.
         * @details This is called while the general mutex is held.
         * @param oMessage The message to be recorded.
         */
        virtual void work(message_type oMessage) = 0;

    public:
        /**
          * @brief Synchronously processes and records a log message.
          * @details This method is the primary entry point for synchronous logging. It
          * performs a thread-safe check of the service status before invoking the
          * internal work() method.
          * @param oMessage The message implementation to be recorded. This object is
          * moved into the processing pipeline to avoid unnecessary copies.
          * @note **Blocking Behavior:** This call blocks the execution of the calling
          * thread until the I/O operation (work()) is completed.
          * @note **Status Check:** Messages are only processed if the logger status is
          * Status::RUNNING or Status::TRASCENDENT. In any other state, the message
          * is safely discarded.
          */
        void log(message_type oMessage) final {
            std::unique_lock<std::mutex> oLock(this->m_oGeneralMutex);
            auto nStatus = this->serviceStatus();
            if (nStatus == Status::TRASCENDENT || nStatus == Status::RUNNING)
                this->work(std::move(oMessage));
        }

    public:
        /**
         * @brief Starts the logger service.
         * @details Blocks until the internal serviceStart() completes and the status is RUNNING.
         */
        void start() final {
            std::unique_lock<std::mutex> oLock(this->m_oGeneralMutex);
            this->serviceStart();
        }

        /**
         * @brief Stops the logger service.
         * @details Blocks until the internal serviceStop() completes and the status is STOPPED.
         */
        void stop() final {
            std::unique_lock<std::mutex> oLock(this->m_oGeneralMutex);
            this->serviceStop();
        }

        /**
         * @brief Signals the logger to stop.
         * @details For synchronous loggers, this is functionally equivalent to stop()
         * as there is no background queue to drain.
         */
        void signalStop() final {
            std::unique_lock<std::mutex> oLock(this->m_oGeneralMutex);
            this->serviceStop();
        }

        /**
         * @brief Waits for the asynchronous end of the shutdown sequence.
         * @details This function is used in conjunction with signalStop() to ensure description safety.
         */
        void join() override {
        }

        /**
         * @brief Retrieves the current status of the service.
         * @return The current Status as reported by the underlying service.
         */
        Status status() final {
            std::unique_lock<std::mutex> oLock(this->m_oGeneralMutex);
            return this->serviceStatus();
        }

    protected:
        /** @name Internal Service Interface
         * These methods must be implemented by derived classes to handle
         * the specific backend lifecycle without worrying about locking.
         * @{ */

        /** @brief Non-locking call to get the current service status. */
        virtual Status serviceStatus() = 0;

        /** @brief Non-locking call to initiate service startup. */
        virtual void serviceStart() = 0;

        /** @brief Non-locking call to initiate service shutdown. */
        virtual void serviceStop() = 0;

        /** @} */

    private:
        /** @brief Mutex protecting the state and the synchronous work() calls. */
        std::mutex m_oGeneralMutex;
    };

    /** @name Logger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based logger (ASCII/UTF-8).
    using SyncLoggerImpl = BasicSyncLoggerImpl<char>;

    /// @brief Wide-character logger (Platform dependent, typically UTF-16 on Windows).
    using wSyncLoggerImpl = BasicSyncLoggerImpl<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 logger using C++20 char8_t.
    using u8SyncLoggerImpl = BasicSyncLoggerImpl<char8_t>;
#endif

    /// @brief UTF-16 logger using char16_t.
    using u16SyncLoggerImpl = BasicSyncLoggerImpl<char16_t>;

    /// @brief UTF-32 logger using char32_t.
    using u32SyncLoggerImpl = BasicSyncLoggerImpl<char32_t>;

    /** @} */
}

#endif
