/**
 * @file logger_impl.h
 * @brief Core abstractions and threading models for the CppTrail logging framework.
 * @details Provides the base interfaces (Synchronous, Asynchronous, and Trascendent)
 * for logging sinks, featuring an optimized asynchronous producer-consumer
 * model with object pooling to minimize heap allocations during high-throughput
 * "wire stuff" testing.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_LOGGER_IMPL_H
#define CPPTRAIL_LOGGER_IMPL_H

#include <condition_variable>
#include <stdexcept>
#include <string>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "cpptrail/def.h"

namespace CppTrail {
    /**
     * @class LoggerOverflowError
     * @brief Exception thrown when the logger queue exceeds its maximum allowed size.
     * This exception is specifically utilized by AsyncLoggerImpl when
     * m_bThrowOnOverflow is set to true and the incoming log rate exceeds
     * the processing capacity of the worker threads.
     */
    class LoggerOverflowError : public std::runtime_error {
    public:
        /**
         * @brief Constructs a new LoggerOverflowError object.
         */
        LoggerOverflowError()
            : std::runtime_error("LoggerOverflowError") {
        }
    };

    namespace Detail {
        /**
         * @struct LoggerImplEntry
         * @brief A POD structure representing a single log record.
         * @tparam char_t The character type (e.g., char, char8_t) used for the log content.
         */
        template<typename char_t>
        struct LoggerImplEntry {
            /** @brief String representation of the log level (e.g., "INFO", "DEBUG"). */
            std::basic_string<char_t> sLevel;

            /** @brief The actual content of the log message. */
            std::basic_string<char_t> sMessage;
        };

        /**
         * @struct LoggerImplEntryListItem
         * @brief A node in the linked-list queue used by asynchronous logger implementations.
         * @details This structure facilitates the "Object Pool" pattern by allowing nodes
         * to be linked, unlinked, and moved to a backup pool without reallocating the
         * underlying LoggerImplEntry.
         * @tparam char_t The character type of the entry data.
         */
        template<typename char_t>
        struct LoggerImplEntryListItem {
            /** @brief Pointer to the next item in the linked-list queue. */
            LoggerImplEntryListItem *m_pNext;

            /** @brief The encapsulated log entry data. */
            LoggerImplEntry<char_t> m_oEntry;

            /**
             * @brief Constructs a list item node by moving provided string data.
             * @param sLevel The severity level string to move into the entry.
             * @param sMessage The log message string to move into the entry.
             */
            LoggerImplEntryListItem(
                std::basic_string<char_t> sLevel,
                std::basic_string<char_t> sMessage
            )
                : m_pNext(nullptr),
                  m_oEntry({std::move(sLevel), std::move(sMessage)}) {
            }
        };
    }

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

        /// @brief Alias for the basic_string_view type associated with this logger's encoding.
        using string_view_type = std::basic_string_view<char_type>;

    public:
        /** @brief Virtual destructor to ensure proper cleanup of derived logger resources. */
        virtual ~BasicLoggerImpl() = default;

        /**
         * @brief Primary interface for submitting a log record.
         * @param sLevel The string representation of the severity level.
         * @param sMessage The content of the log message to be processed.
         */
        virtual void log(string_type sLevel, string_type sMessage) = 0;

        /**
         * @brief Synchronously starts the logger service.
         * @details This call blocks the calling thread until the logger's internal status reaches Status::RUNNING.
         */
        virtual void start() = 0;

        /** @brief Synchronously stops the logger service.
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
     * @class BasicTrascendentLoggerImpl
     * @brief A stateless logger implementation for "always-on" services.
     * This class provides a "trascendent" lifecycle, meaning it exists independently
     * of the standard start/stop orchestration. It is designed for simple sinks
     * (like std::cout or a fixed memory buffer) where initialization and
     * shutdown procedures are unnecessary.
     * @tparam char_t The character type (e.g., char, char8_t) used for the log content.
     * @note All lifecycle methods are marked as final and perform no operations
     * to ensure consistency across all trascendent implementations.
     */
    template<typename char_t>
    class BasicTrascendentLoggerImpl : public BasicLoggerImpl<char_t> {
    public:
        /// @brief Alias for the underlying character type used by this logger.
        using char_type = typename BasicLoggerImpl<char_t>::char_type;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = typename BasicLoggerImpl<char_t>::string_type;

    public:
        /**
         * @brief Interface for logging a message.
         * @details Must be implemented by the derived class to define the specific output behavior.
         * @param sLevel The severity level of the log entry.
         * @param sMessage The content of the log message.
         */
        void log(string_type sLevel, string_type sMessage) override = 0;

    public:
        /**
         * @brief No-op implementation of start.
         * @details Trascendent loggers are considered active by default; no setup is required.
         */
        void start() final {
        }

        /**
         * @brief No-op implementation of stop.
         * @details Trascendent loggers do not require a formal shutdown sequence.
         */
        void stop() final {
        }

        /**
         * @brief No-op implementation of signalStop.
         * @details Since there is no background queue or thread pool, no signaling is needed.
         */
        void signalStop() final {
        }

        /**
         * @brief Returns the persistent status of the logger.
         * @return Always returns Status::TRASCENDENT.
         */
        Status status() final {
            return Status::TRASCENDENT;
        }
    };

    /** @name Logger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based logger (ASCII/UTF-8).
    using TrascendentLoggerImpl = BasicTrascendentLoggerImpl<char>;

    /// @brief Wide-character logger (Platform dependent, typically UTF-16 on Windows).
    using wTrascendentLoggerImpl = BasicTrascendentLoggerImpl<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 logger using C++20 char8_t.
    using u8TrascendentLoggerImpl = BasicTrascendentLoggerImpl<char8_t>;
#endif

    /// @brief UTF-16 logger using char16_t.
    using u16TrascendentLoggerImpl = BasicTrascendentLoggerImpl<char16_t>;

    /// @brief UTF-32 logger using char32_t.
    using u32TrascendentLoggerImpl = BasicTrascendentLoggerImpl<char32_t>;

    /** @} */

    /**
     * @class BasicSyncLoggerImpl
     * @brief A synchronous, thread-safe logger implementation template.
     * This class ensures that every log call is processed immediately on the calling thread.
     * Access to the 'work' function is protected by a mutex, making it safe for
     * multi-threaded environments, though it may introduce latency in the calling thread.
     * @tparam char_t The character type (e.g., char, char8_t) used for the log content.
     * @warning Ensure that your destructor calls stop()
     */
    template<typename char_t>
    class BasicSyncLoggerImpl : public BasicLoggerImpl<char_t> {
    public:
        /// @brief Alias for the underlying character type used by this logger.
        using char_type = typename BasicLoggerImpl<char_t>::char_type;

        /// @brief Alias for the basic_string type associated with this logger's encoding.
        using string_type = typename BasicLoggerImpl<char_t>::string_type;

    protected:
        /**
         * @brief Destructor.
         * @warning Ensure that your destructor calls stop()
         */
        ~BasicSyncLoggerImpl() override = default;

    protected:
        /**
         * @brief Abstract method where the actual logging I/O occurs.
         * @details This is called while the general mutex is held.
         * @param sLevel The severity level string.
         * @param sMessage The message content to be recorded.
         */
        virtual void work(string_type sLevel, string_type sMessage) = 0;

    public:
        /**
         * @brief Thread-safe entry point for logging.
         * @details Acquires a lock and calls work() immediately. This call blocks
         * until work() returns.
         * @param sLevel The severity level string.
         * @param sMessage The message content.
         */
        void log(string_type sLevel, string_type sMessage) final {
            std::unique_lock oLock(this->m_oGeneralMutex);
            this->work(std::move(sLevel), std::move(sMessage));
        }

    public:
        /**
         * @brief Starts the logger service.
         * @details Blocks until the internal serviceStart() completes and the status is RUNNING.
         */
        void start() final {
            std::unique_lock oLock(this->m_oGeneralMutex);
            this->serviceStart();
        }

        /**
         * @brief Stops the logger service.
         * @details Blocks until the internal serviceStop() completes and the status is STOPPED.
         */
        void stop() final {
            std::unique_lock oLock(this->m_oGeneralMutex);
            this->serviceStop();
        }

        /**
         * @brief Signals the logger to stop.
         * @details For synchronous loggers, this is functionally equivalent to stop()
         * as there is no background queue to drain.
         */
        void signalStop() final {
            std::unique_lock oLock(this->m_oGeneralMutex);
            this->serviceStop();
        }

        /**
         * @brief Retrieves the current status of the service.
         * @return The current Status as reported by the underlying service.
         */
        Status status() final {
            std::unique_lock oLock(this->m_oGeneralMutex);
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

    /**
     * @class BasicAsyncLoggerImpl
     * @brief An asynchronous logger template utilizing background worker threads and object recycling.
     * This class decouples the logging call from the actual I/O operation. It maintains
     * an internal queue of log entries and a pool of worker threads to process them.
     * To minimize heap pressure, it uses a backup pool (object pool) to recycle
     * linked-list nodes.
     * @tparam char_t The character type (e.g., char, char8_t) used for log content.
     * @warning Ensure that your destructor calls stop() and join()
     */
    template<typename char_t>
    class BasicAsyncLoggerImpl : public BasicLoggerImpl<char_t> {
    public:
        /** @brief Alias for the underlying character type. */
        using char_type = typename BasicLoggerImpl<char_t>::char_type;

        /** @brief Alias for the basic_string type associated with this logger's encoding. */
        using string_type = typename BasicLoggerImpl<char_t>::string_type;

        /** @brief Internal entry structure type. */
        using Entry = Detail::LoggerImplEntry<char_t>;

        /** @brief Internal queue node type. */
        using Item = Detail::LoggerImplEntryListItem<char_t>;

    protected:
        /**
         * @brief Default constructor.
         * @details Initializes a single worker thread and sets an effectively infinite queue limit.
         */
        BasicAsyncLoggerImpl() : BasicAsyncLoggerImpl(
            std::numeric_limits<std::size_t>::max(),
            true, 1, 16
        ) {
        }

        /**
         * @brief Specialized constructor for controlled resource management.
         * @param nMaxItemCount The maximum number of log entries allowed in the queue.
         * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
         * @param nWorkerCount The number of background threads to spawn for processing.
         * @param nMaxBackupSize The maximum number of log entries allowed in the backup.
         */
        BasicAsyncLoggerImpl(
            std::size_t nMaxItemCount,
            bool bThrowOnOverflow,
            std::size_t nWorkerCount,
            std::size_t nMaxBackupSize
        );

    protected:
        /**
         * @brief Destructor.
         * @warning Ensure that your destructor calls stop() and join()
         */
        ~BasicAsyncLoggerImpl() override = default;

    protected:
        /**
         * @brief Abstract method for the actual I/O work.
         * @details Called by worker threads outside of the queue lock.
         * @param sLevel The log level string.
         * @param sMessage The log message string.
         */
        virtual void work(string_type sLevel, string_type sMessage) = 0;

    public:
        /**
         * @brief Submits a log entry to the asynchronous queue.
         * @details This method is thread-safe and usually non-blocking unless
         * allocation/mutex contention occurs.
         * @param sLevel The severity level.
         * @param sMessage The message content.
         * @throws LoggerOverflowError If the queue is full and bThrowOnOverflow is true.
         */
        void log(string_type sLevel, string_type sMessage) final;

        /** @brief Starts the background worker pool and the underlying service. */
        void start() final;

        /** @brief Synchronously shuts down the worker pool and the service. */
        void stop() final;

        /** @brief Initiates a shutdown sequence on a separate background "killer" thread. */
        void signalStop() final;

        /**
         * @brief Retrieves the current service status.
         * @return The current Status of the logger.
         */
        Status status() final {
            std::lock_guard oLock(this->m_oGeneralMutex);
            return this->serviceStatus();
        }

    protected:
        /** @name Service Lifecycle Interface
         * Hooks for derived classes to manage the underlying sink (e.g., opening sockets).
         * @{ */

        /**
          * @brief Reports the current operational status of the underlying sink.
          * @return The current Status of the service.
          * @note If the sink is considered "always-on" (like std::cout), this
          * should return Status::TRASCENDENT.
          */
        virtual Status serviceStatus() = 0;

        /**
         * @brief Performs the initialization sequence for the sink.
         * @details This is called internally during the start() sequence.
         * It is the appropriate place to allocate memory, open files, or
         * establish network connections before the worker thread begins
         * processing the log queue.
         * @throw std::runtime_error or similar if the resource cannot be acquired.
         */
        virtual void serviceStart() = 0;

        /**
         * @brief Performs the shutdown sequence for the sink.
         * @details This is called internally during the stop() sequence, after
         * the worker thread has finished draining the log queue. It ensures
         * that resources are released gracefully (e.g., sending a TCP FIN or
         * closing a database transaction).
         */
        virtual void serviceStop() = 0;

        /** @} */

        /**
          * @brief Synchronously waits for any pending asynchronous shutdown to complete.
          * @details This method provides a joining point for callers who previously initiated
          * a shutdown via signalStop(). It works by attempting to acquire the internal
          * synchronization primitives; since these are held by the shutdown sequence
          * (including the background killer thread), this call will block until the
          * logger has reached a stable STOPPED state.
          * @note This is particularly useful for ensuring all "wire stuff" logs are
          * flushed and threads are joined before the application exits or the
          * logger object is destroyed.
          */
        void join() {
            // Acquiring these locks ensures that no start/stop/signalStop
            // operation is currently in progress.
            std::unique_lock oLock(this->m_oGeneralMutex);
            std::lock_guard oStateLock(this->m_oStateMutex);
        }

    private:
        std::mutex m_oGeneralMutex; ///< Guards service lifecycle and start/stop state.
        std::mutex m_oQueueMutex; ///< Guards the head, tail, and item count of the queue.
        std::mutex m_oStateMutex; ///< Guards the worker thread vector.
        std::condition_variable m_oCv; ///< Used to wake workers when the queue has items.
        bool m_bContinue; ///< Flag indicating if workers should keep running.
        std::optional<std::thread> m_oKillerThread; ///< Handle for the asynchronous shutdown thread.

        Item *m_pFirst; ///< Pointer to the first item in the queue.
        Item *m_pFinal; ///< Pointer to the last item in the queue.
        Item *m_pBackup; ///< Head of the object pool for recycled nodes.
        std::size_t m_nItemCount; ///< Current number of items waiting in the queue.
        std::size_t m_nBackupCount; ///< Current number of nodes available for reuse in backup.

        std::size_t m_nMaxItemCount; ///< User-defined limit for the queue size.
        bool m_bThrowOnOverflow; ///< Policy for handling a full queue.
        std::size_t m_nWorkerCount; ///< Number of worker threads to manage.
        std::size_t m_nMaxBackupCount; ///< User-defined limit for the backup size.

        std::vector<std::thread> m_vWorkers; ///< Container for the worker thread pool.

        /**
         * @brief Internal helper to remove the front item from the queue.
         * @return An Entry containing the log data, while recycling the node to m_pBackup.
         */
        Entry popItem();

        /** @brief The main loop executed by each worker thread. */
        void doWork();
    };

    /** @name Logger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based logger (ASCII/UTF-8).
    using AsyncLoggerImpl = BasicAsyncLoggerImpl<char>;

    /// @brief Wide-character logger (Platform dependent, typically UTF-16 on Windows).
    using wAsyncLoggerImpl = BasicAsyncLoggerImpl<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 logger using C++20 char8_t.
    using u8AsyncLoggerImpl = BasicAsyncLoggerImpl<char8_t>;
#endif

    /// @brief UTF-16 logger using char16_t.
    using u16AsyncLoggerImpl = BasicAsyncLoggerImpl<char16_t>;

    /// @brief UTF-32 logger using char32_t.
    using u32AsyncLoggerImpl = BasicAsyncLoggerImpl<char32_t>;

    /** @} */

    template<typename char_t>
    BasicAsyncLoggerImpl<char_t>::BasicAsyncLoggerImpl(
        const std::size_t nMaxItemCount,
        const bool bThrowOnOverflow,
        const std::size_t nWorkerCount,
        const std::size_t nMaxBackupSize
    ) : m_bContinue(false),
        m_pFirst(nullptr),
        m_pFinal(nullptr),
        m_pBackup(nullptr),
        m_nItemCount(0),
        m_nBackupCount(0),
        m_nMaxItemCount(nMaxItemCount),
        m_bThrowOnOverflow(bThrowOnOverflow),
        m_nWorkerCount(nWorkerCount),
        m_nMaxBackupCount(nMaxBackupSize) {
        // Reserve workers
        this->m_vWorkers.reserve(nWorkerCount);
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::log(
        string_type sLevel, string_type sMessage
    ) {
        // Lock
        Item *pItem = nullptr;
        std::unique_lock oLock1(this->m_oGeneralMutex);
        std::unique_lock oLock2(this->m_oQueueMutex);
        // Overflow check
        if (this->m_nItemCount + 1 > this->m_nMaxItemCount) {
            // Throw when overflow
            if (this->m_bThrowOnOverflow)
                throw LoggerOverflowError{};
            // Ignore when overflow
            return;
        }
        ++this->m_nItemCount;
        // Instantiate item
        if (this->m_pBackup == nullptr) {
            // Create new item
            pItem = new Item(std::move(sLevel), std::move(sMessage));
        } else {
            // Fish one from backup
            pItem = this->m_pBackup;
            pItem->m_oEntry = {std::move(sLevel), std::move(sMessage)};
            this->m_pBackup = pItem->m_pNext;
            --this->m_nBackupCount;
        }
        // Append
        if (this->m_pFinal != nullptr)
            this->m_pFinal->m_pNext = pItem;
        this->m_pFinal = pItem;
        if (this->m_pFirst == nullptr)
            this->m_pFirst = this->m_pFinal;
        // Wake up call
        this->m_oCv.notify_one();
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::start() {
        std::unique_lock oLock(this->m_oGeneralMutex);
        // Check if valid
        if (
            const auto nStatus = this->serviceStatus();
            nStatus == Status::TRASCENDENT || nStatus == Status::RUNNING
        )
            return;
        // Start service
        std::lock_guard oStateLock(this->m_oStateMutex);
        this->serviceStart();
        // Killer thread
        if (this->m_oKillerThread.has_value()) {
            this->m_oKillerThread->join();
            this->m_oKillerThread.reset();
        }
        // Flag
        this->m_bContinue = true;
        // Create workers
        for (std::size_t i = 0; i < this->m_nWorkerCount; i++) {
            this->m_vWorkers.emplace_back([this] {
                this->doWork();
            });
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::stop() {
        std::unique_lock oLock(this->m_oGeneralMutex);
        // Check if valid
        if (
            const auto nStatus = this->serviceStatus();
            nStatus != Status::TRASCENDENT && nStatus != Status::RUNNING
        )
            return;
        // Shut up all
        std::lock_guard oStateLock(this->m_oStateMutex);
        this->m_bContinue = false;
        // Wake up everybody
        this->m_oCv.notify_all();
        // Kill workers
        for (auto &oWorker: this->m_vWorkers) {
            if (oWorker.joinable())
                oWorker.join();
        }
        this->m_vWorkers.clear();
        // Stop service
        this->serviceStop();
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::signalStop() {
        std::unique_lock oLock(this->m_oGeneralMutex);
        // Check if valid
        if (
            const auto nStatus = this->serviceStatus();
            nStatus != Status::TRASCENDENT && nStatus != Status::RUNNING
        )
            return;
        // Shut up all
        this->m_bContinue = false;
        // Wake up everybody
        this->m_oCv.notify_all();
        // Killer thread
        this->m_oKillerThread.emplace(
            [this] {
                // Kill workers
                std::lock_guard oStateLock(this->m_oStateMutex);
                for (auto &oWorker: this->m_vWorkers) {
                    if (oWorker.joinable())
                        oWorker.join();
                }
                this->m_vWorkers.clear();
                // Stop service
                this->serviceStop();
            }
        );
    }

    // Pops the queue
    template<typename char_t>
    typename BasicAsyncLoggerImpl<char_t>::Entry BasicAsyncLoggerImpl<char_t>::popItem() {
        // Ignore empty list
        if (this->m_pFirst == nullptr)return {};
        // Get entry
        Entry oEntry{std::move(this->m_pFirst->m_oEntry)};
        // Reorder
        const auto pTemp = this->m_pFirst;
        this->m_pFirst = pTemp->m_pNext;
        if (this->m_pFirst == nullptr)
            this->m_pFinal = nullptr;
        // Add to backup
        if (this->m_nBackupCount >= this->m_nMaxBackupCount) {
            delete pTemp;
        } else {
            pTemp->m_pNext = this->m_pBackup;
            this->m_pBackup = pTemp;
            ++this->m_nBackupCount;
        }
        // Overflow check
        --this->m_nItemCount;
        // Return entry
        return oEntry;
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::doWork() {
        bool bContinue;
        do {
            // Continue while there is something to do
            // Continue to end if stop signal
            std::unique_lock oLock(this->m_oQueueMutex);
            this->m_oCv.wait(oLock, [this] {
                return this->m_pFirst != nullptr
                       || !this->m_bContinue;
            });
            // Check if continue
            if (this->m_pFirst == nullptr && !this->m_bContinue)
                break;
            // Get entry
            auto [sLevel, sMessage] = popItem();
            // Do work (heavy lifting)
            oLock.unlock();
            this->work(std::move(sLevel), std::move(sMessage));
            oLock.lock();
            // Continue running
            const bool bEmpty = this->m_pFirst == nullptr;
            bContinue = this->m_bContinue || !bEmpty;
        } while (bContinue);
    }
}

#endif
