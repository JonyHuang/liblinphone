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

#include "TargetConditionals.h"

#include "linphone/core.h"
#include "linphone/api/c-types.h"
#include "linphone/api/c-chat-room-params.h"
#include "tester_utils.h"
#include "linphone/wrapper_utils.h"
#include "liblinphone_tester.h"
#include "bctoolbox/crypto.h"


#define TEST_GROUP_ID "test group id"

static void shared_main_core_prevent_executor_core_start (void) {
// #if TARGET_OS_IPHONE
	LinphoneCoreManager* main_mgr;
	LinphoneCoreManager* executor_mgr;
	main_mgr = linphone_core_manager_new_shared("marie_rc", TEST_GROUP_ID, TRUE);
	executor_mgr = linphone_core_manager_new_shared("pauline_rc", TEST_GROUP_ID, FALSE);

	linphone_core_manager_start(main_mgr, TRUE);
	BC_ASSERT_TRUE(wait_for_until(main_mgr->lc, NULL, &main_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));
	BC_ASSERT_EQUAL(linphone_core_start(executor_mgr->lc), -1, int, "%d");

	linphone_core_manager_destroy(main_mgr);
	linphone_core_manager_destroy(executor_mgr);
// #endif
// BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");
}

static void shared_main_core_stops_executor_core (void) {
	LinphoneCoreManager* main_mgr;
	LinphoneCoreManager* executor_mgr;
	main_mgr = linphone_core_manager_new_shared("marie_rc", TEST_GROUP_ID, TRUE);
	executor_mgr = linphone_core_manager_new_shared("pauline_rc", TEST_GROUP_ID, FALSE);

	linphone_core_manager_start(executor_mgr, TRUE); //TODO PAUL : mettre dans le create?
	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, NULL, &executor_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));

	linphone_core_manager_start(main_mgr, TRUE);
	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, main_mgr->lc, &executor_mgr->stat.number_of_LinphoneGlobalOff, 2, 2000));
	BC_ASSERT_TRUE(wait_for_until(main_mgr->lc, executor_mgr->lc, &main_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));

	linphone_core_manager_destroy(main_mgr);
	linphone_core_manager_destroy(executor_mgr);
}








void atext_message_base_with_text_and_forward(LinphoneCoreManager* marie, LinphoneCoreManager* pauline, const char* text, const char* content_type) {
	LinphoneChatRoom *room = linphone_core_get_chat_room(pauline->lc, marie->identity);
	BC_ASSERT_TRUE(linphone_chat_room_is_empty(room));
	
	LinphoneChatMessage* msg = linphone_chat_room_create_message(room, text);
	linphone_chat_message_set_content_type(msg, content_type);
	LinphoneChatMessageCbs *cbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(cbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);

	BC_ASSERT_TRUE(wait_for(pauline->lc,marie->lc,&pauline->stat.number_of_LinphoneMessageDelivered,1));
	BC_ASSERT_TRUE(wait_for(pauline->lc,marie->lc,&marie->stat.number_of_LinphoneMessageReceived,1));
	BC_ASSERT_PTR_NOT_NULL(marie->stat.last_received_chat_message);
	if (marie->stat.last_received_chat_message != NULL) {
		LinphoneContent *content = (LinphoneContent *)(linphone_chat_message_get_contents(marie->stat.last_received_chat_message)->data);
		char* content_type_header = ms_strdup_printf("Content-Type: %s",content_type);
		belle_sip_header_content_type_t *belle_sip_content_type = belle_sip_header_content_type_parse(content_type_header);
		BC_ASSERT_STRING_EQUAL(linphone_content_get_type(content), belle_sip_header_content_type_get_type(belle_sip_content_type));
		BC_ASSERT_STRING_EQUAL(linphone_content_get_subtype(content), belle_sip_header_content_type_get_subtype(belle_sip_content_type));
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), text);
		ms_free(content_type_header);
		LinphoneChatRoom *marieCr;
		
		const LinphoneAddress *msg_from = linphone_chat_message_get_from_address(marie->stat.last_received_chat_message);
		/* We have special case for anonymous message, that of course won't come in the chatroom to pauline.*/
		if (strcasecmp(linphone_address_get_username(msg_from), "anonymous") == 0){
			marieCr = linphone_chat_message_get_chat_room(marie->stat.last_received_chat_message);
		}else{
			marieCr = linphone_core_get_chat_room(marie->lc, pauline->identity);
		}

		if (linphone_factory_is_database_storage_available(linphone_factory_get())) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 1, int," %i");
			if (linphone_chat_room_get_history_size(marieCr) > 0) {
				bctbx_list_t *history = linphone_chat_room_get_history(marieCr, 1);
				LinphoneChatMessage *recv_msg = (LinphoneChatMessage *)(history->data);
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(recv_msg), text);
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text_content(recv_msg),text);

				bctbx_list_free_with_data(history, (bctbx_list_free_func)linphone_chat_message_unref);
			}
		}
	}

	BC_ASSERT_PTR_NOT_NULL(linphone_core_get_chat_room(marie->lc,pauline->identity));
	linphone_chat_message_unref(msg);

}

