/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#include "bctoolbox/crypto.h"
#include "liblinphone_tester.h"
#include "linphone/api/c-chat-room-params.h"
#include "linphone/api/c-types.h"
#include "linphone/core.h"
#include "linphone/wrapper_utils.h"
#include "tester_utils.h"

#include <pthread.h>

#define TEST_GROUP_ID "test group id"

struct get_msg_args {
	LinphoneCoreManager *sender_mgr;
	LinphoneCore *receiver;
	const char *text;
	const char *call_id;
};

static void shared_main_core_prevent_executor_core_start(void) {
#if TARGET_OS_IPHONE
	LinphoneCoreManager *main_mgr;
	LinphoneCoreManager *executor_mgr;
	main_mgr = linphone_core_manager_create_shared("marie_rc", TEST_GROUP_ID, TRUE);
	executor_mgr = linphone_core_manager_create_shared("marie_rc", TEST_GROUP_ID, FALSE); // TODO PAUL : are they created with the samse config ?
	linphone_core_reset_shared_core_state(main_mgr->lc);

	linphone_core_manager_start(main_mgr, TRUE);
	BC_ASSERT_TRUE(wait_for_until(main_mgr->lc, NULL, &main_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));
	BC_ASSERT_EQUAL(linphone_core_start(executor_mgr->lc), -1, int, "%d");

	linphone_core_manager_destroy(executor_mgr); // TODO PAUL : fait demarrer le core si on le met en dernier -> pas bon
	linphone_core_manager_destroy(main_mgr);
#endif
}

static void shared_main_core_stops_executor_core(void) {
#if TARGET_OS_IPHONE
	LinphoneCoreManager* main_mgr;
	LinphoneCoreManager* executor_mgr;
	main_mgr = linphone_core_manager_create_shared("marie_rc", TEST_GROUP_ID, TRUE);
	executor_mgr = linphone_core_manager_create_shared("marie_rc", TEST_GROUP_ID, FALSE);
	linphone_proxy_config_set_push_notification_allowed(linphone_core_get_default_proxy_config(main_mgr->lc), TRUE);
	linphone_proxy_config_set_push_notification_allowed(linphone_core_get_default_proxy_config(executor_mgr->lc), TRUE);
	linphone_core_reset_shared_core_state(main_mgr->lc);

	linphone_core_manager_start(executor_mgr, TRUE);
	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, NULL, &executor_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));

	linphone_core_manager_start(main_mgr, TRUE);
	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, main_mgr->lc, &executor_mgr->stat.number_of_LinphoneGlobalOff, 1, 2000));
	BC_ASSERT_TRUE(wait_for_until(main_mgr->lc, executor_mgr->lc, &main_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));

	linphone_core_manager_destroy(main_mgr);
	linphone_core_manager_destroy(executor_mgr);
#endif
}

const char *shared_core_send_msg_and_get_call_id(LinphoneCoreManager *sender, LinphoneCoreManager *receiver,
												 const char *text) {
	stats initial_sender_stat = sender->stat;
	const char *content_type = "text/plain";
	LinphoneChatRoom *room = linphone_core_get_chat_room(sender->lc, receiver->identity);
	// BC_ASSERT_TRUE(linphone_chat_room_is_empty(room));

	LinphoneChatMessage *msg = linphone_chat_room_create_message(room, text);
	linphone_chat_message_set_content_type(msg, content_type);
	LinphoneChatMessageCbs *cbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(cbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);

	BC_ASSERT_TRUE(wait_for_until(sender->lc, NULL, &sender->stat.number_of_LinphoneMessageDelivered,
								  initial_sender_stat.number_of_LinphoneMessageDelivered + 1, 30000));
	// const char * call_id = linphone_chat_message_get_custom_header(msg, "Call-ID");
	const char *call_id = linphone_chat_message_get_message_id(msg);
	linphone_chat_message_unref(msg);
	return ms_strdup(call_id);
}

// receiver needs to be a shared core
void shared_core_get_message_from_call_id(LinphoneCoreManager *sender_mgr, LinphoneCore *receiver, const char *text,
										  const char *call_id) {
	const char *content_type = "text/plain";

	BC_ASSERT_PTR_NOT_NULL(call_id);
	if (call_id != NULL) {
		LinphoneChatMessage *received_msg = linphone_core_get_push_notification_message(receiver, call_id);
		BC_ASSERT_PTR_NOT_NULL(received_msg);

		if (received_msg != NULL) {
			LinphoneContent *content = (LinphoneContent *)(linphone_chat_message_get_contents(received_msg)->data);
			char *content_type_header = ms_strdup_printf("Content-Type: %s", content_type);
			belle_sip_header_content_type_t *belle_sip_content_type =
				belle_sip_header_content_type_parse(content_type_header);
			BC_ASSERT_STRING_EQUAL(linphone_content_get_type(content),
								   belle_sip_header_content_type_get_type(belle_sip_content_type));
			BC_ASSERT_STRING_EQUAL(linphone_content_get_subtype(content),
								   belle_sip_header_content_type_get_subtype(belle_sip_content_type));
			BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(received_msg), text);
			ms_free(content_type_header);
			linphone_chat_message_unref(received_msg);

			BC_ASSERT_PTR_NOT_NULL(linphone_core_get_chat_room(receiver, sender_mgr->identity));
		}

	}
	linphone_core_stop(receiver);
}

