#ifndef ARN_UTIL_BITS_HPP
#define ARN_UTIL_BITS_HPP
#include <vector>
#include <cstdint>

struct Bits {
    std::vector<uint8_t> b; // 0/1

    explicit Bits(size_t n = 0) : b(n, 0) {}

    void set(size_t i) {
        if (i < b.size()) b[i] = 1;
    }

    [[nodiscard]] bool get(size_t i) const {
        return (i < b.size()) && (b[i] != 0);
    }

    [[nodiscard]] size_t size() const { return b.size(); }

    void or_with(const Bits& oth) {
        const size_t n = std::min(b.size(), oth.b.size());
        for (size_t i = 0; i < n; ++i) b[i] = uint8_t(b[i] | oth.b[i]);
    }
};

inline bool is_compatible(const Bits& s1, const Bits& s2) {
    for (size_t i = 0; i < s1.size(); ++i) {
        if (!s1.get(i))
            continue;

        if (i > 0 && s2.get(i - 1))
            return false;
        if (s2.get(i + 1))
            return false;
    }
    return true;
}


#endif //ARN_UTIL_BITS_HPP
