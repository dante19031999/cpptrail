/**
 * @file test.h
 * @brief Unit testing suite for the CppTrail logging framework.
 * @details contains specialized mock loggers and a test runner to validate
 * synchronous/asynchronous throughput and RFC 3339 date formatting.
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#ifndef CPPTRAIL_TEST_H
#define CPPTRAIL_TEST_H

#include <cassert>
#include <sstream>
#include <vector>
#include "cpptrail/ostream_logger.h"

namespace CppTrail::Debug {
    /**
     * @class BasicTestSyncLogger
     * @brief A minimal synchronous logger for regression testing.
     * @details Bypasses timestamp formatting to allow for direct string
     * comparison of log levels and messages.
     * @tparam char_t The character type (e.g., char, wchar_t).
     */
    template<typename char_t>
    class BasicTestSyncLogger : public BasicLogger<char_t> {
    public:
        /**
         * @brief Constructs a sync test logger.
         * @param oOstream The output stream to capture logs.
         */
        BasicTestSyncLogger(std::basic_ostream<char_t> &oOstream)
            : BasicTestSyncLogger(std::reference_wrapper(oOstream)) {
        }

        /** @brief Internal handle constructor. */
        BasicTestSyncLogger(std::reference_wrapper<std::basic_ostream<char_t> > oOstream)
            : BasicLogger<char_t>(std::make_shared<Impl>(oOstream)) {
        }

    private:
        /** @brief Implementation that skips timestamps for easier assertion. */
        class Impl : public BasicSyncLoggerImpl<char_t> {
        public:
            using string_type = typename BasicLoggerImpl<char_t>::string_type;

            Impl(std::reference_wrapper<std::basic_ostream<char_t> > oOstream) : m_oOstream(oOstream) {
            }

            ~Impl() override {
                this->stop();
            }

        protected:
            /** @brief Writes raw 'LEVEL Message' format. */
            void work(string_type sLevel, string_type sMessage) override {
                std::basic_ostream<char_t> &oOstream = m_oOstream.get();
                oOstream << sLevel << static_cast<char_t>(' ') << sMessage << static_cast<char_t>('\n');
                oOstream.flush();
            }

            Status serviceStatus() override { return Status::TRASCENDENT; }

            void serviceStart() override {
            }

            void serviceStop() override {
            }

        private:
            std::reference_wrapper<std::basic_ostream<char_t> > m_oOstream;
        };
    };

    /**
     * @class BasicTestAsyncLogger
     * @brief A mock logger to test the asynchronous infrastructure.
     * @details Inherits from BasicSyncLoggerImpl for this test iteration to
     * ensure sequential capture while validating the interface.
     */
    template<typename char_t>
    class BasicTestAsyncLogger : public BasicLogger<char_t> {
    public:
        /// Simple constructor with ostream&
        BasicTestAsyncLogger(std::basic_ostream<char_t> &oOstream)
            : BasicTestAsyncLogger(std::reference_wrapper(oOstream)) {
        }

        /// Simple constructor with ostream reference
        BasicTestAsyncLogger(std::reference_wrapper<std::basic_ostream<char_t> > oOstream)
            : BasicLogger<char_t>(std::make_shared<Impl>(oOstream)) {
        }

    private:
        class Impl : public BasicAsyncLoggerImpl<char_t> {
        public:
            using string_type = typename BasicLoggerImpl<char_t>::string_type;

            Impl(std::reference_wrapper<std::basic_ostream<char_t> > oOstream)
                : m_oOstream(oOstream),
                  m_nStatus(Status::STOPPED) {
            }

            ~Impl() override {
                this->stop();
                this->join();
            }

        protected:
            void work(string_type sLevel, string_type sMessage) override {
                std::basic_ostream<char_t> &oOstream = m_oOstream.get();
                oOstream << sLevel << static_cast<char_t>(' ') << sMessage << static_cast<char_t>('\n');
                oOstream.flush();
            }

            Status serviceStatus() override {
                return this->m_nStatus;
            }

            void serviceStart() override {
                this->m_nStatus = Status::RUNNING;
            }

            void serviceStop() override {
                this->m_nStatus = Status::STOPPED;
            }

        private:
            std::reference_wrapper<std::basic_ostream<char_t> > m_oOstream;
            Status m_nStatus;
        };
    };

    /**
     * @struct Test
     * @brief Singleton-style test runner.
     */
    struct Test {
        /** @brief Executes the full suite of CppTrail tests. */
        static void test() {
            Test oTest;
            oTest.testSyncLog();
            oTest.testAsyncLog();
            oTest.testSyncOstreamLog();
            oTest.testAsyncOstreamLog();
            oTest.testDate();
        }

        /** @brief Tests sync logging by comparing full string blocks. */
        void testSyncLog() {
            std::ostringstream oOut;
            BasicTestSyncLogger oLogger{oOut};
            oLogger.log(Level::SUCCESS, "Message SUCCESS")
                    .log(Level::TRACE, "Message TRACE")
                    .log(Level::DEBUG, "Message DEBUG")
                    .log(Level::INFO, "Message INFO")
                    .log(Level::MESSAGE, "Message MESSAGE")
                    .log(Level::WARNING, "Message WARNING")
                    .log(Level::ERROR, "Message ERROR")
                    .log(Level::CRITICAL, "Message CRITICAL")
                    .log(Level::FATAL, "Message FATAL")
                    .log(std::string{"CUSTOM"}, "Message CUSTOM");

            std::string_view sResult{
                "SUCCESS Message SUCCESS\n"
                "TRACE Message TRACE\n"
                "DEBUG Message DEBUG\n"
                "INFO Message INFO\n"
                "MESSAGE Message MESSAGE\n"
                "WARNING Message WARNING\n"
                "ERROR Message ERROR\n"
                "CRITICAL Message CRITICAL\n"
                "FATAL Message FATAL\n"
                "CUSTOM Message CUSTOM\n"
            };

            assert(sResult == oOut.str());
            this->m_oLogger.log(Level::INFO, "test::synclog");
        }

        /** @brief Tests async logging by verifying all entries exist in the stream. */
        void testAsyncLog() {
            std::ostringstream oOut;
            BasicTestAsyncLogger oLogger{oOut};

            // Start log
            oLogger.start();

            // Log
            oLogger.log(Level::SUCCESS, "Message SUCCESS")
                    .log(Level::TRACE, "Message TRACE")
                    .log(Level::DEBUG, "Message DEBUG")
                    .log(Level::INFO, "Message INFO")
                    .log(Level::MESSAGE, "Message MESSAGE")
                    .log(Level::WARNING, "Message WARNING")
                    .log(Level::ERROR, "Message ERROR")
                    .log(Level::CRITICAL, "Message CRITICAL")
                    .log(Level::FATAL, "Message FATAL")
                    .log(std::string{"CUSTOM"}, "Message CUSTOM");

            // Stop log (wait to finish)
            oLogger.stop();

            std::vector<std::string_view> vResult{
                "SUCCESS Message SUCCESS\n", "TRACE Message TRACE\n",
                "DEBUG Message DEBUG\n", "INFO Message INFO\n",
                "MESSAGE Message MESSAGE\n", "WARNING Message WARNING\n",
                "ERROR Message ERROR\n", "CRITICAL Message CRITICAL\n",
                "FATAL Message FATAL\n", "CUSTOM Message CUSTOM\n"
            };

            const std::string sLog = oOut.str();
            for (const auto sEntry: vResult) {
                assert(sLog.find(sEntry) != std::string_view::npos);
            }
            this->m_oLogger.log(Level::INFO, "test::asynclog");
        }

        /** @brief Tests sync logging by comparing full string blocks. */
        void testSyncOstreamLog() {
            std::ostringstream oOut;
            OstreamLogger oLogger{oOut};
            oLogger.log(Level::SUCCESS, "Message SUCCESS")
                    .log(Level::TRACE, "Message TRACE")
                    .log(Level::DEBUG, "Message DEBUG")
                    .log(Level::INFO, "Message INFO")
                    .log(Level::MESSAGE, "Message MESSAGE")
                    .log(Level::WARNING, "Message WARNING")
                    .log(Level::ERROR, "Message ERROR")
                    .log(Level::CRITICAL, "Message CRITICAL")
                    .log(Level::FATAL, "Message FATAL")
                    .log(std::string{"CUSTOM"}, "Message CUSTOM");

            std::vector<std::string_view> vResult{
                "SUCCESS Message SUCCESS\n",
                "TRACE Message TRACE\n",
                "DEBUG Message DEBUG\n",
                "INFO Message INFO\n",
                "MESSAGE Message MESSAGE\n",
                "WARNING Message WARNING\n",
                "ERROR Message ERROR\n",
                "CRITICAL Message CRITICAL\n",
                "FATAL Message FATAL\n",
                "CUSTOM Message CUSTOM\n"
            };

            // Test limited by datetime, just look for the messages
            const std::string sLog = oOut.str();
            for (const auto sEntry: vResult) {
                assert(sLog.find(sEntry) != std::string_view::npos);
            }
            this->m_oLogger.log(Level::INFO, "test::syncostreamlog");
        }

        /** @brief Tests async logging by verifying all entries exist in the stream. */
        void testAsyncOstreamLog() {
            std::ostringstream oOut;
            AsyncOstreamLogger oLogger{oOut};

            // Start log
            oLogger.start();

            // Log
            oLogger.log(Level::SUCCESS, "Message SUCCESS")
                    .log(Level::TRACE, "Message TRACE")
                    .log(Level::DEBUG, "Message DEBUG")
                    .log(Level::INFO, "Message INFO")
                    .log(Level::MESSAGE, "Message MESSAGE")
                    .log(Level::WARNING, "Message WARNING")
                    .log(Level::ERROR, "Message ERROR")
                    .log(Level::CRITICAL, "Message CRITICAL")
                    .log(Level::FATAL, "Message FATAL")
                    .log(std::string{"CUSTOM"}, "Message CUSTOM");

            // Stop log (wait to finish)
            oLogger.stop();

            std::vector<std::string_view> vResult{
                "SUCCESS Message SUCCESS\n",
                "TRACE Message TRACE\n",
                "DEBUG Message DEBUG\n",
                "INFO Message INFO\n",
                "MESSAGE Message MESSAGE\n",
                "WARNING Message WARNING\n",
                "ERROR Message ERROR\n",
                "CRITICAL Message CRITICAL\n",
                "FATAL Message FATAL\n",
                "CUSTOM Message CUSTOM\n"
            };

            // Test limited by datetime, just look for the messages
            const std::string sLog = oOut.str();
            for (const auto sEntry: vResult) {
                assert(sLog.find(sEntry) != std::string_view::npos);
            }
            this->m_oLogger.log(Level::INFO, "test::asyncostreamlog");
        }

        /** @brief Tests ISO 3339 formatting and Timezone "cheating". */
        void testDate() {
#if __cplusplus >= 202002L
            Detail::time_point oTimePoint{}; // Epoch: 1970-01-01

            std::ostringstream o3339;
            Detail::toIso3339<char>(o3339, oTimePoint);
            assert(o3339.view() == "1970-01-01 00:00:00.000Z");

            // Force Zulu for local test
            Detail::TIME_ZONE = std::chrono::locate_zone("UTC");
            std::ostringstream o3339l;
            Detail::toIso3339Local<char>(o3339l, oTimePoint);
            assert(o3339l.view() == "1970-01-01 00:00:00.000+00:00");
#endif
            this->m_oLogger.log(Level::INFO, "test::datetime");
        }

    private:
        CoutLogger m_oLogger; ///< Real-world logger for test reporting.
    };
}

#endif
