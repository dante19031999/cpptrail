/**
 * @file utf42.h
 * @brief Compile-time polymorphic string literal selection across character encodings.
 *
 * This header provides a mechanism to define a single string literal and
 * retrieve it as a `std::basic_string_view` of a desired character type
 * (`char`, `wchar_t`, `char8_t`, `char16_t`, or `char32_t`) at compile time.
 *
 * The approach relies on the compiler to generate all encoding variants
 * of a string literal (via literal prefixes such as `L`, `u8`, `u`, and `U`)
 * and selects the appropriate one using `consteval` dispatch.
 *
 * This guarantees:
 * - Zero runtime overhead
 * - No heap allocation
 * - No UTF transcoding at runtime
 * - Full support for `\uXXXX` and `\UXXXXXXXX` escape sequences
 *
 * @warning The macro `make_poly_enc` must be used with a string literal.
 *          Passing non-literals results in undefined behavior.
 *
 * @note Requires C++11 or later.
 * Additional features are available with higher C++ versions.
 *
 * @copyright MIT License
 *
 * This file is part of utf42.
 * Copyright (c) 2025 Dante Doménech Martínez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef LIB_UTF_42
#define LIB_UTF_42

#include <type_traits>
#include <cstddef>

// Use std::basic_string_view if C++17 or custom basic_string_view otherwise
#if   __cplusplus >= 201703L
#include <string_view>
#else
#include <string>
#endif

// C++ version requirements
#if __cplusplus < 201103L
#pragma error "C++ minimum version required is 11"
#endif

/**
 * @brief Creates a compile-time polymorphic encoded string literal.
 *
 * This macro generates all standard character-encoded versions of the
 * provided string literal and selects the one matching `char_t`.
 *
 * @param char_t Desired character type (`char`, `wchar_t`, `char8_t` (if C++20),
 *               `char16_t`, or `char32_t`).
 * @param lit A string literal.
 *
 * @return A `std::basic_string_view<char_t>` referring to the selected literal.
 */
#if __cplusplus >= 202002L

#define make_poly_enc(char_t, lit) utf42::visit_poly_enc<char_t>( \
    utf42::poly_enc{ \
        lit, \
        L##lit, \
        u8##lit, \
        u##lit, \
        U##lit, \
    } \
)

#else

#define make_poly_enc(char_t, lit) utf42::visit_poly_enc<char_t>( \
    utf42::poly_enc{ \
        lit, \
        L##lit, \
        u##lit, \
        U##lit, \
    } \
)

#endif

/**
 * @brief Constructs a compile-time polymorphic encoded string literal view.
 *
 * This macro generates all standard character-encoded versions of the
 * provided string literal.
 *
 * @param lit A string literal.
 *
 * @return A `std::basic_string_view<char_t>` referring to the selected literal.
 */
#if __cplusplus >= 202002L

#define cons_poly_enc(lit) utf42::poly_enc{ \
    lit, \
    L##lit, \
    u8##lit, \
    u##lit, \
    U##lit, \
}

#else

#define cons_poly_enc(lit) utf42::poly_enc{ \
    lit, \
    L##lit, \
    u##lit, \
    U##lit, \
}

#endif

/**
 * @namespace utf42
 * @brief Main namespace of the library
 */
namespace utf42 {
    /**
     * @brief Type trait that checks whether a type is a supported character type.
     *
     * Supported types are:
     * - `char`
     * - `wchar_t`
     * - `char8_t`
     * - `char16_t`
     * - `char32_t`
     *
     * @tparam T Type to check.
     * @note Defined only if C++20 is available
     */
    template<typename T>
    struct is_character : std::integral_constant<bool,
                std::is_same<T, char>::value ||
                std::is_same<T, wchar_t>::value ||
#if __cplusplus >= 202002L
                std::is_same<T, char8_t>::value ||
#endif
                std::is_same<T, char16_t>::value ||
                std::is_same<T, char32_t>::value
            > {
    };

#if __cplusplus >= 201703L
    /**
     * @brief Convenience variable template for `is_char`.
     *
     * @tparam T Type to check.
     * @note Defined only if C++17 is available
     */
    template<typename T>
    constexpr bool is_character_v = is_character<T>::value;
#endif

#if __cplusplus >= 201703L

