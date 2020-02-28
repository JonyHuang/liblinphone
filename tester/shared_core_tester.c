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

	linphone_core_manager_destroy(executor_mgr); // TODO PAUL : fait demarrer le core si on le met en dernier -> pas bon
	linphone_core_manager_destroy(main_mgr);
// #endif
}

static void shared_main_core_stops_executor_core (void) {
// 	LinphoneCoreManager* main_mgr;
// 	LinphoneCoreManager* executor_mgr;
// 	main_mgr = linphone_core_manager_new_shared("marie_rc", TEST_GROUP_ID, TRUE);
// 	executor_mgr = linphone_core_manager_new_shared("pauline_rc", TEST_GROUP_ID, FALSE);

// 	linphone_core_manager_start(executor_mgr, TRUE); //TODO PAUL : mettre dans le create?
// 	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, NULL, &executor_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));

// 	linphone_core_manager_start(main_mgr, TRUE);
// 	 BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, main_mgr->lc, &executor_mgr->stat.number_of_LinphoneGlobalOff, 1, 2000));
// //	BC_ASSERT_TRUE(wait_for_until(executor_mgr->lc, main_mgr->lc, &linphone_core_get_global_state(executor_mgr->lc), LinphoneGlobalOff, 2000));
// 	// BC_ASSERT_EQUAL(linphone_core_get_global_state(executor_mgr->lc), LinphoneGlobalOff, int, "%i");
// 	BC_ASSERT_TRUE(wait_for_until(main_mgr->lc, executor_mgr->lc, &main_mgr->stat.number_of_LinphoneGlobalOn, 1, 2000));


// 	linphone_core_manager_destroy(main_mgr);
// 	linphone_core_manager_destroy(executor_mgr);
}




void test(LinphoneCoreManager* receiver, LinphoneCoreManager* sender) {
	const char* text = "Bli bli bli \n blu";
	const char* content_type = "text/plain";
	LinphoneChatRoom *room = linphone_core_get_chat_room(sender->lc, receiver->identity);
	BC_ASSERT_TRUE(linphone_chat_room_is_empty(room));

	LinphoneChatMessage* msg = linphone_chat_room_create_message(room, text);
	linphone_chat_message_set_content_type(msg, content_type);
	LinphoneChatMessageCbs *cbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(cbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);

	BC_ASSERT_TRUE(wait_for(sender->lc,receiver->lc,&sender->stat.number_of_LinphoneMessageDelivered,1));
	BC_ASSERT_TRUE(wait_for(sender->lc,receiver->lc,&receiver->stat.number_of_LinphoneMessageReceived,1));

	BC_ASSERT_PTR_NOT_NULL(receiver->stat.last_received_chat_message);
	if (receiver->stat.last_received_chat_message != NULL) {
		const char * call_id = linphone_chat_message_get_custom_header(receiver->stat.last_received_chat_message, "Call-ID");

		linphone_core_destroy(receiver->lc);
		linphone_core_manager_configure(receiver);

		LinphoneChatMessage *received_msg = linphone_core_get_push_notification_message(receiver->lc, call_id);
		BC_ASSERT_PTR_NOT_NULL(received_msg);

		if (received_msg != NULL) {
			LinphoneContent *content = (LinphoneContent *)(linphone_chat_message_get_contents(received_msg)->data);
			char* content_type_header = ms_strdup_printf("Content-Type: %s",content_type);
			belle_sip_header_content_type_t *belle_sip_content_type = belle_sip_header_content_type_parse(content_type_header);
			BC_ASSERT_STRING_EQUAL(linphone_content_get_type(content), belle_sip_header_content_type_get_type(belle_sip_content_type));
			BC_ASSERT_STRING_EQUAL(linphone_content_get_subtype(content), belle_sip_header_content_type_get_subtype(belle_sip_content_type));
			BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(received_msg), text);
			ms_free(content_type_header);
			linphone_chat_message_unref(received_msg);
		}
	}

	BC_ASSERT_PTR_NOT_NULL(linphone_core_get_chat_room(receiver->lc,sender->identity));
	linphone_chat_message_unref(msg);
}









static void shared_executor_core_get_message (void) {
	LinphoneCoreManager* sender_mgr;
	LinphoneCoreManager* receiver_mgr;
	sender_mgr = linphone_core_manager_new("marie_rc");
	receiver_mgr = linphone_core_manager_new_shared("pauline_rc", TEST_GROUP_ID, FALSE);
	linphone_core_manager_start(receiver_mgr, TRUE);

	BC_ASSERT_TRUE(wait_for_until(sender_mgr->lc, NULL, &sender_mgr->stat.number_of_LinphoneRegistrationOk, 1, 2000));
	BC_ASSERT_TRUE(wait_for_until(receiver_mgr->lc, NULL, &receiver_mgr->stat.number_of_LinphoneRegistrationOk, 1, 2000));

	test(receiver_mgr, sender_mgr);

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
