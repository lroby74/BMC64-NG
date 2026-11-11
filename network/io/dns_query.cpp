#include "dns_query.h"
#include <string.h>

namespace bmx { namespace network { namespace io {
namespace {
unsigned Read16(const uint8_t *p) { return (unsigned(p[0]) << 8) | p[1]; }
char Lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
bool Name(const uint8_t *p, size_t size, size_t *offset, char out[254]) {
    size_t pos = *offset, length = 0;
    bool jumped = false;

    for (unsigned step = 0; step < 128; ++step) {
        if (pos >= size) return false;
        unsigned n = p[pos++];
        if ((n & 0xc0) == 0xc0) {
            if (pos >= size) return false;
            size_t target = ((n & 0x3f) << 8) | p[pos++];
            if (!jumped) *offset = pos;
            if (target >= pos - 2) return false;
            jumped = true;
            pos = target;
            continue;
        }
        if (n > 63 || pos + n > size) return false;
        if (!n) {
            if (!jumped) *offset = pos;
            out[length] = 0;
            return true;
        }
        if (length) {
            if (length >= 253) return false;
            out[length++] = '.';
        }
        if (length + n > 253) return false;
        for (unsigned i = 0; i < n; ++i) {
            if (!p[pos] || p[pos] == '.') return false;
            out[length++] = Lower(static_cast<char>(p[pos++]));
        }
    }
    return false;
}
}

bool ParseIPv4(const char *host, uint8_t address[4]) {
    if (!host || !address) return false;
    uint8_t parsed[4];
    for (unsigned i = 0; i < 4; ++i) {
        unsigned value = 0, digits = 0;
        while (*host >= '0' && *host <= '9') {
            if (++digits > 3) return false;
            value = value * 10 + (*host++ - '0');
        }
        if (!digits || value > 255 || (i < 3 ? *host != '.' : *host != 0))
            return false;
        parsed[i] = value;
        if (i < 3) ++host;
    }
    memcpy(address, parsed, 4);
    return true;
}

bool DnsQuery::Initialize(const char *host, uint16_t transaction) {
    size_ = 0;
    if (!host) return false;
    size_t n = strlen(host);
    if (n && host[n - 1] == '.') --n;
    if (!n || n > 253) return false;
    for (size_t i = 0; i < n; ++i) host_[i] = Lower(host[i]);
    host_[n] = 0;
    memset(packet_, 0, sizeof(packet_));
    packet_[0] = transaction >> 8;
    packet_[1] = transaction;
    packet_[2] = 1;
    packet_[5] = 1;
    size_t pos = 12, start = 0;
    while (start < n) {
        size_t end = start;
        while (end < n && host_[end] != '.') {
            unsigned char c = host_[end];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
                return false;
            ++end;
        }
        size_t length = end - start;
        if (!length || length > 63 || (end < n && end + 1 == n)) return false;
        packet_[pos++] = length;
        memcpy(packet_ + pos, host_ + start, length);
        pos += length;
        start = end + 1;
    }
    packet_[pos++] = 0;
    packet_[pos++] = 0; packet_[pos++] = 1;
    packet_[pos++] = 0; packet_[pos++] = 1;
    size_ = pos;
    return true;
}

DnsQuery::Status DnsQuery::Parse(const uint8_t *p, size_t size, uint8_t address[4]) const {
    if (!size_ || !p || !address || size < 12 || Read16(p) != Read16(packet_)) return Status::Ignore;
    unsigned flags = Read16(p + 2);
    if ((flags & 0xf800) != 0x8000 || Read16(p + 4) != 1) return Status::Invalid;
    size_t pos = 12;
    char name[254];
    if (!Name(p, size, &pos, name) || pos + 4 > size) return Status::Invalid;
    if (strcmp(name, host_) || Read16(p + pos) != 1 || Read16(p + pos + 2) != 1)
        return Status::Ignore;
    if (flags & 0x0200) return Status::Truncated;
    if (flags & 15) return Status::NotFound;
    pos += 4;
    const size_t answers_start = pos;
    const unsigned answers = Read16(p + 6);
    if (answers > 32) return Status::Invalid;
    char wanted[254];
    strcpy(wanted, host_);
    for (unsigned link = 0; link < 8; ++link) {
        bool alias = false;
        pos = answers_start;
        for (unsigned i = 0; i < answers; ++i) {
            if (!Name(p, size, &pos, name) || pos + 10 > size) return Status::Invalid;
            unsigned type = Read16(p + pos), klass = Read16(p + pos + 2);
            unsigned length = Read16(p + pos + 8);
            pos += 10;
            if (length > size - pos) return Status::Invalid;
            if (klass == 1 && strcmp(name, wanted) == 0) {
                if (type == 1) {
                    if (length != 4) return Status::Invalid;
                    memcpy(address, p + pos, 4);
                    return Status::Complete;
                }
                if (type == 5) {
                    size_t end = pos;
                    if (!Name(p, size, &end, wanted) || end != pos + length) return Status::Invalid;
                    alias = true;
                    break;
                }
            }
            pos += length;
        }
        if (!alias) return Status::NotFound;
    }
    return Status::Invalid;
}
}}}
