
/*
   Copyright (C) 2026 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _DCERPC_DTYP_H_
#define _DCERPC_DTYP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <dcerpc/dcerpc.h>











extern unsigned char NT_SID_AUTHORITY[6];

#define MAXSUBAUTH 10
typedef struct RPC_SID {
        uint8_t Revision;
        uint8_t SubAuthorityCount;
        uint8_t IdentifierAuthority[6];
        uint32_t SubAuthority[MAXSUBAUTH];
} RPC_SID, *PRPC_SID;






typedef uint32_t ACCESS_MASK;


#define DELETE                          0x00010000
#define READ_CONTROL                    0x00020000
#define WRITE_DAC                       0x00040000
#define WRITE_DACL                      WRITE_DAC
#define WRITE_OWNER                     0x00080000
#define SYNCHRONIZE                     0x00100000
#define ACCESS_SYSTEM_SECURITY          0x01000000
#define MAXIMUM_ALLOWED                 0x02000000
#define GENERIC_ALL                     0x10000000
#define GENERIC_EXECUTE                 0x20000000
#define GENERIC_WRITE                   0x40000000
#define GENERIC_READ                    0x80000000












#define ACCESS_ALLOWED_ACE_TYPE                 0x00
#define ACCESS_DENIED_ACE_TYPE                  0x01
#define SYSTEM_AUDIT_ACE_TYPE                   0x02
#define SYSTEM_ALARM_ACE_TYPE                   0x03
#define ACCESS_ALLOWED_COMPOUND_ACE_TYPE        0x04
#define ACCESS_ALLOWED_OBJECT_ACE_TYPE          0x05
#define ACCESS_DENIED_OBJECT_ACE_TYPE           0x06
#define SYSTEM_AUDIT_OBJECT_ACE_TYPE            0x07
#define SYSTEM_ALARM_OBJECT_ACE_TYPE            0x08
#define ACCESS_ALLOWED_CALLBACK_ACE_TYPE        0x09
#define ACCESS_DENIED_CALLBACK_ACE_TYPE         0x0A
#define ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE 0x0B
#define ACCESS_DENIED_CALLBACK_OBJECT_ACE_TYPE  0x0C
#define SYSTEM_AUDIT_CALLBACK_ACE_TYPE          0x0D
#define SYSTEM_ALARM_CALLBACK_ACE_TYPE          0x0E
#define SYSTEM_AUDIT_CALLBACK_OBJECT_ACE_TYPE   0x0F
#define SYSTEM_ALARM_CALLBACK_OBJECT_ACE_TYPE   0x10
#define SYSTEM_MANDATORY_LABEL_ACE_TYPE         0x11
#define SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE      0x12
#define SYSTEM_SCOPED_POLICY_ID_ACE_TYPE        0x13


#define OBJECT_INHERIT_ACE              0x01
#define CONTAINER_INHERIT_ACE           0x02
#define NO_PROPAGATE_INHERIT_ACE        0x04
#define INHERIT_ONLY_ACE                0x08
#define INHERITED_ACE                   0x10
#define SUCCESSFUL_ACCESS_ACE_FLAG      0x40
#define FAILED_ACCESS_ACE_FLAG          0x80

typedef struct _ACE_HEADER {
        uint8_t AceType;
        uint8_t AceFlags;




        uint16_t AceSize;
} ACE_HEADER, *PACE_HEADER;

int dcerpc_ACE_HEADER_coder(char *name, struct dcerpc_context *dce,
                            struct dcerpc_pdu *pdu,
                            struct dcerpc_iovec *iov, int *offset,
                            void *ptr);




















typedef struct _ACCESS_ALLOWED_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} ACCESS_ALLOWED_ACE, *PACCESS_ALLOWED_ACE;

typedef struct _ACCESS_DENIED_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} ACCESS_DENIED_ACE, *PACCESS_DENIED_ACE;

typedef struct _SYSTEM_AUDIT_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} SYSTEM_AUDIT_ACE, *PSYSTEM_AUDIT_ACE;

typedef struct _SYSTEM_ALARM_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} SYSTEM_ALARM_ACE, *PSYSTEM_ALARM_ACE;

typedef struct _SYSTEM_MANDATORY_LABEL_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} SYSTEM_MANDATORY_LABEL_ACE, *PSYSTEM_MANDATORY_LABEL_ACE;

typedef struct _SYSTEM_SCOPED_POLICY_ID_ACE {
        ACE_HEADER Header;
        ACCESS_MASK Mask;
        RPC_SID Sid;
} SYSTEM_SCOPED_POLICY_ID_ACE, *PSYSTEM_SCOPED_POLICY_ID_ACE;

int dcerpc_ACCESS_ALLOWED_ACE_coder(char *name, struct dcerpc_context *dce,
                                    struct dcerpc_pdu *pdu,
                                    struct dcerpc_iovec *iov, int *offset,
                                    void *ptr);
int dcerpc_ACCESS_DENIED_ACE_coder(char *name, struct dcerpc_context *dce,
                                   struct dcerpc_pdu *pdu,
                                   struct dcerpc_iovec *iov, int *offset,
                                   void *ptr);
int dcerpc_SYSTEM_AUDIT_ACE_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr);
int dcerpc_SYSTEM_ALARM_ACE_coder(char *name, struct dcerpc_context *dce,
                                  struct dcerpc_pdu *pdu,
                                  struct dcerpc_iovec *iov, int *offset,
                                  void *ptr);
int dcerpc_SYSTEM_MANDATORY_LABEL_ACE_coder(char *name,
                                            struct dcerpc_context *dce,
                                            struct dcerpc_pdu *pdu,
                                            struct dcerpc_iovec *iov,
                                            int *offset, void *ptr);
int dcerpc_SYSTEM_SCOPED_POLICY_ID_ACE_coder(char *name,
                                             struct dcerpc_context *dce,
                                             struct dcerpc_pdu *pdu,
                                             struct dcerpc_iovec *iov,
                                             int *offset, void *ptr);
















#define ACL_REVISION    0x02
#define ACL_REVISION_DS 0x04

typedef struct _ACL {
        uint8_t AclRevision;

        uint8_t Sbz1;

        uint16_t AclSize;
        uint16_t AceCount;

        uint16_t Sbz2;




        ACCESS_ALLOWED_ACE *Aces;
} ACL, *PACL;

int dcerpc_ACL_coder(char *name, struct dcerpc_context *dce,
                     struct dcerpc_pdu *pdu,
                     struct dcerpc_iovec *iov, int *offset,
                     void *ptr);













#define SE_OWNER_DEFAULTED              0x0001
#define SE_GROUP_DEFAULTED              0x0002
#define SE_DACL_PRESENT                 0x0004
#define SE_DACL_DEFAULTED               0x0008
#define SE_SACL_PRESENT                 0x0010
#define SE_SACL_DEFAULTED               0x0020
#define SE_DACL_AUTO_INHERIT_REQ        0x0100
#define SE_SACL_AUTO_INHERIT_REQ        0x0200
#define SE_DACL_AUTO_INHERITED          0x0400
#define SE_SACL_AUTO_INHERITED          0x0800
#define SE_DACL_PROTECTED               0x1000
#define SE_SACL_PROTECTED               0x2000
#define SE_RM_CONTROL_VALID             0x4000
#define SE_SELF_RELATIVE                0x8000

typedef struct _SECURITY_DESCRIPTOR {
        uint8_t Revision;

        uint8_t Sbz1;
        uint16_t Control;
        RPC_SID *Owner;
        RPC_SID *Group;
        ACL *Sacl;
        ACL *Dacl;
} SECURITY_DESCRIPTOR, *PSECURITY_DESCRIPTOR;

int dcerpc_SECURITY_DESCRIPTOR_coder(char *name, struct dcerpc_context *dce,
                                     struct dcerpc_pdu *pdu,
                                     struct dcerpc_iovec *iov, int *offset,
                                     void *ptr);

#ifdef __cplusplus
}
#endif

#endif
