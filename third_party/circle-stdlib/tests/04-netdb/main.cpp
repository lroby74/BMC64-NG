#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <cstdint>

#include "testkernel.h"

int main(void)
{
    return CTestKernel::RunTests("04-netdb");
}

TEST_CASE("Basic getaddrinfo with nullptr node (loopback)")
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    int ret = getaddrinfo(nullptr, "80", &hints, &res);

    REQUIRE(ret == 0);
    REQUIRE(res != nullptr);
    REQUIRE(res->ai_family == AF_INET);
    REQUIRE(res->ai_socktype == SOCK_STREAM);

    struct sockaddr_in * const sa = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    REQUIRE(sa != nullptr);
    REQUIRE(sa->sin_port == htons(80));
    REQUIRE(sa->sin_addr.s_addr == htonl(INADDR_LOOPBACK));

    freeaddrinfo(res);
}

TEST_CASE("Basic getaddrinfo with AI_PASSIVE (INADDR_ANY)")
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = nullptr;
    int ret = getaddrinfo(nullptr, "8080", &hints, &res);

    REQUIRE(ret == 0);
    REQUIRE(res != nullptr);
    REQUIRE(res->ai_family == AF_INET);
    REQUIRE(res->ai_socktype == SOCK_STREAM);

    struct sockaddr_in * const sa = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    REQUIRE(sa != nullptr);
    REQUIRE(sa->sin_port == htons(8080));
    REQUIRE(sa->sin_addr.s_addr == htonl(INADDR_ANY));

    freeaddrinfo(res);
}

TEST_CASE("gai_strerror returns valid strings")
{
    REQUIRE(gai_strerror(EAI_NONAME) != nullptr);
    REQUIRE(strlen(gai_strerror(EAI_NONAME)) > 0);
    
    REQUIRE(gai_strerror(EAI_MEMORY) != nullptr);
    REQUIRE(strlen(gai_strerror(EAI_MEMORY)) > 0);
}

TEST_CASE("getaddrinfo resolves a concrete hostname (DNS)")
{
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = nullptr;
    // Resolve example.com
    int const ret = getaddrinfo("example.com", "80", &hints, &res);

    REQUIRE(ret == 0);
    REQUIRE(res != nullptr);
    REQUIRE(res->ai_family == AF_INET);
    REQUIRE(res->ai_socktype == SOCK_STREAM);

    struct sockaddr_in * const sa = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
    REQUIRE(sa != nullptr);
    REQUIRE(sa->sin_port == htons(80));
    // We don't check a specific IP, just that it's not 0 or INADDR_ANY
    REQUIRE(sa->sin_addr.s_addr != 0);
    REQUIRE(sa->sin_addr.s_addr != htonl(INADDR_ANY));

    // Log the resolved IP address for information
    uint32_t const addr = ntohl(sa->sin_addr.s_addr);
    std::string const ip_str = std::to_string((addr >> 24) & 0xFF) + "." +
                               std::to_string((addr >> 16) & 0xFF) + "." +
                               std::to_string((addr >> 8) & 0xFF) + "." +
                               std::to_string(addr & 0xFF);
    MESSAGE("Resolved example.com to IP: " << ip_str);

    freeaddrinfo(res);
}