    /// String view type for poly_enc
    template<typename char_t>
    using basic_string_view = std::basic_string_view<char_t>;

#else

    /**
     * @brief A class that provides a lightweight, non-owning view of a string.
     *
     * This class allows for efficient string manipulation without ownership,
     * making it particularly useful for passing around substring references
     * or interfacing with C-style strings. It avoids unnecessary data copies
     * and allocations, enhancing performance in string handling operations.
     *
     * @warning This class is a very simple class to achieve the purpose of this library.
     * It does not contain any extra features. You may use your own custom containers to
     * treat the data. On C++ > 17 the standard std::basic_string_view is used on it's stead.
     *
     * @tparam char_t The character type (e.g., char, wchar_t) that the view will operate on.
     * @note Only available on C++14 and C++11
     */
    template<typename char_t>
    class basic_string_view {
    public:
        using pointer = const char_t *; ///< Pointer type to characters.
        using reference = const char_t &; ///< Reference type to characters.
        using char_type = char_t; ///< Character type.
        using size_type = std::size_t; ///< Type for sizes of the string.

        // Check char type
        static_assert(utf42::is_character<char_t>::value, "type_t must be a character.");

        /// Default constructor initializes to an empty string view.
        constexpr basic_string_view() : m_pData(""), m_nSize(0) {
        }

        /// Deleted constructor from nullptr to avoid unintended usage.
        constexpr basic_string_view(nullptr_t) = delete;

        /**
         * @brief Constructor from a C-style string.
         *
         * Initializes the string view with a given null-terminated string
         * and calculates its length.
         *
         * @param pStr Pointer to the null-terminated string.
         */
        template<std::size_t N>
        constexpr basic_string_view(const char_t (&pStr)[N])
            : m_pData(pStr), m_nSize(N - 1) {
            static_assert(N > 0, "basic_string_view: invalid length");
        }

        /**
         * @brief Get the length of the string.
         *
         * @return size_type The length of the string.
         */
        constexpr size_type length() const noexcept { return m_nSize; }

        /**
         * @brief Get a pointer to the underlying character data.
         *
         * @return pointer Pointer to the character data.
         */
        constexpr pointer data() const noexcept { return m_pData; }

        /**
         * @brief Convert to std::basic_string for further manipulation.
         *
         * @return std::basic_string<char_t> The string representation.
         */
        std::basic_string<char_t> str() const {
            return std::basic_string<char_t>(m_pData, m_nSize);
        }

    private:
        pointer m_pData; ///< Pointer to the character data.
        size_type m_nSize; ///< The size of the string view.
    };

#endif

#if __cplusplus >= 202002L

    /**
     * @brief Concept constraining a type to a supported character type.
     *
     * @tparam T Type to check.
     * @note Defined only if C++20 is available
     */
    template<typename T>
    concept CharacterType = is_character_v<T>;

    /**
     * @brief Concept constraining a type to an integral type.
     *
     * @tparam T Type to check.
     * @note Defined only if C++20 is available
     */
    template<typename T>
    concept IntegralType = std::is_integral_v<T>;

    /**
     * @brief Concept constraining a type to an floating point type.
     *
     * @tparam T Type to check.
     * @note Defined only if C++20 is available
     */
    template<typename T>
    concept FloatingPointType = std::is_floating_point_v<T>;
#endif

    /**
     * @brief Container holding all character-encoded views of a string literal.
     *
     * Instances of this type are intended to be constructed only from
     * string literals. Each member corresponds to a distinct literal
     * encoding generated by the compiler.
     *
     * No ownership is taken; all views refer directly to static storage.
     */
    struct poly_enc {
        // String literal fields
        basic_string_view<char> TXT_CHAR; ///< Narrow character literal
        basic_string_view<wchar_t> TXT_CHAR_W; ///< Wide character literal
#if __cplusplus >= 202002L
        basic_string_view<char8_t> TXT_CHAR_8; ///< UTF-8 character literal. Only defined if C++20 is available.
#endif
        basic_string_view<char16_t> TXT_CHAR_16; ///< UTF-16 character literal
        basic_string_view<char32_t> TXT_CHAR_32; ///< UTF-32 character literal

