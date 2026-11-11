#include "multipart.h"

#include <stdio.h>
#include <string.h>

#include "text/http_token.h"
namespace bmx {
namespace network {
namespace http {
namespace {
bool Text(char* out, size_t cap, const char* in, bool required) {
    if (!in) {
        out[0] = 0;
        return !required;
    }
    size_t n = 0;
    while (n < cap && in[n]) {
        unsigned char c = in[n];
        if (c < 32 || c > 126 || c == '"' || c == '\\') return false;
        out[n] = in[n];
        ++n;
    }
    if (n == cap || (required && !n)) return false;
    out[n] = 0;
    return true;
}
io::IoResult Error() { return {io::IoStatus::Error, 0, 0}; }
}
MultipartSource::MultipartSource()
    : count_(0),
      index_(0),
      buffered_(0),
      offset_(0),
      length_(0),
      remaining_(0),
      stage_(0),
      valid_(false) {
    content_type_[0] = 0;
}
bool MultipartSource::Header(size_t index) {
    const Field& f = fields_[index];
    int n = snprintf(buffer_, sizeof(buffer_),
                     "--%s\r\nContent-Disposition: form-data; name=\"%s\"%s%s%s\r\n%s%s%s\r\n",
                     boundary_, f.name, f.filename[0] ? "; filename=\"" : "", f.filename,
                     f.filename[0] ? "\"" : "", f.type[0] ? "Content-Type: " : "", f.type,
                     f.type[0] ? "\r\n" : "");
    if (n < 0 || static_cast<size_t>(n) >= sizeof(buffer_)) return false;
    buffered_ = static_cast<size_t>(n);
    offset_ = 0;
    return true;
}
bool MultipartSource::Initialize(const char* boundary, const MultipartField* fields, size_t count) {
    valid_ = false;
    length_ = 0;
    if (count > 16 || (count && !fields) || !Text(boundary_, sizeof(boundary_), boundary, true))
        return false;
    for (const char* p = boundary_; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
              *p == '-' || *p == '_'))
            return false;
    count_ = count;
    for (size_t i = 0; i < count; ++i) {
        Field& f = fields_[i];
        const MultipartField& in = fields[i];
        if (!in.body || in.body == this || !Text(f.name, sizeof(f.name), in.name, true) ||
            !Text(f.filename, sizeof(f.filename), in.filename, false) ||
            !Text(f.type, sizeof(f.type), in.content_type, false))
            return false;
        for (size_t n = 0; n < i; ++n)
            if (fields_[n].body == in.body) return false;
        f.body = in.body;
        f.size = in.body->Length();
        if (!Header(i) || length_ > UINT64_MAX - buffered_ - 2 ||
            f.size > UINT64_MAX - length_ - buffered_ - 2)
            return false;
        length_ += buffered_ + f.size + 2;
    }
    size_t final = strlen(boundary_) + 6;
    if (length_ > UINT64_MAX - final) return false;
    length_ += final;
    snprintf(content_type_, sizeof(content_type_), "multipart/form-data; boundary=%s", boundary_);
    index_ = buffered_ = offset_ = 0;
    stage_ = 0;
    remaining_ = 0;
    valid_ = true;
    return true;
}
bool MultipartSource::Reset() {
    if (!valid_) return false;
    for (size_t i = 0; i < count_; ++i)
        if (fields_[i].body->Length() != fields_[i].size || !fields_[i].body->Reset()) return false;
    index_ = buffered_ = offset_ = 0;
    stage_ = 0;
    remaining_ = 0;
    return true;
}
io::IoResult MultipartSource::ReadSome(uint8_t* data, size_t capacity) {
    if (!valid_ || !data || !capacity) return Error();
    for (unsigned steps = 0; steps < 4; ++steps) {
        if (offset_ < buffered_) {
            size_t n = buffered_ - offset_;
            if (n > capacity) n = capacity;
            memcpy(data, buffer_ + offset_, n);
            offset_ += n;
            return {io::IoStatus::Progress, n, 0};
        }
        if (stage_ == 0) {
            if (index_ == count_) {
                int n = snprintf(buffer_, sizeof(buffer_), "--%s--\r\n", boundary_);
                buffered_ = static_cast<size_t>(n);
                offset_ = 0;
                stage_ = 3;
            } else {
                if (!Header(index_)) return Error();
                remaining_ = fields_[index_].size;
                stage_ = 1;
            }
        } else if (stage_ == 1) {
            if (remaining_) {
                size_t n = remaining_ < capacity ? static_cast<size_t>(remaining_) : capacity;
                io::IoResult result = fields_[index_].body->ReadSome(data, n);
                if (result.status == io::IoStatus::Progress) {
                    if (!result.bytes || result.bytes > n) return Error();
                    remaining_ -= result.bytes;
                } else if (result.status != io::IoStatus::PendingRead || result.bytes)
                    return Error();
                return result;
            }
            memcpy(buffer_, "\r\n", 2);
            buffered_ = 2;
            offset_ = 0;
            stage_ = 2;
        } else if (stage_ == 2) {
            ++index_;
            stage_ = 0;
        } else
            return {io::IoStatus::EndOfStream, 0, 0};
    }
    return {io::IoStatus::PendingRead, 0, 0};
}
}
}
}
