
/*
   Copyright (C) 2016 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIBSMB2_H_
#define _LIBSMB2_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LIBSMB2_SHARE_ENUM_V2 1
#define LIBSMB2_SRVSVC_V2 1

struct smb2_iovec {
        uint8_t *buf;
        size_t len;
        void (*free)(void *);
};

struct smb2_context;





typedef void (*smb2_command_cb)(struct smb2_context *smb2, int status,
                                void *command_data, void *cb_data);





typedef void (*smb2_error_cb)(struct smb2_context *smb2,
                                const char *error_string);




typedef int (*smb2_accepted_cb)(const int fd, void *cb_data);




typedef void (*smb2_client_connection)(struct smb2_context *smb2, void *cb_data);







struct smb2_oplock_or_lease_break_reply;

typedef void (*smb2_oplock_or_lease_break_cb)(struct smb2_context *smb2,
           int status,
           struct smb2_oplock_or_lease_break_reply *rep,
           uint8_t *new_oplock_level,
           uint32_t *new_lease_state);


#define SMB2_TYPE_FILE      0x00000000
#define SMB2_TYPE_DIRECTORY 0x00000001
#define SMB2_TYPE_LINK      0x00000002




#define SMB2_TYPE_FIFO      0x00000003
#define SMB2_TYPE_CHARDEV   0x00000004
#define SMB2_TYPE_BLOCKDEV  0x00000005
#define SMB2_TYPE_SOCKET    0x00000006
struct smb2_stat_64 {
        uint32_t smb2_type;
        uint32_t smb2_nlink;
        uint64_t smb2_ino;
        uint64_t smb2_size;
        uint64_t smb2_atime;
        uint64_t smb2_atime_nsec;
        uint64_t smb2_mtime;
        uint64_t smb2_mtime_nsec;
        uint64_t smb2_ctime;
        uint64_t smb2_ctime_nsec;
        uint64_t smb2_btime;
        uint64_t smb2_btime_nsec;




        uint32_t smb2_attributes;






        uint32_t smb2_reparse_tag;
};

struct smb2_statvfs {
        uint32_t        f_bsize;
        uint32_t        f_frsize;
        uint64_t        f_blocks;
        uint64_t        f_bfree;
        uint64_t        f_bavail;
        uint32_t        f_files;
        uint32_t        f_ffree;
        uint32_t        f_favail;
        uint32_t        f_fsid;
        uint32_t        f_flag;
        uint32_t        f_namemax;
};

struct smb2dirent {
        const char *name;
        struct smb2_stat_64 st;
};

#if defined(_WINDOWS)
#ifdef __USE_WINSOCK__
#include <winsock.h>
#else
#include <ws2tcpip.h>
#include <winsock2.h>
#endif
#elif defined(_XBOX)
#include <xtl.h>
#include <winsockx.h>
#endif

#if defined(_WINDOWS) || defined(_XBOX)
typedef SOCKET t_socket;
#else
#ifndef T_SOCKET_DEFINED
#define T_SOCKET_DEFINED
typedef int t_socket;
#endif





#if defined(__PS2__) && !defined(_EE)
#include <ps2ip.h>
#elif defined(__has_include)
#if __has_include(<sys/select.h>)
#include <sys/select.h>
#endif
#else
#include <sys/select.h>
#endif
#endif







struct smb2_context *smb2_init_context(void);








void smb2_close_context(struct smb2_context *smb2);










void smb2_destroy_context(struct smb2_context *smb2);




struct smb2_context *smb2_active_contexts(void);





int smb2_context_active(struct smb2_context *smb2);






















t_socket smb2_get_fd(struct smb2_context *smb2);



int smb2_which_events(struct smb2_context *smb2);














const t_socket *
smb2_get_fds(struct smb2_context *smb2, size_t *fd_count, int *timeout);













#define SMB2_ADD_FD 0
#define SMB2_DEL_FD 1
typedef void (*smb2_change_fd_cb)(struct smb2_context *smb2, t_socket fd, int cmd);
typedef void (*smb2_change_events_cb)(struct smb2_context *smb2, t_socket fd,
                                      int events);
void smb2_fd_event_callbacks(struct smb2_context *smb2,
                             smb2_change_fd_cb change_fd,
                             smb2_change_events_cb change_events);











int smb2_service(struct smb2_context *smb2, int revents);
















int smb2_service_fd(struct smb2_context *smb2, t_socket fd, int revents);









void smb2_set_timeout(struct smb2_context *smb2, int seconds);













void smb2_set_passthrough(struct smb2_context *smb2,
                      int passthrough);




void smb2_get_passthrough(struct smb2_context *smb2,
                      int *passthrough);





enum smb2_negotiate_version {
        SMB2_VERSION_ANY  = 0,
        SMB2_VERSION_ANY2 = 2,
        SMB2_VERSION_ANY3 = 3,
        SMB2_VERSION_0202 = 0x0202,
        SMB2_VERSION_0210 = 0x0210,
        SMB2_VERSION_0300 = 0x0300,
        SMB2_VERSION_0302 = 0x0302,
        SMB2_VERSION_0311 = 0x0311,
};

#define SMB2_VERSION_WILDCARD 0x02FF

void smb2_set_version(struct smb2_context *smb2,
                      enum smb2_negotiate_version version);




#define LIBSMB2_MAJOR_VERSION 4
#define LIBSMB2_MINOR_VERSION 0
#define LIBSMB2_PATCH_VERSION 0

struct smb2_libversion
{
     uint8_t major_version;
     uint8_t minor_version;
     uint8_t patch_version;
};






void smb2_get_libsmb2Version(struct smb2_libversion *smb2_ver);




uint16_t smb2_get_dialect(struct smb2_context *smb2);







void smb2_set_security_mode(struct smb2_context *smb2, uint16_t security_mode);















void smb2_set_seal(struct smb2_context *smb2, int val);






void smb2_set_sign(struct smb2_context *smb2, int val);

enum smb2_sec {
        SMB2_SEC_UNDEFINED = 0,
        SMB2_SEC_NTLMSSP,
        SMB2_SEC_KRB5,
};







void smb2_set_authentication(struct smb2_context *smb2, int val);





void smb2_set_user(struct smb2_context *smb2, const char *user);





const char *smb2_get_user(struct smb2_context *smb2);





void smb2_set_password(struct smb2_context *smb2, const char *password);




void smb2_win_to_timeval(uint64_t smb2_time, struct smb2_timeval *tv);




uint64_t smb2_timeval_to_win(struct smb2_timeval *tv);




void smb2_set_error(struct smb2_context *smb2,
                    const char *error_string, ...);





void smb2_register_error_callback(struct smb2_context *smb,
                    smb2_error_cb error_cb);




void smb2_set_oplock_or_lease_break_callback(struct smb2_context *smb2,
                    smb2_oplock_or_lease_break_cb cb);





void smb2_set_password_from_file(struct smb2_context *smb2);





void smb2_set_domain(struct smb2_context *smb2, const char *domain);





const char *smb2_get_domain(struct smb2_context *smb2);




void smb2_set_workstation(struct smb2_context *smb2, const char *workstation);





const char *smb2_get_workstation(struct smb2_context *smb2);





void smb2_set_opaque(struct smb2_context *smb2, void *opaque);




void *smb2_get_opaque(struct smb2_context *smb2);







int smb2_delegate_credentials(struct smb2_context *in, struct smb2_context *out);




void smb2_set_client_guid(struct smb2_context *smb2, const uint8_t guid[SMB2_GUID_SIZE]);




const char *smb2_get_client_guid(struct smb2_context *smb2);




const char *smb2_get_server_guid(struct smb2_context *smb2);
        














int smb2_connect_async(struct smb2_context *smb2, const char *server,
                       smb2_command_cb cb, void *cb_data);
















int smb2_connect_share_async(struct smb2_context *smb2,
                             const char *server,
                             const char *share,
                             const char *user,
                             smb2_command_cb cb, void *cb_data);









int smb2_connect_share(struct smb2_context *smb2,
                       const char *server,
                       const char *share,
                       const char *user);















int smb2_disconnect_share_async(struct smb2_context *smb2,
                                smb2_command_cb cb, void *cb_data);








int smb2_disconnect_share(struct smb2_context *smb2);









int smb2_select_tree_id(struct smb2_context *smb2, uint32_t tree_id);

struct smb2_pdu;








int smb2_get_tree_id_for_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu, uint32_t *tree_id);
int smb2_set_tree_id_for_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu, uint32_t tree_id);








int smb2_get_session_id(struct smb2_context *smb2, uint64_t *session_id);




const char *smb2_get_error(struct smb2_context *smb2);

int smb2_get_nterror(struct smb2_context *smb2);

struct smb2_url {
        const char *domain;
        const char *user;
        const char *server;
        const char *share;
        const char *path;
};


const char *nterror_to_str(uint32_t status);


int nterror_to_errno(uint32_t status);














struct smb2_url *smb2_parse_url(struct smb2_context *smb2, const char *url);
void smb2_destroy_url(struct smb2_url *url);

















void smb2_add_compound_pdu(struct smb2_context *smb2,
                           struct smb2_pdu *pdu, struct smb2_pdu *next_pdu);











void smb2_add_unrelated_compound_pdu(struct smb2_context *smb2,
                                     struct smb2_pdu *pdu,
                                     struct smb2_pdu *next_pdu);

void smb2_free_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu);
void smb2_queue_pdu(struct smb2_context *smb2, struct smb2_pdu *pdu);





struct smb2_pdu *smb2_get_compound_pdu(struct smb2_context *smb2,
                           struct smb2_pdu *pdu);
void smb2_set_pdu_status(struct smb2_context *smb2, struct smb2_pdu *pdu, int status);
void smb2_set_pdu_message_id(struct smb2_context *smb2, struct smb2_pdu *pdu, uint64_t message_id);
uint64_t smb2_get_pdu_message_id(struct smb2_context *smb2, struct smb2_pdu *pdu);
uint64_t smb2_get_last_request_message_id(struct smb2_context *smb2);
uint64_t smb2_get_last_reply_message_id(struct smb2_context *smb2);
int smb2_pdu_is_compound(struct smb2_context *smb2);




struct smb2dir;



















struct smb2_pdu *
smb2_opendir_async_pdu(struct smb2_context *smb2, const char *path,
                       smb2_command_cb cb, void *cb_data, void (*free_cb)(void *));






struct smb2dir *smb2_opendir(struct smb2_context *smb2, const char *path);

int smb2_opendir_async(struct smb2_context *smb2, const char *path,
                       smb2_command_cb cb, void *cb_data);







void smb2_closedir(struct smb2_context *smb2, struct smb2dir *smb2dir);







struct smb2dirent *smb2_readdir(struct smb2_context *smb2,
                                struct smb2dir *smb2dir);







void smb2_rewinddir(struct smb2_context *smb2, struct smb2dir *smb2dir);







long smb2_telldir(struct smb2_context *smb2, struct smb2dir *smb2dir);







void smb2_seekdir(struct smb2_context *smb2, struct smb2dir *smb2dir,
                  long loc);




struct smb2fh;



























struct smb2_pdu *
smb2_open_async_pdu(struct smb2_context *smb2, const char *path, int flags,
                    smb2_command_cb cb, void *cb_data, void (*free_cb)(void *));








int smb2_open_async_with_oplock_or_lease(struct smb2_context *smb2, const char *path, int flags,
                    uint8_t oplock_level, uint32_t lease_state, smb2_lease_key lease_key,
                    smb2_command_cb cb, void *cb_data);

int smb2_open_async(struct smb2_context *smb2, const char *path, int flags,
                    smb2_command_cb cb, void *cb_data);






struct smb2fh *smb2_open(struct smb2_context *smb2, const char *path, int flags);


















int smb2_close_async(struct smb2_context *smb2, struct smb2fh *fh,
                     smb2_command_cb cb, void *cb_data);




int smb2_close(struct smb2_context *smb2, struct smb2fh *fh);


















int smb2_fsync_async(struct smb2_context *smb2, struct smb2fh *fh,
                     smb2_command_cb cb, void *cb_data);




int smb2_fsync(struct smb2_context *smb2, struct smb2fh *fh);





uint32_t smb2_get_max_read_size(struct smb2_context *smb2);
uint32_t smb2_get_max_write_size(struct smb2_context *smb2);

struct smb2_read_cb_data {
        struct smb2fh *fh;
        uint8_t *buf;
        uint32_t count;
        uint64_t offset;
};

struct smb2_write_cb_data {
        struct smb2fh *fh;
        const uint8_t *buf;
        uint32_t count;
        uint64_t offset;
};






















int smb2_pread_async(struct smb2_context *smb2, struct smb2fh *fh,
                     uint8_t *buf, uint32_t count, uint64_t offset,
                     smb2_command_cb cb, void *cb_data);






int smb2_pread(struct smb2_context *smb2, struct smb2fh *fh,
               uint8_t *buf, uint32_t count, uint64_t offset);






















int smb2_pwrite_async(struct smb2_context *smb2, struct smb2fh *fh,
                      const uint8_t *buf, uint32_t count, uint64_t offset,
                      smb2_command_cb cb, void *cb_data);






int smb2_pwrite(struct smb2_context *smb2, struct smb2fh *fh,
                const uint8_t *buf, uint32_t count, uint64_t offset);





















int smb2_read_async(struct smb2_context *smb2, struct smb2fh *fh,
                    uint8_t *buf, uint32_t count,
                    smb2_command_cb cb, void *cb_data);




int smb2_read(struct smb2_context *smb2, struct smb2fh *fh,
              uint8_t *buf, uint32_t count);





















int smb2_write_async(struct smb2_context *smb2, struct smb2fh *fh,
                     const uint8_t *buf, uint32_t count,
                     smb2_command_cb cb, void *cb_data);




int smb2_write(struct smb2_context *smb2, struct smb2fh *fh,
               const uint8_t *buf, uint32_t count);









int64_t smb2_lseek(struct smb2_context *smb2, struct smb2fh *fh,
                   int64_t offset, int whence, uint64_t *current_offset);


















int smb2_unlink_async(struct smb2_context *smb2, const char *path,
                      smb2_command_cb cb, void *cb_data);




int smb2_unlink(struct smb2_context *smb2, const char *path);


















int smb2_rmdir_async(struct smb2_context *smb2, const char *path,
                     smb2_command_cb cb, void *cb_data);




int smb2_rmdir(struct smb2_context *smb2, const char *path);


















int smb2_mkdir_async(struct smb2_context *smb2, const char *path,
                     smb2_command_cb cb, void *cb_data);




int smb2_mkdir(struct smb2_context *smb2, const char *path);
















int smb2_statvfs_async(struct smb2_context *smb2, const char *path,
                       struct smb2_statvfs *statvfs,
                       smb2_command_cb cb, void *cb_data);



int smb2_statvfs(struct smb2_context *smb2, const char *path,
                 struct smb2_statvfs *statvfs);
















int smb2_fstat_async(struct smb2_context *smb2, struct smb2fh *fh,
                     struct smb2_stat_64 *st,
                     smb2_command_cb cb, void *cb_data);



int smb2_fstat(struct smb2_context *smb2, struct smb2fh *fh,
               struct smb2_stat_64 *st);













int smb2_stat_async(struct smb2_context *smb2, const char *path,
                    struct smb2_stat_64 *st,
                    smb2_command_cb cb, void *cb_data);



int smb2_stat(struct smb2_context *smb2, const char *path,
              struct smb2_stat_64 *st);













int smb2_rename_async(struct smb2_context *smb2, const char *oldpath,
                      const char *newpath, smb2_command_cb cb, void *cb_data);




int smb2_rename(struct smb2_context *smb2, const char *oldpath,
                const char *newpath);













int smb2_link_async(struct smb2_context *smb2, const char *oldpath,
                    const char *newpath, smb2_command_cb cb, void *cb_data);




int smb2_link(struct smb2_context *smb2, const char *oldpath,
              const char *newpath);









#define SMB2_SYMLINK_DIRECTORY 0x00000001






#define SMB2_SYMLINK_ABSOLUTE  0x00000002


















int smb2_symlink_async(struct smb2_context *smb2, const char *target,
                       const char *linkpath, uint32_t flags,
                       smb2_command_cb cb, void *cb_data);




int smb2_symlink(struct smb2_context *smb2, const char *target,
                 const char *linkpath, uint32_t flags);













int smb2_truncate_async(struct smb2_context *smb2, const char *path,
                        uint64_t length, smb2_command_cb cb, void *cb_data);






int smb2_truncate(struct smb2_context *smb2, const char *path,
                  uint64_t length);













int smb2_ftruncate_async(struct smb2_context *smb2, struct smb2fh *fh,
                         uint64_t length, smb2_command_cb cb, void *cb_data);






int smb2_ftruncate(struct smb2_context *smb2, struct smb2fh *fh,
                   uint64_t length);

















int smb2_readlink_async(struct smb2_context *smb2, const char *path,
                        smb2_command_cb cb, void *cb_data);




int smb2_readlink(struct smb2_context *smb2, const char *path, char *buf, uint32_t bufsiz);

















int smb2_request_resume_key_async(struct smb2_context *smb2, struct smb2fh *fh,
                                  smb2_command_cb cb, void *cb_data);




int smb2_request_resume_key(struct smb2_context *smb2, struct smb2fh *fh,
                            struct smb2_srv_copychunk_resume_key *resume_key);
















int smb2_copychunk_async(struct smb2_context *smb2,
                         uint32_t ctl_code,
                         const struct smb2_srv_copychunk_resume_key *resume_key,
                         struct smb2fh *dstfh,
                         const struct smb2_srv_copychunk *chunks,
                         uint32_t chunk_count,
                         smb2_command_cb cb, void *cb_data);





int smb2_copychunk(struct smb2_context *smb2,
                   uint32_t ctl_code,
                   const struct smb2_srv_copychunk_resume_key *resume_key,
                   struct smb2fh *dstfh,
                   const struct smb2_srv_copychunk *chunks,
                   uint32_t chunk_count,
                   struct smb2_srv_copychunk_reply *reply);












int smb2_server_side_copy_async(struct smb2_context *smb2,
                                uint32_t ctl_code,
                                struct smb2fh *srcfh, struct smb2fh *dstfh,
                                const struct smb2_srv_copychunk *chunks,
                                uint32_t chunk_count,
                                smb2_command_cb cb, void *cb_data);






int smb2_server_side_copy(struct smb2_context *smb2,
                          uint32_t ctl_code,
                          struct smb2fh *srcfh, struct smb2fh *dstfh,
                          const struct smb2_srv_copychunk *chunks,
                          uint32_t chunk_count,
                          struct smb2_srv_copychunk_reply *reply);













int smb2_echo_async(struct smb2_context *smb2,
                    smb2_command_cb cb, void *cb_data);








int smb2_echo(struct smb2_context *smb2);

void
free_smb2_file_notify_change_information(struct smb2_context *smb2, struct smb2_file_notify_change_information *fnc);

int smb2_notify_change_async(struct smb2_context *smb2, const char *path, uint16_t flags, uint32_t filter, int loop,
                       smb2_command_cb cb, void *cb_data);

int smb2_notify_change_filehandle_async(struct smb2_context *smb2, struct smb2fh *smb2_dir_fh, uint16_t flags, uint32_t filter, int loop,
                       smb2_command_cb cb, void *cb_data);





struct smb2_file_notify_change_information *smb2_notify_change(struct smb2_context *smb2, const char *path, uint16_t flags, uint32_t filter);





struct smb2_utf16 {
        int len;
        uint16_t val[1];
};




struct smb2_utf16 *smb2_utf8_to_utf16(const char *utf8);




const char *smb2_utf16_to_utf8(const uint16_t *str, size_t len);


struct smb2_server;






struct smb2_server_request_handlers {
        int (*destruction_event)(struct smb2_server *srvr, struct smb2_context *smb2);
        int (*authorize_user)(struct smb2_server *srvr, struct smb2_context *smb2,
                            const char *user,
                            const char *domain,
                            const char *workstation);
        int (*session_established)(struct smb2_server *srvr, struct smb2_context *smb2);
        int (*logoff_cmd)(struct smb2_server *srvr, struct smb2_context *smb2);
        int (*tree_connect_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_tree_connect_request *req,
                            struct smb2_tree_connect_reply *rep);
        int (*tree_disconnect_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            const uint32_t tree_id);
        int (*create_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_create_request *req,
                            struct smb2_create_reply *rep);
        int (*close_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_close_request *req,
                            struct smb2_close_reply *rep);
        int (*flush_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_flush_request *req);
        int (*read_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_read_request *req,
                            struct smb2_read_reply *rep);
        int (*write_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_write_request *req,
                            struct smb2_write_reply *rep);
        int (*oplock_break_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_oplock_break_acknowledgement *req);
        int (*lease_break_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_lease_break_acknowledgement *req);
        int (*lock_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_lock_request *req);





        int (*ioctl_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_ioctl_request *req,
                            struct smb2_ioctl_reply *rep);
        int (*cancel_cmd)(struct smb2_server *srvr, struct smb2_context *smb2);
        int (*echo_cmd)(struct smb2_server *srvr, struct smb2_context *smb2);
        int (*query_directory_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_query_directory_request *req,
                            struct smb2_query_directory_reply *rep);
        int (*change_notify_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_change_notify_request *req,
                            struct smb2_change_notify_reply *rep);
        int (*query_info_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_query_info_request *req,
                            struct smb2_query_info_reply *rep);
        int (*set_info_cmd)(struct smb2_server *srvr, struct smb2_context *smb2,
                            struct smb2_set_info_request *req);




};

struct smb2_server {
        uint8_t guid[16];
        char hostname[128];
        char domain[128];
        int fd;
        uint16_t port;
        uint64_t session_counter;
        struct smb2_server_request_handlers *handlers;
        uint32_t max_transact_size;
        uint32_t max_read_size;
        uint32_t max_write_size;
        int signing_enabled;
        int allow_anonymous;


        int proxy_authentication;

        uint32_t capabilities;
        uint32_t security_mode;

        char keytab_path[256];
        char error[128];
        void *auth_data;






        void (*extra_fdset)(struct smb2_server *server,
                            fd_set *rfds, fd_set *wfds, int *maxfd);
        void (*extra_service)(struct smb2_server *server,
                              fd_set *rfds, fd_set *wfds);






        volatile int stop_requested;






        volatile int listener_ready;
};

int smb2_bind_and_listen(const uint16_t port, const int max_connections, int *out_fd);
int smb2_accept_connection_async(const int fd, const int to_msecs, smb2_accepted_cb cb, void *cb_data);
int smb2_serve_port_async(const int fd, const int to_msecs, struct smb2_context **out_smb2);








int smb2_serve_port(struct smb2_server *server, const int max_connections, smb2_client_connection cb, void *cb_data);





#include <smb2/libsmb2-share-enum.h>

#ifdef __cplusplus
}
#endif
#endif
