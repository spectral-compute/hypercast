#include "server/Address.hpp"

#include <gtest/gtest.h>

namespace
{

constexpr const char *testIPv6Address = "2001:db8:c0de::c0ff:ee";
constexpr const char *testIPv6Network = "2001:db8:c0de::c0ff:ee/64";

constexpr const char *testIPv4Address = "192.0.2.42";
constexpr const char *testIPv4MappedAddress = "::ffff:192.0.2.42";
constexpr const char *testIPv4Network = "192.0.2.42/24";
constexpr const char *testIPv4MappedNetwork = "::ffff:192.0.2.42/120";

TEST(Address, Default)
{
    Server::Address address;
    EXPECT_EQ("::", (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv6)
{
    Server::Address address(testIPv6Address);
    EXPECT_EQ(testIPv6Address, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv6WithPrefix)
{
    Server::Address address(testIPv6Network, true, false);
    EXPECT_EQ(testIPv6Network, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv4)
{
    Server::Address address(testIPv4Address);
    EXPECT_EQ(testIPv4MappedAddress, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv4WithPrefix)
{
    Server::Address address(testIPv4Network, true, false);
    EXPECT_EQ(testIPv4MappedNetwork, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv6OptionalWithoutPrefix)
{
    Server::Address address(testIPv6Address, true);
    EXPECT_EQ(testIPv6Address, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv6OptionalWithPrefix)
{
    Server::Address address(testIPv6Network, true);
    EXPECT_EQ(testIPv6Network, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv4OptionalWithoutPrefix)
{
    Server::Address address(testIPv4Address, true);
    EXPECT_EQ(testIPv4MappedAddress, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, IPv4OptionalWithPrefix)
{
    Server::Address address(testIPv4Network, true);
    EXPECT_EQ(testIPv4MappedNetwork, (std::string)address);
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, Bad)
{
    EXPECT_THROW(Server::Address("meow"), std::exception);
}

TEST(Address, IPv6NeedsPrefix)
{
    EXPECT_THROW((Server::Address(testIPv6Address, true, false)), std::exception);
}

TEST(Address, IPv6WithExtraPrefix)
{
    EXPECT_THROW((Server::Address(testIPv6Network)), std::exception);
}

TEST(Address, IPv6Loopback)
{
    EXPECT_TRUE((Server::Address("::1").isLoopback()));
}

TEST(Address, IPv4Loopback)
{
    EXPECT_TRUE((Server::Address("127.0.0.1").isLoopback()));
}

TEST(Address, IPv4OtherLoopback)
{
    EXPECT_TRUE((Server::Address("127.3.1.4").isLoopback()));
}

TEST(Address, IPv4LoopbackNetwork)
{
    EXPECT_TRUE((Server::Address("127.0.0.0/8", true, false).isLoopback()));
}

TEST(Address, IPv4LoopbackSubnet)
{
    EXPECT_TRUE((Server::Address("127.3.1.4/16", true, false).isLoopback()));
}

TEST(Address, ContainsAddress)
{
    Server::Address network("2001:db8:c0de::/64", true, false);
    Server::Address address("2001:db8:c0de::c0ff:ee");
    EXPECT_TRUE(network.contains(address));
    EXPECT_FALSE(network.isLoopback());
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, NotContainsAddress)
{
    Server::Address network("2001:db8:b4::/64", true, false);
    Server::Address address("2001:db8:c0de::c0ff:ee");
    EXPECT_FALSE(network.contains(address));
    EXPECT_FALSE(network.isLoopback());
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, ContainsAddressSameNetwork)
{
    Server::Address network("2001:db8:c0de::c0de/64", true, false);
    Server::Address address("2001:db8:c0de::c0ff:ee");
    EXPECT_TRUE(network.contains(address));
    EXPECT_FALSE(network.isLoopback());
    EXPECT_FALSE(address.isLoopback());
}

TEST(Address, ContainsSubnet)
{
    Server::Address network("2001:db8::/32", true, false);
    Server::Address subnet("2001:db8::/64", true, false);
    EXPECT_TRUE(network.contains(subnet));
    EXPECT_FALSE(subnet.contains(network));
    EXPECT_FALSE(network.isLoopback());
    EXPECT_FALSE(subnet.isLoopback());
}

} // namespace
