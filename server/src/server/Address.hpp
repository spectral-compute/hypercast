#pragma once

#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <cstdint>

namespace Server
{

/**
 * Represents an IP address (IPv4 or IPv6) and optionally a network prefix length.
 *
 * This object always represents an IPv6 address. If it's constructed with an IPv4 address, it's converted to the
 * IPv4-mapped address range @::ffff:0.0.0.0/96.
 */
class Address final
{
public:
    /**
     * Create the all-zeros address.
     */
    Address() = default;

    /**
     * Construct an address from network-order bytes.
     *
     * @param bytes The bytes representing the address. Must be either 4 or 16 bytes long.
     */
    Address(std::span<const std::byte> bytes);

    /**
     * Construct an address with network prefix from network-order bytes.
     *
     * @param bytes The bytes representing the address. Must be either 4 or 16 bytes long.
     * @param prefixLength The prefix length. Must be at most 32 if bytes is of size 4, or 128 if bytes is of length 16.
     */
    Address(std::span<const std::byte> bytes, uint8_t prefixLength);

    /**
     * Construct an address from its string representation.
     *
     * @param representation The string representation of the IP address.
     * @param allowPrefixLength Whether to allow a prefix length.
     * @param allowAddressOnly Whether to allow an IP address without a prefix length.
     */
    Address(std::string_view representation, bool allowPrefixLength = false, bool allowAddressOnly = true);

    auto operator<=>(const Address &) const = default;

    /**
     * Make a string representation of the IP address and (if present) prefix length.
     */
    operator std::string() const;

    /**
     * Determine if every address in the range is an IPv4 or IPv6 loopback.
     *
     * @return True if the address is a loopback address, and false otherwise.
     */
    bool isLoopback() const;

    /**
     * Determine if this address-range contains another.
     *
     * The range of addresses represented by an Address object are those formed by the network represented by its
     * address and prefix length. If there is no prefix length, then the range contains only one address.
     *
     * @param other The address-range to see if we contain.
     * @return True if every address in the range represented by other is also in the range represented by this object.
     */
    bool contains(const Address &other) const;

private:
    /**
     * A special value used in prefixLength to indicate that there's no prefix length.
     */
    static constexpr uint8_t noPrefixLength = 0xFF;

    /**
     * The network-order bytes representing the address.
     */
    std::byte bytes[16] = {};

    /**
     * The prefix length, if any.
     *
     * Set to noPrefixLength if there's no prefix length.
     */
    uint8_t prefixLength = noPrefixLength;
};

} // namespace Server
