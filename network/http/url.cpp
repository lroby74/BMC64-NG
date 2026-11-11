#include "url.h"

#include <string.h>
namespace bmx {
namespace network {
namespace http {
namespace {
bool StartsNoCase(const char* s, const char* prefix) {
    for (; *prefix; ++prefix, ++s) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != *prefix) return false;
    }
    return true;
}
bool Copy(char* out, size_t capacity, const char* in, size_t size) {
    if (size >= capacity) return false;
    memcpy(out, in, size);
    out[size] = 0;
    return true;
}
bool Hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
bool Syntax(const char* input, size_t* size) {
    if (!input) return false;
    size_t n = 0;
    for (; n < kUrlBytes && input[n]; ++n) {
        unsigned char c = input[n];
        if (c <= 32 || c >= 127 || c == '\\' || c == '"' || c == '<' || c == '>') return false;
        if (c == '%' && (n + 2 >= kUrlBytes || !input[n + 1] || !input[n + 2] ||
                         !Hex(input[n + 1]) || !Hex(input[n + 2])))
            return false;
    }
    *size = n;
    return n < kUrlBytes;
}

bool Normalize(char* target) {
    char output[kUrlBytes];
    size_t used = 0;
    const char* p = target;
    const char* query = strchr(p, '?');
    const char* end = query ? query : p + strlen(p);
    while (p < end) {
        size_t left = static_cast<size_t>(end - p);
        if (left >= 3 && !memcmp(p, "../", 3)) {
            p += 3;
            continue;
        }
        if (left >= 2 && !memcmp(p, "./", 2)) {
            p += 2;
            continue;
        }
        if (left >= 3 && !memcmp(p, "/./", 3)) {
            p += 2;
            continue;
        }
        if (left == 2 && !memcmp(p, "/.", 2)) {
            output[used++] = '/';
            p = end;
            continue;
        }
        if ((left >= 4 && !memcmp(p, "/../", 4)) || (left == 3 && !memcmp(p, "/..", 3))) {
            if (used) {
                --used;
                while (used && output[used] != '/') --used;
            }
            p += 3;
            if (p == end) output[used++] = '/';
            continue;
        }
        if ((left == 1 && *p == '.') || (left == 2 && !memcmp(p, "..", 2))) {
            p = end;
            continue;
        }
        output[used++] = *p++;
        while (p < end && *p != '/') output[used++] = *p++;
    }
    if (!used) output[used++] = '/';
    if (query) {
        size_t n = strlen(query);
        if (used + n >= sizeof(output)) return false;
        memcpy(output + used, query, n);
        used += n;
    }
    output[used] = 0;
    memcpy(target, output, used + 1);
    return true;
}
}
bool ParseUrl(const char* input, Url* out) {
    size_t size;
    if (!out || !Syntax(input, &size)) return false;
    const char* p;
    if (StartsNoCase(input, "https://")) {
        out->secure = true;
        p = input + 8;
    } else if (StartsNoCase(input, "http://")) {
        out->secure = false;
        p = input + 7;
    } else
        return false;
    out->port = out->secure ? 443 : 80;
    const char* host = p;
    while (*p && *p != ':' && *p != '/' && *p != '?' && *p != '#') {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '.' || c == '_'))
            return false;
        ++p;
    }
    if (p == host || !Copy(out->host, sizeof(out->host), host, p - host)) return false;
    for (char* h = out->host; *h; ++h)
        if (*h >= 'A' && *h <= 'Z') *h += 'a' - 'A';
    if (*p == ':') {
        unsigned port = 0;
        const char* start = ++p;
        for (; *p >= '0' && *p <= '9'; ++p) {
            port = port * 10 + *p - '0';
            if (port > 65535) return false;
        }
        if (p == start || !port) return false;
        out->port = static_cast<uint16_t>(port);
    }
    if (*p && *p != '/' && *p != '?' && *p != '#') return false;
    const char* fragment = strchr(p, '#');
    size_t n = fragment ? fragment - p : strlen(p);
    size_t prefix = *p == '/' ? 0 : 1;
    if (n + prefix >= sizeof(out->target)) return false;
    if (prefix) out->target[0] = '/';
    memcpy(out->target + prefix, p, n);
    out->target[n + prefix] = 0;
    return Normalize(out->target);
}
bool ResolveUrl(const Url& base, const char* reference, Url* out) {
    size_t n;
    if (!out || !Syntax(reference, &n)) return false;
    if (StartsNoCase(reference, "http://") || StartsNoCase(reference, "https://"))
        return ParseUrl(reference, out);
    if (!strncmp(reference, "//", 2)) {
        char absolute[kUrlBytes];
        const char* scheme = base.secure ? "https:" : "http:";
        size_t s = strlen(scheme);
        if (s + n >= sizeof(absolute)) return false;
        memcpy(absolute, scheme, s);
        memcpy(absolute + s, reference, n + 1);
        return ParseUrl(absolute, out);
    }
    const char* colon = strchr(reference, ':');
    const char* slash = strchr(reference, '/');
    const char* question = strchr(reference, '?');
    const char* fragment = strchr(reference, '#');
    if (colon && (!slash || colon < slash) && (!question || colon < question) &&
        (!fragment || colon < fragment))
        return false;
    *out = base;
    if (fragment) n = fragment - reference;
    if (!n) return true;
    if (*reference == '/') {
        if (!Copy(out->target, sizeof(out->target), reference, n)) return false;
    } else {
        size_t prefix = strcspn(base.target, "?");
        if (*reference != '?')
            while (prefix && base.target[prefix - 1] != '/') --prefix;
        if (prefix + n >= sizeof(out->target)) return false;
        memcpy(out->target + prefix, reference, n);
        out->target[prefix + n] = 0;
    }
    return Normalize(out->target);
}
bool SameOrigin(const Url& a, const Url& b) {
    return a.secure == b.secure && a.port == b.port && !strcmp(a.host, b.host);
}
}
}
}
