#pragma once

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace InventoryTrade {

template<std::size_t Count>
inline std::int64_t ProtectionTotal(const int (&protection)[Count]) {
    // Both averages have the same denominator, so compare their sums exactly.
    return std::accumulate(protection, protection + Count, std::int64_t{0});
}

inline int SalePrice(int value, float multiplier) {
    if (value <= 0 || !std::isfinite(multiplier) || multiplier < 0) return 0;
    const float product = static_cast<float>(static_cast<double>(value) * multiplier);
    const auto rounded = std::nearbyint(static_cast<double>(product));
    if (!std::isfinite(rounded) || rounded > SHRT_MAX) return 0;
    return (std::max)(1, static_cast<int>(rounded));
}

inline int Affordable(int stock, int unitPrice, std::int64_t gold) {
    if (stock <= 0 || unitPrice < 0 || gold < 0) return 0;
    return unitPrice == 0 ? stock : static_cast<int>((std::min)(std::int64_t(stock), gold / unitPrice));
}

} // namespace InventoryTrade