void *thread_shared_core_get_message_from_call_id(void *arguments) {
	struct get_msg_args *args = (struct get_msg_args *)arguments;
	shared_core_get_message_from_call_id(args->sender_mgr, args->receiver, args->text, args->call_id);
	pthread_exit(NULL);
	return NULL;
}

LinphoneCore *get_another_core_from_manager(LinphoneCoreManager *mgr) {
	LinphoneCore *lc = linphone_core_manager_configure_lc(mgr);

	LinphoneProxyConfig *mgr_proxy = (LinphoneProxyConfig *)linphone_core_get_proxy_config_list(mgr->lc)->data;
	const char *mgr_proxy_username =
		linphone_address_get_username(linphone_proxy_config_get_identity_address(mgr_proxy));

	LinphoneProxyConfig *new_core_proxy = (LinphoneProxyConfig *)linphone_core_get_proxy_config_list(lc)->data;
	LinphoneAddress *id_addr = linphone_address_clone(linphone_proxy_config_get_identity_address(new_core_proxy));
	linphone_address_set_username(id_addr, mgr_proxy_username);
	linphone_proxy_config_set_identity_address(new_core_proxy, id_addr);
	linphone_address_unref(id_addr);

	LinphoneAuthInfo *auth_info =
		linphone_auth_info_clone((LinphoneAuthInfo *)(linphone_core_get_auth_info_list(mgr->lc)->data));
	linphone_core_clear_all_auth_info(lc);
	linphone_core_add_auth_info(lc, auth_info);
	linphone_auth_info_unref(auth_info);

	return lc;
}

static void shared_executor_core_get_message(void) {
#if TARGET_OS_IPHONE
	LinphoneCoreManager *sender_mgr;
	LinphoneCoreManager *receiver_mgr;
	const char *text = "Bli bli bli \n blu";
	sender_mgr = linphone_core_manager_new("marie_rc");
	receiver_mgr = linphone_core_manager_create_shared("pauline_rc", TEST_GROUP_ID, FALSE);
	linphone_core_reset_shared_core_state(receiver_mgr->lc);
	linphone_core_manager_start(receiver_mgr, TRUE);

	const char *call_id = shared_core_send_msg_and_get_call_id(sender_mgr, receiver_mgr, text);

	shared_core_get_message_from_call_id(sender_mgr, receiver_mgr->lc, text, call_id); // TODO PAUL : check newMsgNn
	ms_free((void *)call_id);
	linphone_core_manager_destroy(sender_mgr);
	linphone_core_manager_destroy(receiver_mgr);
#endif
}

static void two_shared_executor_cores_get_messages(void) {
#if TARGET_OS_IPHONE
	LinphoneCoreManager *sender_mgr = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *receiver_mgr = linphone_core_manager_create_shared("pauline_rc", TEST_GROUP_ID, FALSE);

	// linphone_proxy_config_set_push_notification_allowed(linphone_core_get_default_proxy_config(sender_mgr->lc), TRUE);
	// linphone_proxy_config_set_push_notification_allowed(linphone_core_get_default_proxy_config(receiver_mgr1->lc), TRUE);
	LinphoneCore *receiver_lc1 = get_another_core_from_manager(receiver_mgr);
	LinphoneCore *receiver_lc2 = get_another_core_from_manager(receiver_mgr);
	linphone_core_reset_shared_core_state(receiver_mgr->lc);

	linphone_core_manager_start(receiver_mgr, TRUE);
	const char *call_id1 = shared_core_send_msg_and_get_call_id(sender_mgr, receiver_mgr, "message1");
	const char *call_id2 = shared_core_send_msg_and_get_call_id(sender_mgr, receiver_mgr, "message2");
	linphone_core_stop(receiver_mgr->lc);

	pthread_t executor1;
	struct get_msg_args *args1 = ms_malloc(sizeof(struct get_msg_args));
	args1->sender_mgr = sender_mgr;
	args1->receiver = receiver_lc1;
	args1->text = "message1";
	args1->call_id = call_id1;
	if (pthread_create(&executor1, NULL, &thread_shared_core_get_message_from_call_id, (void *)args1)) {
		ms_fatal("Error creating executor1 thread");
	}

	pthread_t executor2;
	struct get_msg_args *args2 = ms_malloc(sizeof(struct get_msg_args));
	args2->sender_mgr = sender_mgr;
	args2->receiver = receiver_lc2;
	args2->text = "message2";
	args2->call_id = call_id2;

	if (pthread_create(&executor2, NULL, &thread_shared_core_get_message_from_call_id, (void *)args2)) {
		ms_fatal("Error creating executor1 thread");
	}

	if (pthread_join(executor1, NULL)) {
		ms_fatal("Error joining thread executor1");
	}
	if (pthread_join(executor2, NULL)) {
		ms_fatal("Error joining thread executor1");
	}

	ms_free(args1);
	ms_free(args2);

	// shared_core_get_message_from_call_id(sender_mgr, receiver_mgr1->lc, "message1", call_id1);
	if (call_id1) {
		ms_free((void *)call_id1);
	}
	if (call_id2) {
		ms_free((void *)call_id2);
	}

	linphone_core_destroy(receiver_lc1);
	linphone_core_destroy(receiver_lc2);
	linphone_core_manager_destroy(receiver_mgr);
	linphone_core_manager_destroy(sender_mgr);
#endif
}