        /**
         * @brief Constructs a polymorphic encoding container.
         *
         * @param txt_char     Narrow character string view.
         * @param txt_char_w   Wide character string view.
         * @param txt_char_8   UTF-8 string view. Only defined if C++20 is available.
         * @param txt_char_16  UTF-16 string view.
         * @param txt_char_32  UTF-32 string view.
         */
        constexpr poly_enc(
            const basic_string_view<char> txt_char,
            const basic_string_view<wchar_t> txt_char_w,
#if __cplusplus >= 202002L
            const basic_string_view<char8_t> txt_char_8,
#endif
            const basic_string_view<char16_t> txt_char_16,
            const basic_string_view<char32_t> txt_char_32
        ) noexcept
            : TXT_CHAR(txt_char),
              TXT_CHAR_W(txt_char_w),
#if __cplusplus >= 202002L
              TXT_CHAR_8(txt_char_8),
#endif
              TXT_CHAR_16(txt_char_16),
              TXT_CHAR_32(txt_char_32) {
        }

        /**
         * @brief Selects the appropriate encoded string view for a given character type.
         *
         * This function is evaluated at compile time and returns a
         * `std::basic_string_view<char_t>` referring to the correctly
         * encoded literal stored in the provided `poly_enc`.
         * Returns an empty string if the character type is not recognized.
         *
         * @tparam char_t Desired character type.
         *
         * @return A string view of the requested character type or char_t"".
         */
#if __cplusplus >= 202002L
        template<CharacterType char_t>
        constexpr basic_string_view<char_t>
        visit() const noexcept;
#else
        template<typename char_t>
        constexpr basic_string_view<char_t>
        visit() const noexcept;
#endif
    };

    // Primary template for unsupported types
#if __cplusplus >= 202002L
    template<CharacterType char_t>
    constexpr basic_string_view<char_t> poly_enc::visit() const noexcept {
        return {};
    }
#else
    template<typename char_t>
    constexpr basic_string_view<char_t> poly_enc::visit() const noexcept {
        static_assert(false, "Unsupported character type");
        return {};
    }
#endif

    /// Specialization for char
    template<>
    constexpr basic_string_view<char> poly_enc::visit<char>() const noexcept {
        return TXT_CHAR;
    }

    /// Specialization for wchar_t
    template<>
    constexpr basic_string_view<wchar_t> poly_enc::visit<wchar_t>() const noexcept {
        return TXT_CHAR_W;
    }

#if __cplusplus >= 202002L
    /// Specialization for char8_t
    template<>
    constexpr basic_string_view<char8_t> poly_enc::visit<char8_t>() const noexcept {
        return TXT_CHAR_8;
    }
#endif

    /// Specialization for char16_t
    template<>
    constexpr basic_string_view<char16_t> poly_enc::visit<char16_t>() const noexcept {
        return TXT_CHAR_16;
    }

    /// Specialization for char32_t
    template<>
    constexpr basic_string_view<char32_t> poly_enc::visit<char32_t>() const noexcept {
        return TXT_CHAR_32;
    }

    /**
     * @brief Selects the appropriate encoded string view for a given character type.
     *
     * This function is evaluated at compile time and returns a
     * `std::basic_string_view<char_t>` referring to the correctly
     * encoded literal stored in the provided `poly_enc`.
     *
     * @tparam char_t Desired character type.
     * @param oPolyEnv Polymorphic encoding container.
     *
     * @return A string view of the requested character type.
     */
#if __cplusplus >= 202002L
    template<CharacterType char_t>
    consteval std::basic_string_view<char_t>
    visit_poly_enc(const poly_enc &oPolyEnv) {
        return oPolyEnv.visit<char_t>();
    }
#else
    template<typename char_t>
    constexpr basic_string_view<char_t>
    visit_poly_enc(const poly_enc &oPolyEnv) {
        return oPolyEnv.visit<char_t>();
    }
#endif
} // namespace utf42

// Clean up helper macro
#undef LIB_UTF_42_CHAR_TYPE

#endif //LIB_UTF_42