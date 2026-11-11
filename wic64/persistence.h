#ifndef BMX_WIC64_PERSISTENCE_H
#define BMX_WIC64_PERSISTENCE_H
#include "wic64/policy.h"
namespace bmx { namespace wic64 {
struct IdentityPaths { const char *file, *temporary, *backup; };
extern const IdentityPaths DefaultIdentityPaths;


bool LoadIdentity(Identity &identity, const IdentityPaths &paths = DefaultIdentityPaths);
bool PersistIdentity(const Identity &identity, const IdentityPaths &paths = DefaultIdentityPaths);
} }
#endif