static void shared_executor_core_get_chat_room(void) {
#if TARGET_OS_IPHONE
	const char *subject = "new chat room";
	LinphoneCoreManager *sender = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *receiver = linphone_core_manager_create_shared("pauline_rc", TEST_GROUP_ID, FALSE);
	linphone_core_reset_shared_core_state(receiver->lc);

	bctbx_list_t *coresManagerList = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, sender);
	coresManagerList = bctbx_list_append(coresManagerList, receiver);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);

	start_core_for_conference(coresManagerList);

	LinphoneAddress *receiverAddr = linphone_address_new(linphone_core_get_identity(receiver->lc));
	bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, (void *)receiverAddr);

	LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(sender->lc);

	// Should create a one-to-one flexisip chat
	linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendFlexisipChat);
	linphone_chat_room_params_enable_group(params, TRUE);
	LinphoneChatRoom *senderCr = linphone_core_create_chat_room_2(sender->lc, params, subject, participantsAddresses);
	linphone_chat_room_params_unref(params);
	BC_ASSERT_PTR_NOT_NULL(senderCr);

	BC_ASSERT_TRUE(wait_for_until(sender->lc, NULL, &sender->stat.number_of_LinphoneChatRoomStateCreated, 1, 5000)); // 65
	LinphoneAddress *senderAddr = linphone_address_new(linphone_proxy_config_get_identity(linphone_core_get_default_proxy_config(sender->lc)));
	BC_ASSERT_TRUE(linphone_address_weak_equal(linphone_chat_room_get_local_address(senderCr), senderAddr));
	linphone_address_unref(senderAddr);

	const char *confAddrUsername = linphone_address_get_username(linphone_chat_room_get_conference_address(senderCr));
	LinphoneChatRoom *receiverCr = linphone_core_get_push_notification_chat_room_invite(receiver->lc, confAddrUsername);
	linphone_chat_room_unref(receiverCr); // linphone_core_get_push_notification_chat_room_invite takes a ref on the chat room, required when called from outside sdk
//	ms_free((void *)confAddrUsername);

	BC_ASSERT_PTR_NOT_NULL(receiverCr);
	if (receiverCr != NULL) {
		BC_ASSERT_TRUE(strcmp(linphone_chat_room_get_subject(receiverCr), subject) == 0);
		BC_ASSERT_TRUE(linphone_address_weak_equal(linphone_chat_room_get_local_address(receiverCr), receiverAddr));
	}

	linphone_core_manager_delete_chat_room(sender, senderCr, coresList);
	linphone_core_manager_delete_chat_room(receiver, receiverCr, coresList);
	linphone_chat_room_unref(receiverCr);
//	linphone_chat_room_unref(senderCr);

	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(sender);
	linphone_core_manager_destroy(receiver);
#endif
}

static void two_shared_executor_cores_get_message_and_chat_room(void) {
#if TARGET_OS_IPHONE
#endif
}

test_t shared_core_tests[] = {
	TEST_NO_TAG("Executor Shared Core can't start because Main Shared Core runs", shared_main_core_prevent_executor_core_start),
	TEST_NO_TAG("Executor Shared Core stopped by Main Shared Core", shared_main_core_stops_executor_core),
	TEST_NO_TAG("Executor Shared Core get message from callId", shared_executor_core_get_message),
	TEST_NO_TAG("Two Executor Shared Cores get messages", two_shared_executor_cores_get_messages),
	TEST_NO_TAG("Executor Shared Core get new chat room from invite", shared_executor_core_get_chat_room),
	TEST_NO_TAG("Two Executor Shared Cores get one msg and one chat room", two_shared_executor_cores_get_message_and_chat_room)
	// group chat msg tester?
};

test_suite_t shared_core_test_suite = {"Shared Core",
									   NULL,
									   NULL,
									   liblinphone_tester_before_each,
									   liblinphone_tester_after_each,
									   sizeof(shared_core_tests) / sizeof(shared_core_tests[0]),
									   shared_core_tests};
