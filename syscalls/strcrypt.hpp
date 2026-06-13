#pragma once
#include <string>
#include <array>

namespace crypt
{
    constexpr auto key = 0x57; // Compile-time key

    template<size_t N>
    struct encrypted {
        char data[N];
        
        constexpr encrypted(const char(&s)[N]) {
            for (size_t i = 0; i < N; ++i) {
                data[i] = s[i] ^ key;
            }
        }
    };

    template<size_t N>
    inline std::string decrypt(const encrypted<N>& e) {
        std::string s;
        s.resize(N - 1);
        for (size_t i = 0; i < N - 1; ++i) {
            s[i] = e.data[i] ^ key;
        }
        return s;
    }

    template<size_t N>
    inline std::wstring decrypt_w(const encrypted<N>& e) {
        std::wstring s;
        s.resize(N - 1);
        for (size_t i = 0; i < N - 1; ++i) {
            s[i] = static_cast<wchar_t>(e.data[i] ^ key);
        }
        return s;
    }
}