void atext_message_base_with_text(LinphoneCoreManager* marie, LinphoneCoreManager* pauline, const char* text, const char* content_type) {
	atext_message_base_with_text_and_forward(marie, pauline, text, content_type);
}

void atext_message_base(LinphoneCoreManager* marie, LinphoneCoreManager* pauline) {
	atext_message_base_with_text(marie,pauline, "Bli bli bli \n blu", "text/plain");
}












static void shared_executor_core_get_message (void) {
	LinphoneCoreManager* sender_mgr;
	LinphoneCoreManager* receiver_mgr;
	sender_mgr = linphone_core_manager_new("marie_rc");
	receiver_mgr = linphone_core_manager_new_shared("pauline_rc", TEST_GROUP_ID, FALSE);
	linphone_core_manager_start(receiver_mgr, TRUE);

	BC_ASSERT_TRUE(wait_for_until(sender_mgr->lc, NULL, &sender_mgr->stat.number_of_LinphoneRegistrationOk, 1, 2000));
	BC_ASSERT_TRUE(wait_for_until(receiver_mgr->lc, NULL, &receiver_mgr->stat.number_of_LinphoneRegistrationOk, 1, 2000));

	atext_message_base(sender_mgr, receiver_mgr);

	linphone_core_manager_destroy(sender_mgr);
	linphone_core_manager_destroy(receiver_mgr);
}

static void shared_executor_core_get_chat_room (void) {

}

static void two_shared_executor_cores_get_messages (void) {

}

static void two_shared_executor_cores_get_message_and_chat_room (void) {

}

test_t shared_core_tests[] = {
	TEST_NO_TAG("Executor Shared Core can't start because Main Shared Core runs", shared_main_core_prevent_executor_core_start),
	TEST_NO_TAG("Executor Shared Core stopped by Main Shared Core", shared_main_core_stops_executor_core),
	TEST_NO_TAG("Executor Shared Core get message from callId", shared_executor_core_get_message),
	TEST_NO_TAG("Executor Shared Core get chat room from chat room address", shared_executor_core_get_chat_room),
	TEST_NO_TAG("Two Executor Shared Cores get messages", two_shared_executor_cores_get_messages),
	TEST_NO_TAG("Two Executor Shared Cores get one msg and one chat room", two_shared_executor_cores_get_message_and_chat_room)
	// group chat msg tester?
};

test_suite_t shared_core_test_suite = {
	"Shared Core",
	NULL,
	NULL,
	liblinphone_tester_before_each,
	liblinphone_tester_after_each,
	sizeof(shared_core_tests) / sizeof(shared_core_tests[0]), shared_core_tests
};
