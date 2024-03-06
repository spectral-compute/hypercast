#include "Address.hpp"

#include "util/debug.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>

#include <cstring>
#include <stdexcept>

namespace
{

/**
 * Convert the bytes of an IPv4 address to bytes of an IPv6 address.
 *
 * @param dst A pointer to the bytes of the IPv6 address to create. Must be 16 bytes in size.
 * @param src A pointer to the bytes of the IPv4 address to convert. Must be 4 bytes in size.
 */
void convertIPv4ToIPv6(std::byte *dst, const std::byte *src)
{
    memset(dst, 0, 10);
    memset(dst + 10, 0xFF, 2);
    memcpy(dst + 12, src, 4);
}

} // namespace

Server::Address::Address(std::span<const std::byte> bytes)
{
    /* Handle the IPv4 case by turning it into an IPv6 address. */
    if (bytes.size() == 4) {
        convertIPv4ToIPv6(this->bytes, bytes.data());
    }

    /* For IPv6, just copy the address. */
    else if (bytes.size() == sizeof(this->bytes)) {
        memcpy(this->bytes, bytes.data(), sizeof(this->bytes));
    }

    /* Any other length is an error. */
    else {
        unreachable();
    }
}

Server::Address::Address(std::span<const std::byte> bytes, uint8_t prefixLength) : Address(bytes)
{
    assert(prefixLength <= 32 || (bytes.size() == 16 && prefixLength <= 128));
    this->prefixLength = (int)prefixLength;
}

Server::Address::Address(std::string_view representation, bool allowPrefixLength, bool allowAddressOnly)
{
    /* Somewhere compatible with boost's representation of the address bytes. */
    boost::asio::ip::address_v6::bytes_type addressBytes;
    assert(addressBytes.size() == sizeof(bytes));

    /* Handle the case of a network with prefix length. */
    if (representation.find('/') != std::string_view::npos) {
        // Check that this is allowed.
        if (!allowPrefixLength) {
            throw std::runtime_error("IP address has network prefix length but shouldn't.");
        }

        // Parse the input string as an IPv6 network.
        if (representation.find(':') != std::string_view::npos) {
            // Parse.
            boost::asio::ip::network_v6 network = boost::asio::ip::make_network_v6(representation);

            // Extract the address bytes.
            addressBytes = network.address().to_bytes();

            // Get the network prefix.
            prefixLength = network.prefix_length();
            assert(prefixLength <= 128);
        }

        // Parse the input string as an IPv4 network, and convert it to IPv6.
        else {
            // Parse.
            boost::asio::ip::network_v4 network = boost::asio::ip::make_network_v4(representation);

            // Extract the address bytes and convert them to IPv6.
            boost::asio::ip::address_v4::bytes_type addressV4Bytes = network.address().to_bytes();
            convertIPv4ToIPv6((std::byte *)addressBytes.data(), (const std::byte *)addressV4Bytes.data());

            // Get the network prefix and adjust it to be a sub-prefix of the IPv6 prefix for IPv4-mapped addresses.
            prefixLength = 96 + network.prefix_length();
            assert(prefixLength <= 128);
        }
    }

    /* Handle the case where there's no prefix length. */
    else {
        // Check that we're allowed an address with no prefix.
        if (!allowAddressOnly) {
            throw std::runtime_error("IP address range has no network prefix length.");
        }

        // Get the address bytes.
        boost::asio::ip::address address = boost::asio::ip::make_address(representation);

        // Convert to IPv6 if necessary.
        if (address.is_v4()) {
            boost::asio::ip::address_v4::bytes_type addressV4Bytes = address.to_v4().to_bytes();
            convertIPv4ToIPv6((std::byte *)addressBytes.data(), (const std::byte *)addressV4Bytes.data());
        }

        // Otherwise, just exctract IPv6 the addresss's bytes
        else {
            assert(address.is_v6());
            addressBytes = address.to_v6().to_bytes();
        }
    }

    /* Set the address bytes. */
    memcpy(bytes, addressBytes.data(), sizeof(bytes));
}

Server::Address::operator std::string() const
{
    /* Construct a Boost IPv6 address. */
    boost::asio::ip::address_v6::bytes_type addressBytes;
    assert(addressBytes.size() == sizeof(bytes));

    memcpy(addressBytes.data(), bytes, addressBytes.size());
    boost::asio::ip::address_v6 address(addressBytes);

    /* Convert the address component to a string. */
    std::string addressString = address.to_string();

    /* If we have no prefix length, then the address is all there is. */
    if (prefixLength == noPrefixLength) {
        return addressString;
    }

    /* Append the prefix length. */
    return addressString + "/" + std::to_string(prefixLength);
}

bool Server::Address::isLoopback() const
{
    /* All of both the IPv4 (in the IPv4-mapped range) and IPv6 loopback addresses start with 10 zero bytes. */
    for (size_t i = 0; i < 10; i++) {
        if (bytes[i] != (std::byte)0) {
            return false;
        }
    }

    /* IPv4-mapped addresses. */
    if (bytes[10] == (std::byte)0xFF && bytes[11] == (std::byte)0xFF) {
        return bytes[12] == (std::byte)127 && prefixLength >= 104; // Includes noPrefixLength.
    }

    /* There's only one IPv6 loopback address. */
    // Therefore, if the prefix length is not 128 or noPrefixLength, then there'll be a non-loopback address.
    if (prefixLength < 128) {
        return false;
    }

    // Check that the remaining bytes are correct.
    for (size_t i = 10; i < 15; i++) {
        if (bytes[i] != (std::byte)0) {
            return false;
        }
    }
    return bytes[15] == (std::byte)1;
}

bool Server::Address::contains(const Address &other) const
{
    /* This network can't contain a larger one. */
    if (other.prefixLength < prefixLength) {
        return false;
    }

    /* Figure out which bits and bytes to compare. */
    unsigned int numWholeBytes = prefixLength / 8;
    unsigned int numBitsInExtraByte = prefixLength % 8;
    assert(numWholeBytes <= sizeof(bytes));
    assert(numWholeBytes < sizeof(bytes) || numBitsInExtraByte == 0);

    /* Compare the whole bytes. */
    if (memcmp(bytes, other.bytes, numWholeBytes) != 0) {
        return false;
    }

    /* Compare the remaining bits in the extra byte, if any. */
    if (numBitsInExtraByte == 0) {
        return true;
    }
    std::byte mask = ~(std::byte)((1 << numBitsInExtraByte) - 1);
    return (bytes[numWholeBytes] & mask) == (other.bytes[numWholeBytes] & mask);
}
