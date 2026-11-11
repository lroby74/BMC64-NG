#include "wic64/policy.h"
#include "network/http/url.h"
#include "text/hex.h"
#include <stdio.h>
#include <string.h>
namespace bmx { namespace wic64 {
namespace {
bool Add(char *out, size_t *used, const char *data, size_t count) {
    if (count > MaximumUrl - *used) return false;
    memcpy(out + *used, data, count);
    *used += count; out[*used] = 0;
    return true;
}
bool Token(const char *text, size_t capacity) {
    const char *end = static_cast<const char *>(memchr(text, 0, capacity));
    if (!end) return false;

    for (const char *p = text; p != end; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.')) return false;
    return true;
}
}
bool ValidIdentity(const Identity &id) {
    if (!memchr(id.mac, 0, sizeof id.mac) || strlen(id.mac) != 17 ||
        !Token(id.token, sizeof id.token) || !Token(id.token_name, sizeof id.token_name)) return false;
    for (unsigned i = 0; i < 17; ++i) {
        if (i % 3 == 2) { if (id.mac[i] != ':') return false; }
        else if (text::HexDigit(id.mac[i], text::HexCase::Mixed) < 0) return false;
    }
    return true;
}
bool DeriveMac(const char *serial, char out[18]) {
    if (!serial || !out || strlen(serial) != 16 ||
        !text::IsHex(serial, 16, text::HexCase::Mixed)) return false;
    uint32_t hash = 2166136261U;
    for (unsigned i = 0; i < 16; ++i) {
        hash ^= static_cast<uint32_t>(text::HexDigit(serial[i], text::HexCase::Mixed));
        hash *= 16777619U;
    }

    snprintf(out, 18, "02:64:%02x:%02x:%02x:%02x", (hash >> 24) & 255,
             (hash >> 16) & 255, (hash >> 8) & 255, hash & 255);
    return true;
}
UrlStatus ExpandUrl(const uint8_t *data, size_t size, bool encoded, bool legacy,
                    const char *server, const Identity &identity,
                    char out[MaximumUrl + 1]) {
    if (!data || !size || !server || !out || !ValidIdentity(identity)) return UrlStatus::Invalid;
    char decoded[MaximumUrl + 1];
    size_t used = 0;
    decoded[0] = 0;
    for (size_t i = 0; i < size;) {
        if (encoded && i + 1 < size && data[i] == '<' && data[i + 1] == '$') {
            if (size - i < 4) return UrlStatus::Invalid;
            const size_t n = data[i + 2] | (static_cast<size_t>(data[i + 3]) << 8);
            i += 4;
            if (n > size - i) return UrlStatus::Invalid;
            if (n > (MaximumUrl - used) / 2) return UrlStatus::TooLarge;
            if (!text::EncodeHex(data + i, n, decoded + used, MaximumUrl + 1 - used))
                return UrlStatus::Invalid;
            i += n; used += n * 2;
        } else {
            if (!Add(decoded, &used, reinterpret_cast<const char *>(data + i), 1))
                return UrlStatus::TooLarge;
            ++i;
        }
    }

    if (legacy) {
        for (size_t i = 0; i + 6 < used; ++i) {
            if (!memcmp(decoded + i, "&pid=", 5)) {
                for (size_t n = 0; n < 2; ++n) {
                    const unsigned char c = decoded[i + 5 + n];
                    if (c < 32 || c > 126) decoded[i + 5 + n] = n ? 'Y' : 'X';
                }
            }
        }
    }
    while (used && (decoded[used - 1] == 0 || decoded[used - 1] == ' ' ||
                    decoded[used - 1] == '\r' || decoded[used - 1] == '\n')) --used;
    if (!used) return UrlStatus::Invalid;
    for (size_t i = 0; i < used; ++i)
        if (static_cast<unsigned char>(decoded[i]) < 33 ||
            static_cast<unsigned char>(decoded[i]) > 126) return UrlStatus::Invalid;
    char mac[13];
    for (unsigned i = 0, n = 0; i < 17; ++i) if (identity.mac[i] != ':') mac[n++] = identity.mac[i];
    mac[12] = 0;
    size_t length = 0;
    out[0] = 0;
    for (size_t i = 0; i < used;) {
        if ((i == 0 && decoded[i] == '!') ||
            (used - i >= 4 && !memcmp(decoded + i, "%ser", 4))) {
            if (!Add(out, &length, server, strlen(server))) return UrlStatus::TooLarge;
            i += decoded[i] == '!' ? 1 : 4;
        } else if (used - i >= 4 && !memcmp(decoded + i, "%mac", 4)) {
            if (!Add(out, &length, mac, 12) ||
                !Add(out, &length, identity.token, strlen(identity.token))) return UrlStatus::TooLarge;
            i += 4;
        } else {
            if (!Add(out, &length, decoded + i, 1)) return UrlStatus::TooLarge;
            ++i;
        }
    }
    network::http::Url parsed;
    if (!network::http::ParseUrl(out, &parsed)) return UrlStatus::Invalid;
    return UrlStatus::Ok;
}
} }
