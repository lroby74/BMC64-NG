
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

#ifndef _LIBSMB2_RAW_H_
#define _LIBSMB2_RAW_H_

#ifdef __cplusplus
extern "C" {
#endif







extern const smb2_file_id compound_file_id;




void smb2_free_data(struct smb2_context *smb2, void *ptr);

















struct smb2_pdu *smb2_cmd_negotiate_async(struct smb2_context *smb2,
                                          struct smb2_negotiate_request *req,
                                          smb2_command_cb cb, void *cb_data);


struct smb2_pdu *smb2_cmd_negotiate_reply_async(struct smb2_context *smb2,
                                          struct smb2_negotiate_reply *rep,
                                          smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_session_setup_async(struct smb2_context *smb2,
                                 struct smb2_session_setup_request *req,
                                 smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_session_setup_reply_async(struct smb2_context *smb2,
                                struct smb2_session_setup_reply *rep,
                                smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_tree_connect_async(struct smb2_context *smb2,
                                struct smb2_tree_connect_request *req,
                                smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_tree_connect_reply_async(struct smb2_context *smb2,
                            struct smb2_tree_connect_reply *rep,
                            uint32_t tree_id,
                            smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_tree_disconnect_async(struct smb2_context *smb2,
                                  smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_tree_disconnect_reply_async(struct smb2_context *smb2,
                                  smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_create_async(struct smb2_context *smb2,
                                       struct smb2_create_request *req,
                                       smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_create_reply_async(struct smb2_context *smb2,
                                       struct smb2_create_reply *rep,
                                       smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_close_async(struct smb2_context *smb2,
                                      struct smb2_close_request *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_close_reply_async(struct smb2_context *smb2,
                                      struct smb2_close_reply *rep,
                                      smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_read_async(struct smb2_context *smb2,
                                     struct smb2_read_request *req,
                                     smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_read_reply_async(struct smb2_context *smb2,
                                     struct smb2_read_reply *rep,
                                     smb2_command_cb cb, void *cb_data);





















struct smb2_pdu *smb2_cmd_write_async(struct smb2_context *smb2,
                                      struct smb2_write_request *req,
                                      int pass_buf_ownership,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_write_reply_async(struct smb2_context *smb2,
                                      struct smb2_write_reply *rep,
                                      smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_query_directory_async(struct smb2_context *smb2,
                             struct smb2_query_directory_request *req,
                             smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_query_directory_reply_async(struct smb2_context *smb2,
                             struct smb2_query_directory_request *req,
                             struct smb2_query_directory_reply *rep,
                             smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_change_notify_async(struct smb2_context *smb2,
                                      struct smb2_change_notify_request *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_change_notify_reply_async(struct smb2_context *smb2,
                                      struct smb2_change_notify_reply *rep,
                                      smb2_command_cb cb, void *cb_data);



















struct smb2_pdu *smb2_cmd_query_info_async(struct smb2_context *smb2,
                                           struct smb2_query_info_request *req,
                                           smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_query_info_reply_async(struct smb2_context *smb2,
                                           struct smb2_query_info_request *req,
                                           struct smb2_query_info_reply *rep,
                                           smb2_command_cb cb, void *cb_data);
















struct smb2_pdu *smb2_cmd_set_info_async(struct smb2_context *smb2,
                                         struct smb2_set_info_request *req,
                                         smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_set_info_reply_async(struct smb2_context *smb2,
                                         struct smb2_set_info_request *req,
                                         smb2_command_cb cb, void *cb_data);



















struct smb2_pdu *smb2_cmd_ioctl_async(struct smb2_context *smb2,
                                      struct smb2_ioctl_request *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_ioctl_reply_async(struct smb2_context *smb2,
                                      struct smb2_ioctl_reply *rep,
                                      smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_echo_async(struct smb2_context *smb2,
                                     smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_echo_reply_async(struct smb2_context *smb2,
                                     smb2_command_cb cb, void *cb_data);






















struct smb2_pdu *smb2_cmd_cancel_async(struct smb2_context *smb2,
                                       uint64_t msg_id,
                                       smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_lock_async(struct smb2_context *smb2,
                                       struct smb2_lock_request *req,
                                       smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_lock_reply_async(struct smb2_context *smb2,
                                       smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_logoff_async(struct smb2_context *smb2,
                                       smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_logoff_reply_async(struct smb2_context *smb2,
                                       smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_flush_async(struct smb2_context *smb2,
                                      struct smb2_flush_request *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_flush_reply_async(struct smb2_context *smb2,
                                      smb2_command_cb cb, void *cb_data);

















struct smb2_pdu *smb2_cmd_oplock_break_async(struct smb2_context *smb2,
                                      struct smb2_oplock_break_acknowledgement *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_oplock_break_reply_async(struct smb2_context *smb2,
                                      struct smb2_oplock_break_reply *rep,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_oplock_break_notification_async(struct smb2_context *smb2,
                                      struct smb2_oplock_break_notification *rep,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_lease_break_async(struct smb2_context *smb2,
                                      struct smb2_lease_break_acknowledgement *req,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_lease_break_reply_async(struct smb2_context *smb2,
                                      struct smb2_lease_break_reply *rep,
                                      smb2_command_cb cb, void *cb_data);

struct smb2_pdu *smb2_cmd_lease_break_notification_async(struct smb2_context *smb2,
                                      struct smb2_lease_break_notification *req,
                                      smb2_command_cb cb, void *cb_data);



struct smb2_pdu *smb2_cmd_error_reply_async(struct smb2_context *smb2,
                                      struct smb2_error_reply *rep,
                                      uint8_t causing_command,
                                      int status,
                                      smb2_command_cb cb, void *cb_data);
#ifdef __cplusplus
}
#endif

#endif
