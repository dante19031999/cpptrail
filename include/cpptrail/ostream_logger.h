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
#include <chrono>
#endif

#include <iostream>

#include "cpptrail/logger.h"

namespace CppTrail {
#if __cplusplus >= 202002L
    namespace Detail {
        /// Time point type for CppTrail
        using time_point = std::chrono::time_point<std::chrono::system_clock>;

        /**
         * @brief Writes an integer to an output stream with optional zero padding.
         * @warning This function does not handle the minimum value of signed types
         * (e.g., INT_MIN) due to negation overflow. It is intended for date
         * components which remain within reasonable positive/negative bounds.
         * @tparam char_t The character type of the stream (e.g., char, wchar_t).
         * @tparam int_t The integral type of the value to write.
         * @param sOutput The output stream to which the formatted integer is written.
         * @param nValue The integer value to format.
         * @param nMinCiphers The minimum number of digits to write (padded with leading zeros).
         */
        template<typename char_t, typename int_t>
        void writeInteger(
            std::basic_ostream<char_t> &sOutput,
            int_t nValue,
            std::uint8_t nMinCiphers = 0
        );

        /**
         * @brief Writes a time_point to an output stream using the RFC 3339 standard (UTC).
         *
         * Pattern: YYYY-MM-DD HH:mm:ss.SSSZ
         *
         * @tparam char_t Character type (e.g., char, wchar_t).
         * @param sOutput The output stream to write to.
         * @param nTime The chrono time_point to be converted.
         * @note Optimized for performance by using sOutput.put() and internal integer
         * formatting to avoid ostream operator<< overhead.
         */
        template<typename char_t>
        void toIso3339(std::basic_ostream<char_t> &sOutput, time_point nTime);

        /**
         * @brief Writes a time_point to an output stream using the RFC 3339 standard (Local Time).
         *
         * Pattern: YYYY-MM-DD HH:mm:ss.SSS±HH:mm
         *
         * @tparam char_t Character type.
         * @param sOutput The output stream to write to.
         * @param nTime The chrono time_point to be converted.
         * @note Decomposes the time_point exactly once before streaming, ensuring
         * high efficiency in performance-critical paths.
         */
        template<typename char_t>
        void toIso3339Local(std::basic_ostream<char_t> &sOutput, time_point nTime);
    }
#endif

