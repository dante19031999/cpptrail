/**
 * @file base_logger.h
 * @brief ???
 * @author Dante Doménech Martínez
 * @copyright GPL-3 License
 */

#pragma once

#include <cstdint>
#include <ostream>

#if __cplusplus >= 202002L
#include <chrono>
#include <type_traits>
#endif

namespace CppTrail {
    namespace Detail {
#if __cplusplus >= 202002L
        /// Time point type for CppTrail
        using time_point = std::chrono::time_point<std::chrono::system_clock>;

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
        void toRfc3339Utc(std::basic_ostream<char_t> &sOutput, const time_point nTime) {
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
        void toRfc3339Local(std::basic_ostream<char_t> &sOutput, const time_point nTime) {
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

        template<typename char_t>
        void toRfc3339(
            std::basic_ostream<char_t> &sOutput,
            bool bUseLocalTIme
        ) {
            // Write time
            auto nTime = std::chrono::system_clock::now();
            if (bUseLocalTIme)
                Detail::toRfc3339Local<char_t>(sOutput, nTime);
            else
                Detail::toRfc3339Utc<char_t>(sOutput, nTime);
            sOutput.put(static_cast<char_t>(' '));
        }
#endif
    }
}
