//
// Created by Curtis on 30/10/26.
//

#ifndef MATCHINGENGINE_ORDERPARSER_H
#define MATCHINGENGINE_ORDERPARSER_H

#include "orders.h"

#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

template <typename T>
T parseNumber(std::string_view field) {
    T value{};
    const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
    if (error != std::errc{} || end != field.data() + field.size()) {
        throw std::runtime_error("invalid number");
    }
    return value;
}

inline std::uint64_t priceTicks(std::string_view price) {
    const std::size_t dot = price.find('.');
    if (dot == std::string_view::npos || dot + 3 != price.size()) {
        throw std::runtime_error("expected price with two decimal places: " + std::string(price));
    }
    // One dollar is 1,000 ticks; the input file uses cents.
    return parseNumber<std::uint64_t>(price.substr(0, dot)) * 1000
         + parseNumber<std::uint64_t>(price.substr(dot + 1)) * 10;
}

inline OrderMessage parseOrder(const std::string& line) {
    const std::string_view input(line);
    if (input.starts_with("CANCEL,")) {
        return CancelOrder{{0, parseNumber<std::uint64_t>(input.substr(7))}};
    }

    // Fields are views into the line, so splitting does not copy their text.
    std::array<std::string_view, 6> fields;
    std::size_t start = 0;
    for (std::size_t i = 0; i < 5; ++i) {
        const std::size_t comma = input.find(',', start);
        if (comma == std::string_view::npos) throw std::runtime_error("invalid order line: " + line);
        fields[i] = input.substr(start, comma - start);
        start = comma + 1;
    }
    fields[5] = input.substr(start);
    if (fields[0] != "ADD" || (fields[3] != "BID" && fields[3] != "ASK")) {
        throw std::runtime_error("invalid order line: " + line);
    }

    return AddOrder{
        .header = {0, parseNumber<std::uint64_t>(fields[1])},
        .side = fields[3] == "BID" ? BID : ASK,
        .ticker = std::string(fields[2]),
        .price_ticks = priceTicks(fields[4]),
        .quantity = parseNumber<std::uint32_t>(fields[5])
    };
}

#endif //MATCHINGENGINE_ORDERPARSER_H
