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

namespace CppTrail {
    namespace Debug {
        /**
         * String view type for testing.
         * In C++ version <17 uses std::string instead of std::string_view
         */
#if __cplusplus >= 201703L
        using string_view_type = typename std::string_view;
#else
        using string_view_type = typename std::string;
#endif

        /**
         * @struct Test
         * @brief Singleton-style test runner.
         */
        struct Test {
            /** @brief Executes the full suite of CppTrail tests. */
            static void test() {
                Test oTest;
                oTest.testSyncOstreamLog();
                oTest.testAsyncOstreamLog();
                oTest.testDate();
            }

            /** @brief Tests sync logging by comparing full string blocks. */
            void testSyncOstreamLog() {
                std::ostringstream oOut;
                SyncOstreamLogger oLogger{oOut};
                testLogEntries(oLogger, oOut);
                this->m_oLogger.info("test::syncostreamlog");
            }

            /** @brief Tests async logging by verifying all entries exist in the stream. */
            void testAsyncOstreamLog() {
                std::ostringstream oOut1;
                AsyncOstreamLogger oLogger1{oOut1};
                testLogEntries(oLogger1, oOut1);
                std::ostringstream oOut2;
                AsyncOstreamLogger oLogger2{oOut2, false, 16, true};
                testLogEntries(oLogger2, oOut2);
                this->m_oLogger.info( "test::asyncostreamlog");
            }

            /**
             * @brief Tests logger logging by verifying all entries exist in the stream
             * @param oLogger Logger to test
             * @param oOut String stream where the logger outputs
             */
            static void testLogEntries(Logger &oLogger, std::ostringstream &oOut) {
                // Start logger
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

                // Stop logger
                oLogger.stop();

                // Vector of entries
                std::vector<string_view_type> vResult{
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
                for (const auto &sEntry: vResult) {
                    assert(sLog.find(sEntry) != string_view_type::npos);
                }
            }

            /** @brief Tests ISO 3339 formatting and Timezone "cheating". */
            void testDate() {
#if __cplusplus >= 202002L
                Detail::time_point oTimePoint{}; // Epoch: 1970-01-01

                std::ostringstream o3339;
                Detail::toRfc3339Utc<char>(o3339, oTimePoint);
                assert(o3339.view() == "1970-01-01 00:00:00.000Z");

                // Force Zulu for local test
                Detail::TIME_ZONE = std::chrono::locate_zone("UTC");
                std::ostringstream o3339l;
                Detail::toRfc3339Local<char>(o3339l, oTimePoint);
                assert(o3339l.view() == "1970-01-01 00:00:00.000+00:00");
#endif
                this->m_oLogger.log(Level::INFO, "test::datetime");
            }

        private:
            SyncCoutLogger m_oLogger; ///< Real-world logger for test reporting.
        };
    }
}

#endif
