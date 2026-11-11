#include "wic64/persistence.h"
#include <stdio.h>
#include <string.h>









namespace bmx { namespace wic64 {
const IdentityPaths DefaultIdentityPaths = {
    "/wic64.cfg", "/wic64.new", "/wic64.bak"
};
namespace {
struct Record { char magic[8]; Identity identity; };
static_assert(sizeof(Record) == 123, "WiC64 identity format is versioned and byte-exact");
bool Read(const char *path, Identity &identity) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    Record record{};
    const bool valid = fread(&record, 1, sizeof record, file) == sizeof record &&
                       fgetc(file) == EOF && !memcmp(record.magic, "BMXWIC1\n", 8) &&
                       ValidIdentity(record.identity);
    fclose(file);
    if (valid) identity = record.identity;
    return valid;
}
bool Exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}
}
bool LoadIdentity(Identity &identity, const IdentityPaths &paths) {
    return Read(paths.file, identity) || Read(paths.backup, identity);
}
bool PersistIdentity(const Identity &identity, const IdentityPaths &paths) {
    if (!ValidIdentity(identity)) return false;
    Record record{};
    memcpy(record.magic, "BMXWIC1\n", 8);

    snprintf(record.identity.mac, sizeof record.identity.mac, "%s", identity.mac);
    snprintf(record.identity.token, sizeof record.identity.token, "%s", identity.token);
    snprintf(record.identity.token_name, sizeof record.identity.token_name, "%s", identity.token_name);
    remove(paths.temporary);
    FILE *file = fopen(paths.temporary, "wb");
    if (!file) return false;
    bool written = fwrite(&record, 1, sizeof record, file) == sizeof record &&
                   fflush(file) == 0;
    if (fclose(file) != 0) written = false;
    if (!written) { remove(paths.temporary); return false; }

    if (Exists(paths.file)) {
        remove(paths.backup);
        if (rename(paths.file, paths.backup) != 0) { remove(paths.temporary); return false; }
    }
    if (rename(paths.temporary, paths.file) != 0) {
        rename(paths.backup, paths.file);
        return false;
    }
    remove(paths.backup);
    return true;
}
} }