    /**
      * @class BasicOstreamLogger
      * @brief A synchronous logger handle that directs output to a C++ standard output stream.
      * @details This class acts as a high-level wrapper around a specialized synchronous
      * implementation. It is ideal for console logging (`std::cout`/`std::cerr`) or
      * simple file logging via std::ofstream.
      * @tparam char_t The character type (e.g., char, wchar_t) used by the stream and logger.
      */
    template<typename char_t>
    class BasicOstreamLogger : public BasicLogger<char_t> {
    public:
        /**
         * @brief Constructs a logger from a raw stream reference.
         * @param oOstream The output stream to be used for logging.
         * @param bUseLocalTime If true, uses local time for timestamps; otherwise, uses UTC.
         */
        BasicOstreamLogger(
            std::basic_ostream<char_t> &oOstream,
            const bool bUseLocalTime = true
        ) : BasicOstreamLogger(
            std::reference_wrapper(oOstream),
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
        BasicOstreamLogger(
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
            /// @brief Alias for the underlying character type.
            using char_type = typename BasicLoggerImpl<char_t>::char_type;

            /// @brief Alias for the basic_string type.
            using string_type = typename BasicLoggerImpl<char_t>::string_type;

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

        protected:
            /**
             * @brief Performs the actual string formatting and stream writing.
             * @details Overrides the internal work hook to write timestamps,
             * levels, and messages.
             * @param sLevel The log level string.
             * @param sMessage The log message string.
             */
            void work(string_type sLevel, string_type sMessage) override;

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
    using OstreamLogger = BasicOstreamLogger<char>;

    /// @brief Wide-character OstreamLogger (Platform dependent, typically UTF-16 on Windows).
    using wOstreamLogger = BasicOstreamLogger<wchar_t>;

#if __cplusplus >= 202002L
    /// @brief Explicit UTF-8 OstreamLogger using C++20 char8_t.
    using u8OstreamLogger = BasicOstreamLogger<char8_t>;
#endif

    /// @brief UTF-16 OstreamLogger using char16_t.
    using u16OstreamLogger = BasicOstreamLogger<char16_t>;

    /// @brief UTF-32 OstreamLogger using char32_t.
    using u32OstreamLogger = BasicOstreamLogger<char32_t>;

    /** @} */

    /**
     * @class CoutLogger
     * @brief A specialized logger handle targeting the standard output stream (std::cout).
     * @details Inherits the synchronous, thread-safe properties of OstreamLogger.
     * This class is intended for general information and diagnostic logging
     * destined for the console.
     * @note This class is a convenience wrapper and does not add additional state
     * beyond the standard OstreamLogger.
     */
    class CoutLogger : public OstreamLogger {
    public:
        /**
         * @brief Constructs a new CoutLogger.
         * @details Initializes the underlying ostream sink to std::cout.
         */
        CoutLogger() : OstreamLogger(std::cout) {
        }
    };

    /**
     * @class CerrLogger
     * @brief A specialized logger handle targeting the standard error stream (std::cerr).
     * @details Inherits the synchronous, thread-safe properties of OstreamLogger.
     * This class is typically used for high-priority alerts, errors, and
     * critical failures that must be visible even if stdout is redirected.
     * @note This class is a convenience wrapper and does not add additional state
     * beyond the standard OstreamLogger.
     */
    class CerrLogger : public OstreamLogger {
    public:
        /**
         * @brief Constructs a new CerrLogger.
         * @details Initializes the underlying ostream sink to std::cerr.
         */
        CerrLogger() : OstreamLogger(std::cerr) {
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
            std::reference_wrapper(oOstream),
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
         * @param nMaxItemCount The maximum number of log entries allowed in the queue.
         * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
         * @param nWorkerCount The number of background threads to spawn for processing.
         * @param nMaxBackupSize The maximum number of log entries allowed in the backup.
         */
        BasicAsyncOstreamLogger(
            std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
            const bool bUseLocalTime,
            std::size_t nMaxItemCount,
            bool bThrowOnOverflow,
            std::size_t nWorkerCount,
            std::size_t nMaxBackupSize
        ) : BasicLogger<char_t>(
            std::make_shared<Impl>(
                oOstream,
                bUseLocalTime,
                nMaxItemCount,
                bThrowOnOverflow,
                nWorkerCount,
                nMaxBackupSize
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
            /// @brief Alias for the underlying character type.
            using char_type = typename BasicAsyncLoggerImpl<char_t>::char_type;

            /// @brief Alias for the basic_string type.
            using string_type = typename BasicAsyncLoggerImpl<char_t>::string_type;

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
                m_bUseLocalTime(bUseLocalTime),
                m_nStatus(Status::STOPPED) {
            }

            /**
            * @brief Internal constructor for the implementation.
            * @param oOstream Reference wrapper to the output sink.
            * @param bUseLocalTime Timestamp preference flag.
            * @param nMaxItemCount The maximum number of log entries allowed in the queue.
            * @param bThrowOnOverflow If true, log() throws LoggerOverflowError when full; otherwise, it drops the log.
            * @param nWorkerCount The number of background threads to spawn for processing.
            * @param nMaxBackupSize The maximum number of log entries allowed in the backup.
            */
            Impl(
                std::reference_wrapper<std::basic_ostream<char_t> > oOstream,
                const bool bUseLocalTime,
                std::size_t nMaxItemCount,
                bool bThrowOnOverflow,
                std::size_t nWorkerCount,
                std::size_t nMaxBackupSize
            ) : BasicAsyncLoggerImpl<char_t>(
                    nMaxItemCount,
                    bThrowOnOverflow,
                    nWorkerCount,
                    nMaxBackupSize
                ), m_oOstream(oOstream),
                m_bUseLocalTime(bUseLocalTime),
                m_nStatus(Status::STOPPED) {
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
             * @brief Performs the actual string formatting and stream writing.
             * @details Overrides the internal work hook to write timestamps,
             * levels, and messages.
             * @param sLevel The log level string.
             * @param sMessage The log message string.
             */
            void work(string_type sLevel, string_type sMessage) override;

            /**
             * @brief Reports the service status.
             * @return Always returns Status::TRASCENDENT as streams are "always-on".
             */
            Status serviceStatus() override {
                return this->m_nStatus;
            }

            /** @brief No-op; ostream sinks do not require a startup sequence. */
            void serviceStart() override {
                this->m_nStatus = Status::RUNNING;
            }

            /** @brief No-op; ostream sinks do not require a formal shutdown. */
            void serviceStop() override {
                this->m_nStatus = Status::STOPPED;
            }

        private:
            /// The target output stream reference.
            std::reference_wrapper<std::basic_ostream<char_t> > m_oOstream;
            /// Flag determining the timezone of the output.
            bool m_bUseLocalTime;
            /// Service status
            Status m_nStatus;
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
    * @class CoutAsyncLogger
    * @brief A specialized asynchronous logger handle targeting the standard output stream (std::cout).
    * @details This class provides a non-blocking wrapper for console output. By utilizing 
    * the underlying AsyncOstreamLogger architecture, log submissions return immediately 
    * while a background thread handles the actual terminal I/O. 
    * This is ideal for high-frequency diagnostic data where blocking the main thread 
    * for console scrolling or terminal latency would skew performance metrics.
    * @note As an asynchronous logger, it requires an explicit start() call to begin 
    * processing the queue.
    */
    class CoutAsyncLogger : public AsyncOstreamLogger {
    public:
        /**
         * @brief Constructs a CoutAsyncLogger initialized with the std::cout sink.
         */
        CoutAsyncLogger() : AsyncOstreamLogger(std::cout) {
        }
    };

    /**
     * @class CerrAsyncLogger
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
    class CerrAsyncLogger : public AsyncOstreamLogger {
    public:
        /**
         * @brief Constructs a CerrAsyncLogger initialized with the std::cerr sink.
         */
        CerrAsyncLogger() : AsyncOstreamLogger(std::cerr) {
        }
    };

    template<typename char_t>
    void BasicOstreamLogger<char_t>::Impl::work(
        string_type sLevel, string_type sMessage
    ) {
        // Get ostream
        std::basic_ostream<char_t> &oOstream = m_oOstream.get();

#if __cplusplus >= 202002L
        // Write time
        auto nTime = std::chrono::system_clock::now();
        if (this->m_bUseLocalTime)
            Detail::toIso3339Local(oOstream, nTime);
        else
            Detail::toIso3339(oOstream, nTime);
        oOstream.put(static_cast<char_t>(' '));
#endif

        // Write level + message
        oOstream << sLevel;
        oOstream.put(static_cast<char_t>(' '));
        oOstream << sMessage;
        oOstream.put(static_cast<char_t>('\n'));

        // Flush
        oOstream.flush();
    }

#if __cplusplus >= 202002L
    template<typename char_t>
    void BasicAsyncOstreamLogger<char_t>::Impl::work(
        string_type sLevel, string_type sMessage
    ) {
        // Get ostream
        std::basic_ostream<char_t> &oOstream = m_oOstream.get();

        // Write time
        auto nTime = std::chrono::system_clock::now();
        if (this->m_bUseLocalTime)
            Detail::toIso3339Local(oOstream, nTime);
        else
            Detail::toIso3339(oOstream, nTime);

        // Write level
        oOstream.put(static_cast<char_t>(' '));
        oOstream << sLevel;

        // Write message
        oOstream.put(static_cast<char_t>(' '));
        oOstream << sMessage;
        oOstream.put(static_cast<char_t>('\n'));

        // Flush
        oOstream.flush();
    }

    namespace Detail {
        /// Defalut timezone
        inline const std::chrono::time_zone *TIME_ZONE
                = std::chrono::current_zone();

        template<typename char_t, typename int_t>
        void writeInteger(
            std::basic_ostream<char_t> &sOutput,
            int_t nValue,
            const std::uint8_t nMinCiphers
        ) {
            static_assert(std::is_integral_v<int_t>);

            // Handle possible troublesome case
            if (nValue == 0) {
                for (std::uint8_t i = 1; i < nMinCiphers; ++i) {
                    sOutput.put(static_cast<char_t>('0'));
                }
                sOutput.put('0');
                return;
            }

            // Handle sign
            if constexpr (std::is_signed_v<int_t>) {
                if (nValue < 0) {
                    sOutput.put(static_cast<char_t>('-'));
                    nValue = -nValue;
                }
            }

            // Info about log10
            auto nValueCopy = nValue / 10;
            auto nDigits = 1;
            auto nExp = 1;

            // Calc log 10
            while (nValueCopy > 0) {
                nValueCopy /= 10;
                nDigits++;
                nExp *= 10;
            }

            // Put leading zeros
            if (nDigits < nMinCiphers) {
                for (std::uint8_t i = nDigits; i < nMinCiphers; ++i) {
                    sOutput.put(static_cast<char_t>('0'));
                }
            }

            // Start divisions
            while (nExp > 0) {
                // Process cipher
                auto nCipher = nValue / nExp + '0';
                sOutput.put(static_cast<char_t>(nCipher));

                // Prepare next iteration
                nValue %= nExp;
                nExp /= 10;
            }
        }

        template<typename char_t>
        void toIso3339(std::basic_ostream<char_t> &sOutput, const time_point nTime) {
            // Calculate times
            const auto nDays = std::chrono::floor<std::chrono::days>(nTime);
            const std::chrono::year_month_day nDate{nDays};
            const auto nInstantMillis = std::chrono::floor<std::chrono::milliseconds>(nTime - nDays);
            const std::chrono::hh_mm_ss nInstant{nInstantMillis};

            // Extract time components
            const int nYears = static_cast<int>(nDate.year());
            const unsigned nMonth = static_cast<unsigned>(nDate.month());
            const unsigned nDay = static_cast<unsigned>(nDate.day());
            const unsigned nHour = nInstant.hours().count();
            const unsigned nMinute = nInstant.minutes().count();
            const unsigned nSecond = nInstant.seconds().count();
            const unsigned nMillis = nInstant.subseconds().count();

            // Write date
            writeInteger<char_t, int>(sOutput, nYears, 4);
            sOutput.put(static_cast<char_t>('-'));
            writeInteger<char_t, unsigned>(sOutput, nMonth, 2);
            sOutput.put(static_cast<char_t>('-'));
            writeInteger<char_t, unsigned>(sOutput, nDay, 2);
            sOutput.put(static_cast<char_t>(' '));

            // Write hour
            writeInteger<char_t, unsigned>(sOutput, nHour, 2);
            sOutput.put(static_cast<char_t>(':'));
            writeInteger<char_t, unsigned>(sOutput, nMinute, 2);
            sOutput.put(static_cast<char_t>(':'));
            writeInteger<char_t, unsigned>(sOutput, nSecond, 2);
            sOutput.put(static_cast<char_t>('.'));
            writeInteger<char_t, unsigned>(sOutput, nMillis, 3);
            sOutput.put(static_cast<char_t>('Z'));
        }

        template<typename char_t>
        void toIso3339Local(std::basic_ostream<char_t> &sOutput, const time_point nTime) {
            // Get time zone
            const auto pZone = TIME_ZONE;
            const auto oZoneInfo = pZone->get_info(nTime);
            const auto nLocalTime = pZone->to_local(nTime);

            // Calculate times
            const auto nDays = std::chrono::floor<std::chrono::days>(nLocalTime);
            const std::chrono::year_month_day nDate{nDays};
            const auto nInstantMillis = std::chrono::floor<std::chrono::milliseconds>(nLocalTime - nDays);
            const std::chrono::hh_mm_ss nInstant{nInstantMillis};

            // Extract time components
            const int nYears = static_cast<int>(nDate.year());
            const unsigned nMonth = static_cast<unsigned>(nDate.month());
            const unsigned nDay = static_cast<unsigned>(nDate.day());
            const unsigned nHour = nInstant.hours().count();
            const unsigned nMinute = nInstant.minutes().count();
            const unsigned nSecond = nInstant.seconds().count();
            const unsigned nMillis = nInstant.subseconds().count();

            // Write date
            writeInteger<char_t, int>(sOutput, nYears, 4);
            sOutput.put(static_cast<char_t>('-'));
            writeInteger<char_t, unsigned>(sOutput, nMonth, 2);
            sOutput.put(static_cast<char_t>('-'));
            writeInteger<char_t, unsigned>(sOutput, nDay, 2);
            sOutput.put(static_cast<char_t>(' '));

            // Write hour
            writeInteger<char_t, unsigned>(sOutput, nHour, 2);
            sOutput.put(static_cast<char_t>(':'));
            writeInteger<char_t, unsigned>(sOutput, nMinute, 2);
            sOutput.put(static_cast<char_t>(':'));
            writeInteger<char_t, unsigned>(sOutput, nSecond, 2);
            sOutput.put(static_cast<char_t>('.'));
            writeInteger<char_t, unsigned>(sOutput, nMillis, 3);

            // Handle the Offset (±HH:mm)
            auto nOffsetSecs = oZoneInfo.offset.count();
            if (nOffsetSecs >= 0) {
                sOutput.put(static_cast<char_t>('+'));
            } else {
                sOutput.put(static_cast<char_t>('-'));
                nOffsetSecs = -nOffsetSecs;
            }

            // Calculate offset value
            const auto nOffsetTotalMinutes = static_cast<unsigned>(nOffsetSecs / 60);
            const unsigned nOffsetHours = nOffsetTotalMinutes / 60;
            const unsigned nOffsetMins = nOffsetTotalMinutes % 60;

            // Write offset value
            writeInteger<char_t, int>(sOutput, static_cast<unsigned>(nOffsetHours), 2);
            sOutput.put(static_cast<char_t>(':'));
            writeInteger<char_t, int>(sOutput, static_cast<unsigned>(nOffsetMins), 2);
        }
    }
#endif
}

#endif
