/**
 * @file ostream_logger.h
 * @brief Synchronous ostream-based logger implementation for the CppTrail framework.
 * @details Provides a concrete logger that directs output to any std::ostream
 * (console, files, stringstreams). Includes high-performance RFC 3339
 * timestamp formatting (UTC and Local) with sub-millisecond precision.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_OSTREAM_LOGGER_H
#define CPPTRAIL_OSTREAM_LOGGER_H

#if __cplusplus >= 202002L
#include "rfc3339.h"
#endif

#include <iostream>

#include "cpptrail/logger.h"

namespace CppTrail {
    /**
      * @class BasicSyncOstreamLogger
      * @brief A synchronous logger handle that directs output to a C++ standard output stream.
      * @details This class acts as a high-level wrapper around a specialized synchronous
      * implementation. It is ideal for console logging (`std::cout`/`std::cerr`) or
      * simple file logging via std::ofstream.
      * @tparam char_t The character type (e.g., char, wchar_t) used by the stream and logger.
      */
    template<typename char_t>
    class BasicSyncOstreamLogger : public BasicLogger<char_t> {
    public:
        /**
         * @brief Constructs a logger from a raw stream reference.
         * @param oOstream The output stream to be used for logging.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         */
        BasicSyncOstreamLogger(
            std::basic_ostream<char_t> &oOstream,
            const bool bUseLocalTime = true
        ) : BasicSyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> >(oOstream),
            bUseLocalTime
        ) {
        }

        /**
         * @brief Constructs a logger from a reference_wrapper.
         * @details This overload allows for more flexible lifecycle management of the
         * underlying stream.
         * @param oOstream A wrapper around the target output stream.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         */
        BasicSyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
            const bool bUseLocalTime = true
        ) : BasicLogger<char_t>(
            std::make_shared<Impl>(oOstream, bUseLocalTime)
        ) {
        }

    private:
        /**
         * @class Impl
         * @brief The concrete synchronous implementation for ostream-based logging.
         * @details Inherits from BasicSyncLoggerImpl to leverage existing mutex
         * protection, ensuring that interleaved logs from multiple threads do
         * not corrupt the stream output.
         */
        class Impl : public BasicSyncLoggerImpl<char_t> {
        public:
            /** @brief Alias for the underlying character type. */
            using char_type = typename BasicLoggerImpl<char_t>::char_type;

            /** @brief Alias for the basic_string type associated with this logger's encoding. */
            using string_type = typename BasicLoggerImpl<char_t>::string_type;

            /// @brief Alias for the BasicMessage type associated to the logger.
            using message_type = typename BasicLoggerImpl<char_t>::message_type;

        public:
            /**
             * @brief Internal constructor for the implementation.
             * @param oOstream Reference wrapper to the output sink.
             * @param bUseLocalTime Timestamp preference flag.
             */
            Impl(
                std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
                const bool bUseLocalTime
            ) : m_oOstream(oOstream),
                m_bUseLocalTime(bUseLocalTime) {
            }

            ~Impl() override {
                this->stop();
                this->join();
            }

        protected:
            /**
             * @brief Performs a blocking write operation to the underlying ostream.
             * @details This implementation executes directly on the caller's thread. It follows a
             * strict sequential pipeline:
             * 1. **Timestamping:** Generates an RFC 3339 compliant timestamp (C++20 only).
             * 2. **Level Recording:** Writes the log level string.
             * 3. **Message Flushing:** Writes the log content and appends a newline.
             * * @param oMessage The log record to be processed. This object is consumed (moved)
             * to minimize string allocations.
             * @note **Thread Safety:** This method is called while the base class's mutex is held,
             * ensuring that log entries from different threads do not interleave at the character level.
             * @warning Because this is synchronous, a slow I/O device (like a network-mounted file
             * or a busy terminal) will directly stall the thread that called log().
             */
            void work(message_type oMessage) override;

            /**
             * @brief Reports the service status.
             * @return Always returns Status::TRASCENDENT as streams are "always-on".
             */
            Status serviceStatus() override {
                return Status::TRASCENDENT;
            }

            /** @brief No-op; ostream sinks do not require a startup sequence. */
            void serviceStart() override {
            }

            /** @brief No-op; ostream sinks do not require a formal shutdown. */
            void serviceStop() override {
            }

        private:
            /// The target output stream reference.
            std::reference_wrapper<std::basic_ostream<char_t> > m_oOstream;

            /// Flag determining the timezone of the output.
            bool m_bUseLocalTime;
        };
    };

    /** @name OstreamLogger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based OstreamLogger (ASCII/UTF-8).
    using SyncOstreamLogger = BasicSyncOstreamLogger<char>;

    /// @brief Wide-character OstreamLogger (Platform dependent, typically UTF-16 on Windows).
    using wSyncOstreamLogger = BasicSyncOstreamLogger<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 OstreamLogger using C++20 char8_t.
    using u8SyncOstreamLogger = BasicSyncOstreamLogger<char8_t>;
#endif

    /// @brief UTF-16 OstreamLogger using char16_t.
    using u16SyncOstreamLogger = BasicSyncOstreamLogger<char16_t>;

    /// @brief UTF-32 OstreamLogger using char32_t.
    using u32SyncOstreamLogger = BasicSyncOstreamLogger<char32_t>;

    /** @} */

    /**
     * @class SyncCoutLogger
     * @brief A specialized logger handle targeting the standard output stream (std::cout).
     * @details Inherits the synchronous, thread-safe properties of OstreamLogger.
     * This class is intended for general information and diagnostic logging
     * destined for the console.
     * @note This class is a convenience wrapper and does not add additional state
     * beyond the standard OstreamLogger.
     */
    class SyncCoutLogger : public SyncOstreamLogger {
    public:
        /**
         * @brief Constructs a new CoutLogger.
         * @details Initializes the underlying ostream sink to std::cout.
         */
        SyncCoutLogger() : SyncOstreamLogger(std::cout) {
        }
    };

    /**
     * @class SyncCerrLogger
     * @brief A specialized logger handle targeting the standard error stream (std::cerr).
     * @details Inherits the synchronous, thread-safe properties of OstreamLogger.
     * This class is typically used for high-priority alerts, errors, and
     * critical failures that must be visible even if stdout is redirected.
     * @note This class is a convenience wrapper and does not add additional state
     * beyond the standard OstreamLogger.
     */
    class SyncCerrLogger : public SyncOstreamLogger {
    public:
        /**
         * @brief Constructs a new CerrLogger.
         * @details Initializes the underlying ostream sink to std::cerr.
         */
        SyncCerrLogger() : SyncOstreamLogger(std::cerr) {
        }
    };

    /**
     * @class BasicAsyncOstreamLogger
     * @brief An asynchronous logger handle that dispatches output to a C++ standard stream.
     * @details This class provides a non-blocking interface for stream-based logging.
     * Log entries are submitted to an internal thread-safe queue and processed
     * by a background worker thread, ensuring that I/O stalls (e.g., slow terminal
     * scrolling or disk latency) do not block the application's critical path.
     * @note This is the asynchronous counterpart to BasicOstreamLogger. It is
     * particularly suited for high-throughput "wire stuff" where logging overhead
     * must be minimized.
     * @tparam char_t The character type (e.g., char, wchar_t) used for the stream
     * and log encoding.
     */
    template<typename char_t>
    class BasicAsyncOstreamLogger : public BasicLogger<char_t> {
    public:
        /**
         * @brief Constructs a AsyncLogger from a raw stream reference.
         * @param oOstream The output stream to be used for logging.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         */
        BasicAsyncOstreamLogger(
            std::basic_ostream<char_t> &oOstream,
            const bool bUseLocalTime = true
        ) : BasicAsyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> >(oOstream),
            bUseLocalTime
        ) {
        }

        /**
         * @brief Constructs a AsyncLogger from a reference_wrapper.
         * @details This overload allows for more flexible lifecycle management of the
         * underlying stream.
         * @param oOstream A wrapper around the target output stream.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         */
        BasicAsyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
            const bool bUseLocalTime = true
        ) : BasicLogger<char_t>(
            std::make_shared<Impl>(oOstream, bUseLocalTime)
        ) {
        }

        /**
         * @brief Constructs a AsyncLogger from a reference_wrapper.
         * @details This overload allows for more flexible lifecycle management of the
         * underlying stream.
         * @param oOstream A wrapper around the target output stream.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         * @param nMaxEntryCount The maximum number of log entries allowed in the queue.
         * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
         * @warning nWorkerCount is dissabled because std::basic_ostream does not offer multi-threading gurarantees
         *          That makes the use of multiple workers useless if used mutex is used or jumbled if not.
         */
        BasicAsyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
            const bool bUseLocalTime,
            std::size_t nMaxEntryCount,
            bool bThrowOnOverflow
        ) : BasicLogger<char_t>(
            std::make_shared<Impl>(
                oOstream,
                bUseLocalTime,
                nMaxEntryCount,
                bThrowOnOverflow
            )
        ) {
        }

    private:
        /**
         * @class Impl
         * @brief The concrete asynchronous implementation for stream-based logging.
         * @details Inherits from BasicAsyncLoggerImpl to provide a non-blocking
         * logging sink. This implementation manages an internal producer-consumer
         * queue and a background worker thread that performs the actual I/O
         * operations on the target stream.
         * By decoupling the log submission from the stream writing, it prevents
         * high-latency "wire stuff" captures or slow terminal scrolling from
         * stalling the application's critical execution path.
         */
        class Impl : public BasicAsyncLoggerImpl<char_t> {
        public:
            /** @brief Alias for the underlying character type. */
            using char_type = typename BasicLoggerImpl<char_t>::char_type;

            /** @brief Alias for the basic_string type associated with this logger's encoding. */
            using string_type = typename BasicLoggerImpl<char_t>::string_type;

            /// @brief Alias for the BasicMessage type associated to the logger.
            using message_type = typename BasicLoggerImpl<char_t>::message_type;

        public:
            /**
             * @brief Internal constructor for the implementation.
             * @param oOstream Reference wrapper to the output sink.
             * @param bUseLocalTime Timestamp preference flag.
             */
            Impl(
                std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
                const bool bUseLocalTime
            ) : m_oOstream(oOstream),
                m_bUseLocalTime(bUseLocalTime) {
            }

            /**
             * @brief Internal constructor for the implementation.
             * @param oOstream Reference wrapper to the output sink.
             * @param bUseLocalTime Timestamp preference flag.
             * @param nMaxEntryCount The maximum number of log entries allowed in the queue.
             * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
             * @warning nWorkerCount is dissabled because std::basic_ostream does not offer multi-threading gurarantees
             *          That makes the use of multiple workers useless if used mutex is used or jumbled if not.
             */
            Impl(
                std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
                const bool bUseLocalTime,
                std::size_t nMaxEntryCount,
                bool bThrowOnOverflow
            ) : BasicAsyncLoggerImpl<char_t>(
                    nMaxEntryCount,
                    bThrowOnOverflow,
                    1
                ), m_oOstream(oOstream),
                m_bUseLocalTime(bUseLocalTime) {
            }

            /**
             * @brief Destructor for the concrete asynchronous stream implementation.
             * @details This destructor explicitly invokes stop() and join() to ensure
             * a graceful shutdown before the object begins its "degradation" into
             * the base class.
             * By triggering the shutdown here, the background worker threads are
             * guaranteed to finish their remaining 'work()' calls while this
             * class's specific vtable and resources (like the ostream reference)
             * are still valid and accessible.
             * @note Failure to call stop/join in this derived destructor would result
             * in the base class destructor attempting to join threads that might
             * still try to call pure virtual methods, leading to a runtime crash.
             */
            ~Impl() override {
                this->stop();
                this->join();
            }

        protected:
            /**
             * @brief Background worker task for flushing queued log records to the stream.
             * @details This implementation is invoked by the background worker pool. Unlike the
             * synchronous version, this call does not block the application's main execution path.
             * * **Execution Logic:**
             * - **Resource Stealing:** Uses `stealLevel()` and `stealString()` to take ownership
             * of the message data without performing deep copies, maximizing "wire-speed" throughput.
             * - **I/O Offloading:** Handles the potentially slow `oOstream.flush()` call in the
             * background, isolating the rest of the application from disk or terminal latency.
             * - **Formatting:** Prepends an RFC 3339 timestamp if C++20 is available.
             * * @param oMessage The log record retrieved from the internal producer-consumer queue.
             * @note This method is executed outside of the queue lock, allowing other threads
             * to continue submitting logs while this worker performs the heavy I/O lifting.
             */
            void work(message_type oMessage) override;

            /**
             * @brief Reports the service status.
             * @return Always returns Status::TRASCENDENT as streams are "always-on".
             */
            Status serviceStatus() override {
                return Status::TRASCENDENT;
            }

            /** @brief No-op; ostream sinks do not require a startup sequence. */
            void serviceStart() override {
            }

            /** @brief No-op; ostream sinks do not require a formal shutdown. */
            void serviceStop() override {
            }

        private:
            /// The target output stream reference.
            std::reference_wrapper<std::basic_ostream<char_t> > m_oOstream;
            /// Flag determining the timezone of the output.
            bool m_bUseLocalTime;
        };
    };

    /** @name AsyncOstreamLogger Type Aliases
     * Standardized aliases for common character encodings.
     * @{ */

    /// @brief Standard char-based AsyncOstreamLogger (ASCII/UTF-8).
    using AsyncOstreamLogger = BasicAsyncOstreamLogger<char>;

    /// @brief Wide-character AsyncOstreamLogger (Platform dependent, typically UTF-16 on Windows).
    using wAsyncOstreamLogger = BasicAsyncOstreamLogger<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 AsyncOstreamLogger using C++20 char8_t.
    using u8AsyncOstreamLogger = BasicAsyncOstreamLogger<char8_t>;
