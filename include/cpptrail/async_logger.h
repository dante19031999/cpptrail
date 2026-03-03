/**
 * @file async_logger.h
 * @brief Asynchronous logger implementation using a background worker pool.
 * @details This header defines the BasicAsyncLoggerImpl template, providing a
 * decoupled logging architecture where I/O operations are performed by a
 * managed pool of background threads. It includes overflow handling and
 * an asynchronous "killer" mechanism for graceful shutdown.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */
#ifndef CPPTRAIL_ASYNC_LOGGER_H
#define CPPTRAIL_ASYNC_LOGGER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include "cpptrail/base_logger.h"

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

        /// @brief Alias for the BasicMessage type associated to the logger.
        using message_type = typename BasicLoggerImpl<char_t>::message_type;

    protected:
        /**
         * @brief Default constructor.
         * @details Initializes a single worker thread and sets an effectively infinite queue limit.
         */
        BasicAsyncLoggerImpl() : BasicAsyncLoggerImpl(
            std::numeric_limits<std::size_t>::max(),
            true, 1
        ) {
        }

        /**
         * @brief Specialized constructor for controlled resource management.
         * @param nMaxEntryCount The maximum number of log entries allowed in the queue.
         * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
         * @param nWorkerCount The number of background threads to spawn for processing.
         */
        BasicAsyncLoggerImpl(
            std::size_t nMaxEntryCount,
            bool bThrowOnOverflow,
            std::size_t nWorkerCount
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
         * @param oMessage The message to be recorded.
         */
        virtual void work(message_type oMessage) = 0;

    public:
        /**
          * @brief Enqueues a log message for asynchronous processing.
          * @details This is the primary entry point for logging. It is designed to be 
          * high-performance and non-blocking under normal conditions. The message is 
          * moved into an internal thread-safe queue to be processed by the worker pool.
          * @param oMessage The message to be logged. This object is moved; the caller 
          * should not rely on its state after this call.
          * @throws LoggerOverflowError If the internal queue reaches m_nMaxEntryCount 
          * and the logger is configured to throw on overflow.
          * @note If the logger is in Status::STOPPED or Status::BROKEN, the message 
          * will be silently discarded to prevent memory leaks during shutdown.
          * @note If overflow occurs and bThrowOnOverflow is false, the message is 
          * dropped to prioritize application stability over log completeness.
          */
        void log(message_type oMessage) final;

        /**
          * @brief Starts the background worker pool and the underlying service.
          * @details This is a blocking call that ensures the lifecycle is initialized in the correct order:
          * 1. It waits for any existing "killer" threads to finish.
          * 2. It calls serviceStart() to allow derived classes to acquire resources (files, sockets, etc.).
          * 3. It spawns the specified number of background worker threads.
          * @note This method is thread-safe and prevents multiple simultaneous start sequences.
          */
        void start() final;

        /**
         * @brief Synchronously shuts down the worker pool and the service.
         * @details This method performs a graceful "Drain and Stop" sequence:
         * 1. It signals the worker engine to stop accepting new logs.
         * 2. It blocks the calling thread until all workers have finished processing the current queue.
         * 3. It calls serviceStop() to release resources in the derived implementation.
         * @warning This is a blocking call. If the queue is large, it may take time to return.
         */
        void stop() final;

        /**
         * @brief Initiates a shutdown sequence on a separate background "killer" thread.
         * @details This provides a non-blocking alternative to stop(). It spawns a
         * temporary thread that executes the full stop() sequence in the background,
         * allowing the calling thread to continue immediately.
         * @note Use join() later to ensure this background shutdown has completed.
         */
        void signalStop() final;

        /**
         * @brief Blocks until all background activity (workers and killer threads) has ceased.
         * @details This is the final synchronization point. It ensures that no background
         * threads are still accessing the logger's memory.
         * @note This is called automatically by the BasicLogger handle's destructor to
         * ensure RAII safety and prevent "zombie" threads.
         */
        void join() final;

        /**
         * @brief Retrieves the aggregated operational status of the asynchronous logger.
         * @details This method performs a thread-safe evaluation of the logger's health by
         * cross-referencing the underlying service (sink) status with the worker engine's state.
         * The logic follows a "Contract of Expectation":
         * - If the service is **RUNNING/TRASCENDENT**, the workers must be **ON**.
         * - If the service is **STOPPED**, the workers must be **OFF**.
         * - Any mismatch (e.g., workers running but service stopped) results in **Status::BROKEN**.
         * @return Status The current composite state:
         * - **RUNNING/TRASCENDENT**: Fully operational and accepting logs.
         * - **STOPPED**: Gracefully shut down; no background activity.
         * - **BROKEN**: An inconsistency or critical failure has occurred in the pipeline.
         * @note This call acquires the worker mutex to ensure a consistent snapshot of
         * the engine state.
         */
        Status status() final;

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

    private:
        /** @name Queue Engine
         * Members responsible for message buffering and overflow management.
         * @{ */
        /// @brief Mutex protecting access to the message queue and overflow logic.
        std::mutex m_oQueueMutex;
        /// @brief Internal buffer for messages awaiting processing.
        std::deque<message_type> m_qEntryQueue;
        /// @brief Hard limit for the number of messages allowed in the queue.
        std::size_t m_nMaxEntryCount;
        /// @brief If true, log() throws an exception on overflow; otherwise, messages are dropped.
        bool m_bThrowOnOverflow;
        /** @} */

        /** @name Worker Engine
         * Members responsible for the background processing lifecycle.
         * @{ */
        /// @brief Mutex protecting the lifecycle state (start/stop) of the worker pool.
        std::mutex m_oWorkerMutex;
        /// @brief Condition variable used to wake workers when data is available or shutdown is requested.
        std::condition_variable m_oWorkerCv;
        /// @brief The pool of active background worker threads.
        std::vector<std::thread> m_vWorkers;
        /// @brief Configured number of threads to maintain in the worker pool.
        std::size_t m_nWorkerCount;
        /// @brief Operational flag; when false, workers will begin their graceful exit sequence.
        std::atomic_bool m_bWorkerEngineOn{false};
        /** @} */

        /** @name Delay Killer
         * Members responsible for offloading the blocking shutdown sequence.
         * @{ */
        /// @brief Mutex ensuring serialized access to the background shutdown thread.
        std::mutex m_oKillerMutex;
        /// @brief Handle for the background thread performing a delayed stop().
        std::vector<std::thread> m_oKillerThread;
        /// @brief Condition variable for synchronizing killer thread events.
        std::condition_variable m_oKillerCv;
        /** @} */

    private:
        /**
         * @brief The main loop executed by every worker thread in the pool.
         * @details Implements the "Drain and Work" logic: workers wait on m_oWorkerCv,
         * pop messages from the queue while holding m_oQueueMutex, and execute the
         * virtual work() implementation after releasing the lock.
         */
        void work();

        /**
         * @brief Allocates and initializes the worker thread pool.
         * @details Sets m_bWorkerEngineOn to true and populates m_vWorkers with
         * threads running the work() loop.
         */
        void startWorkers();

        /**
         * @brief Orchestrates the shutdown of all active worker threads.
         * @details Signals termination via m_bWorkerEngineOn, notifies all waiting
         * threads, and joins each worker thread to ensure no leaks occur.
         */
        void stopWorkers();

        /**
         * @brief Synchronizes with the background killer thread.
         * @details Checks if a killer thread is active, joins it if it is joinable,
         * and clears the container to prepare for future shutdown requests.
         */
        void waitForKiller();
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
        const std::size_t nMaxEntryCount,
        const bool bThrowOnOverflow,
        const std::size_t nWorkerCount
    ) : m_nMaxEntryCount(nMaxEntryCount),
        m_bThrowOnOverflow(bThrowOnOverflow),
        m_nWorkerCount(nWorkerCount) {
        // Reserve workers
        this->m_vWorkers.reserve(nWorkerCount);
        this->m_oKillerThread.reserve(1);
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::log(
        message_type oMessage
    ) {
        // Check if we accept log requests
        if (!m_bWorkerEngineOn.load())return;

        // Queue lock
        std::unique_lock<std::mutex> oQueueLock(this->m_oQueueMutex);

        // Overflow check
        if (this->m_qEntryQueue.size() + 1 > this->m_nMaxEntryCount) {
            // Throw when overflow
            if (this->m_bThrowOnOverflow)
                throw LoggerOverflowError{};
            // Ignore when overflow
            return;
        }

        // Push back message
        this->m_qEntryQueue.push_back(std::move(oMessage));

        // Wake up call
        this->m_oWorkerCv.notify_one();
    }


    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::startWorkers() {
        if (!this->m_bWorkerEngineOn.load()) {
            // Create workers
            this->m_vWorkers.clear();
            for (std::size_t i = 0; i < this->m_nWorkerCount; i++) {
                this->m_vWorkers.emplace_back([this] {
                    this->work();
                });
            }
            // Update status
            this->m_bWorkerEngineOn.store(true);
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::stopWorkers() {
        if (this->m_bWorkerEngineOn.load()) {
            // Ask for worker termination
            this->m_bWorkerEngineOn.store(false);
            this->m_oWorkerCv.notify_all();
            // Kill workers
            for (auto &oWorker: this->m_vWorkers) {
                if (oWorker.joinable())
                    oWorker.join();
            }
            this->m_vWorkers.clear();
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::waitForKiller() {
        // Ensure killer thread is off
        if (!this->m_oKillerThread.empty()) {
            auto &oKiller = this->m_oKillerThread.front();
            if (oKiller.joinable())
                oKiller.join();
            this->m_oKillerThread.clear();
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::start() {
        // Ensure killer thread is OFF
        this->m_oKillerMutex.lock();
        this->waitForKiller();

        // Worker lock
        std::unique_lock<std::mutex> oWorkerLock(this->m_oWorkerMutex);

        // Release the killer lock
        // We no longer need it
        this->m_oKillerMutex.unlock();

        // Ensure killer thread is OFF
        this->waitForKiller();

        // Get service status
        const auto nStatus = this->serviceStatus();

        // Start service
        if (nStatus != Status::TRASCENDENT && nStatus != Status::RUNNING) {
            // External service if OFF
            this->serviceStart();
        }

        // Start the working engine
        this->startWorkers();
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::stop() {
        // Ensure killer thread is OFF
        this->m_oKillerMutex.lock();
        this->waitForKiller();

        // Worker lock
        std::unique_lock<std::mutex> oWorkerLock(this->m_oWorkerMutex);

        // Release the killer lock
        // We no longer need it
        this->m_oKillerMutex.unlock();

        // Stop the working engine
        this->stopWorkers();

        // Get service status
        const auto nStatus = this->serviceStatus();

        // Stop service
        if (nStatus == Status::TRASCENDENT || nStatus == Status::RUNNING) {
            // External service if ON
            this->serviceStop();
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::signalStop() {
        // Killer lock
        std::unique_lock<std::mutex> oKillerLock(this->m_oKillerMutex);

        // Ensure killer thread is OFF
        this->waitForKiller();

        // Worker lock
        std::unique_lock<std::mutex> oWorkerLock(this->m_oWorkerMutex);

        // Killer thread
        this->m_oKillerThread.emplace_back(
            [this] {
                // Worker lock
                std::unique_lock<std::mutex> oKillerWorkerLock(this->m_oWorkerMutex);

                // Stop the working engine
                this->stopWorkers();

                // Get service status
                const auto nStatus = this->serviceStatus();

                // Stop service
                if (nStatus == Status::TRASCENDENT || nStatus == Status::RUNNING) {
                    // External service if ON
                    this->serviceStop();
                }
            }
        );
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::join() {
        // Killer lock
        std::unique_lock<std::mutex> oKillerLock(this->m_oKillerMutex);
        // Ensure killer thread is OFF
        this->waitForKiller();
    }

    template<typename char_t>
    Status BasicAsyncLoggerImpl<char_t>::status() {
        // Get service status
        auto nStatus = this->serviceStatus();

        // Calculate logger status
        switch (nStatus) {
            case Status::TRASCENDENT:
            case Status::RUNNING:
                return m_bWorkerEngineOn.load() ? nStatus : Status::BROKEN;
            case Status::STOPPED:
                return !m_bWorkerEngineOn.load() ? nStatus : Status::BROKEN;
            case Status::BROKEN:
            default:
                return Status::BROKEN;
        }
    }

    template<typename char_t>
    void BasicAsyncLoggerImpl<char_t>::work() {
        bool bContinue;
        do {
            // Queue lock
            std::unique_lock<std::mutex> oQueueLock(this->m_oQueueMutex);

            // Continue while there is something to do
            // Continue to end if stop signal
            this->m_oWorkerCv.wait(oQueueLock, [this] {
                // Checkif there are entries queued
                if (!this->m_qEntryQueue.empty())return true;
                // Continue to stop
                return !this->m_bWorkerEngineOn.load();
            });

            // Check entry
            if (!this->m_qEntryQueue.empty()) {
                // Get entry
                message_type oEntry = std::move(this->m_qEntryQueue.front());
                this->m_qEntryQueue.pop_front();

                // Do work (heavy lifting)
                this->m_oQueueMutex.unlock();
                this->work(std::move(oEntry));
                this->m_oQueueMutex.lock();
            }

            // Determine wherther continue
            const bool bEmpty = this->m_qEntryQueue.empty();
            const bool bEngine = this->m_bWorkerEngineOn.load();
            bContinue = bEngine || !bEmpty;
        } while (bContinue);
    }
}

#endif
