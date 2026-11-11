#ifndef BMX_NETWORK_HTTP_MULTIPART_H
#define BMX_NETWORK_HTTP_MULTIPART_H
#include "body_source.h"
namespace bmx {
namespace network {
namespace http {
struct MultipartField {
    const char* name;
    const char* filename;
    const char* content_type;
    BodySource* body;
};


class MultipartSource : public BodySource {
   public:
    MultipartSource();
    bool Initialize(const char* boundary, const MultipartField* fields, size_t count);
    uint64_t Length() const override { return length_; }
    io::IoResult ReadSome(uint8_t* data, size_t capacity) override;
    bool Reset() override;
    const char* ContentType() const { return content_type_; }

   private:
    struct Field {
        char name[65], filename[129], type[97];
        BodySource* body;
        uint64_t size;
    };
    bool Header(size_t index);
    Field fields_[16];
    char boundary_[71], content_type_[112], buffer_[512];
    size_t count_, index_, buffered_, offset_;
    uint64_t length_, remaining_;
    unsigned stage_;
    bool valid_;
};
}
}
}
#endif