#endif

    /// @brief UTF-16 AsyncOstreamLogger using char16_t.
    using u16AsyncOstreamLogger = BasicAsyncOstreamLogger<char16_t>;

    /// @brief UTF-32 AsyncOstreamLogger using char32_t.
    using u32AsyncOstreamLogger = BasicAsyncOstreamLogger<char32_t>;

    /** @} */

    /**
    * @class AsyncCoutLogger
    * @brief A specialized asynchronous logger handle targeting the standard output stream (std::cout).
    * @details This class provides a non-blocking wrapper for console output. By utilizing 
    * the underlying AsyncOstreamLogger architecture, log submissions return immediately 
    * while a background thread handles the actual terminal I/O. 
    * This is ideal for high-frequency diagnostic data where blocking the main thread 
    * for console scrolling or terminal latency would skew performance metrics.
    * @note As an asynchronous logger, it requires an explicit start() call to begin 
    * processing the queue.
    */
    class AsyncCoutLogger : public AsyncOstreamLogger {
    public:
        /**
         * @brief Constructs a CoutAsyncLogger initialized with the std::cout sink.
         */
        AsyncCoutLogger() : AsyncOstreamLogger(std::cout) {
        }
    };

    /**
     * @class AsyncCerrLogger
     * @brief A specialized asynchronous logger handle targeting the standard error stream (std::cerr).
     * @details Provides a non-blocking channel for high-priority alerts and error 
     * reporting. Unlike standard std::cerr, which is unbuffered and blocking, this 
     * class offloads the error reporting to a background worker.
     * This ensures that even during a catastrophic failure or "wire stuff" crash, 
     * the error messages are queued and flushed without freezing the remaining 
     * logic of the application.
     * @note Logs destined for std::cerr via this handle are processed in the order 
     * they were submitted to the queue.
     */
    class AsyncCerrLogger : public AsyncOstreamLogger {
    public:
        /**
         * @brief Constructs a CerrAsyncLogger initialized with the std::cerr sink.
         */
        AsyncCerrLogger() : AsyncOstreamLogger(std::cerr) {
        }
    };

    template<typename char_t>
    void BasicSyncOstreamLogger<char_t>::Impl::work(
        message_type oMessage
    ) {
        // Get ostream
        std::basic_ostream<char_t> &oOstream = m_oOstream.get();

#if __cplusplus >= 202002L
        // Write time
        Detail::toRfc3339<char_t>(oOstream, this->m_bUseLocalTime);
#endif

        // Write level + message
        oOstream << oMessage.stealLevel();
        oOstream.put(static_cast<char_t>(' '));
        oOstream << oMessage.stealString();
        oOstream.put(static_cast<char_t>('\n'));

        // Flush
        oOstream.flush();
    }

    template<typename char_t>
    void BasicAsyncOstreamLogger<char_t>::Impl::work(
        message_type oMessage
    ) {
        // Get ostream
        std::basic_ostream<char_t> &oOstream = m_oOstream.get();

#if __cplusplus >= 202002L
        // Write time
        Detail::toRfc3339<char_t>(oOstream, this->m_bUseLocalTime);
#endif

        // Write level
        oOstream << oMessage.stealLevel();

        // Write message
        oOstream.put(static_cast<char_t>(' '));
        oOstream << oMessage.stealString();
        oOstream.put(static_cast<char_t>('\n'));

        // Flush
        oOstream.flush();
    }
}

#endif
