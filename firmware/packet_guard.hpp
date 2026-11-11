#pragma once
#include <cstddef>
#include <cstdint>
namespace probe {
// Check both request consumption and worst-case response BEFORE upstream parsing.
bool valid_packet(const std::uint8_t *p, std::size_t size);
}
