/*
 * LongPollConnection.cpp
 *
 *  Created on: Nov 16, 2013
 *      Author: developer
 */

#include "LongPollConnection.h"

#include <FBase.h>
#include <FApp.h>
#include <FShell.h>

#include "RestRequestOperation.h"

#include "AuthManager.h"
#include "RestResponse.h"
#include "LongPollResponse.h"
#include "LongPollServerDataResponse.h"
#include "LongPollDescriptor.h"
#include "LongPollServerDataDescriptor.h"
#include "LongPollObject.h"

#include "Error.h"

#include "RestClient.h"

#include "UiMessagesPanel.h"
#include "UiChatForm.h"
#include "MainForm.h"
#include "UiUpdateConstants.h"

#include "MUserDao.h"
#include "MDialogDao.h"
#include "MMessageDao.h"
#include <FShell.h>
#include "PostMan.h"
#include "MUser.h"
#include "MMessage.h"

#include "ImageCache.h"

using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::Base::Collection;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Shell;

LongPollConnection::LongPollConnection() {
	__pLongPollServerDataOperation = null;
	__pLongPollConnectionOperation = null;
	this->__pKey = null;
	this->__pServer = null;
	this->__pTS = null;
	__IsRunning = false;
	__pendingRestart = false;
	__pPendingTimer = null;

	__pNotificationManager = new Tizen::Shell::NotificationManager;
	AppAssert(__pNotificationManager);
	__pNotificationManager->Construct();
	__mutex.Create();

}

LongPollConnection::~LongPollConnection() {
	__pLongPollServerDataOperation = null;
	__pLongPollConnectionOperation = null;

	if (__pPendingTimer) {
		this->CancelTimer();
	}
}

void
LongPollConnection::Run() {
	if (!__IsRunning && AuthManager::getInstance().IsAuthorized()) {
		__IsRunning = true;
		GetLongPollServerData();
	}
}

void
LongPollConnection::Stop() {
	__IsRunning = false;
}

void
LongPollConnection::GetLongPollServerData() {
	HashMap *params = new HashMap();
	params->Construct();
	params->Add(new String(L"access_token"), AuthManager::getInstance().AccessToken());

	if (!__pLongPollServerDataOperation) {
		__pendingRestart = false;
		__pLongPollServerDataOperation = new RestRequestOperation(LONGPOLL_GET_SERVER, new String(L"messages.getLongPollServer"), params);
		__pLongPollServerDataOperation->AddEventListener(this);
		__pLongPollServerDataOperation->SetResponseDescriptor(new LongPollServerDataDescriptor());
		RestClient::getInstance().PerformOperation(__pLongPollServerDataOperation);
	}
}

void
LongPollConnection::Reconnect() {
	if (this->__pKey && this->__pServer  && this->__pTS) {
		this->SendRequestToLongPollServer(__pKey, __pServer, __pTS);
	} else {
		this->GetLongPollServerData();
	}
}

void
LongPollConnection::SendRequestToLongPollServer(IList *pArgs) {

	AppAssert(pArgs->GetCount() == 3);
	String *key = static_cast<String *>(pArgs->GetAt(0));
	String *server = static_cast<String *>(pArgs->GetAt(1));
	String *ts = static_cast<String *>(pArgs->GetAt(2));

	this->__pKey = key;
	this->__pServer = server;
	this->__pTS = ts;
	AppLog("SendRequestToLongPollServer");
	this->SendRequestToLongPollServer(key, server, ts);
}

void
LongPollConnection::SendRequestToLongPollServer(String *key, String *server, String *ts) {
	HashMap *params = new HashMap();
	params->Construct();
	params->Add(new String(L"access_token"), AuthManager::getInstance().AccessToken());
	params->Add(new String(L"key"), key);
	params->Add(new String(L"act"), new String(L"a_check"));
	params->Add(new String(L"ts"), ts);
	params->Add(new String(L"wait"), new String(L"25"));
	params->Add(new String(L"mode"), new String(L"9"));

	String uri(L"http://");
	uri.Append(server->GetPointer());

	if (!__pLongPollConnectionOperation) {
		__pendingRestart = false;
		__pLongPollConnectionOperation = new RestRequestOperation(new String(uri), LONGPOLL_CONNECTION, null, params);
		__pLongPollConnectionOperation->AddEventListener(this);
		__pLongPollConnectionOperation->SetResponseDescriptor(new LongPollDescriptor());
		RestClient::getInstance().PerformOperation(__pLongPollConnectionOperation);
	}
}

/******************* REST REQUEST LISTENER ******************/

void
LongPollConnection::OnSuccessN(RestResponse *result) {

	if (result->GetOperationCode() == LONGPOLL_GET_SERVER) {
		__pLongPollServerDataOperation->AddEventListener(null);
		__pLongPollServerDataOperation = null;
		LongPollServerDataResponse *longPollServerDataResponse = static_cast<LongPollServerDataResponse *>(result);

		LinkedList *list = new LinkedList();
		list->Add(longPollServerDataResponse->GetKey());
		list->Add(longPollServerDataResponse->GetServer());
		list->Add(longPollServerDataResponse->GetTS());

		App::GetInstance()->SendUserEvent(LONGPOLL_GET_SERVER, list);

	} else if (result->GetOperationCode() == LONGPOLL_CONNECTION) {
		__pLongPollConnectionOperation->AddEventListener(null);
		__pLongPollConnectionOperation = null;
		LongPollResponse *longPollResponse = static_cast<LongPollResponse *>(result);

		//*** test ***//

		if (longPollResponse->GetError()) {
			this->__pKey = null;
			this->__pServer = null;
			this->__pTS = null;
			this->GetLongPollServerData();
			return;
		}

		this->__pTS = longPollResponse->GetTS();

		//{"ts":1709548768,"updates":[[9,-26033241,1],[8,-183384680,0]]}

		LinkedList *testData = new LinkedList();

		LongPollObject *testObject1 = new LongPollObject();
		testObject1->SetType(LP_USER_ONLINE);
		testObject1->SetUserId(183384680);


		LongPollObject *testObject2 = new LongPollObject();
		testObject2->SetType(LP_USER_OFFLINE);
		testObject2->SetUserId(26033241);

		testData->Add(testObject2);
		testData->Add(testObject1);

		//longPollResponse->SetUpdates(testData);

		//******//


		Tizen::Ui::Controls::Frame* pFrame = Tizen::App::UiApp::GetInstance()->GetAppFrame()->GetFrame();
		LinkedList *pArgs;
		AppAssert(longPollResponse->GetUpdates());

		String *pLastText = null;
		int lastMid = -1;

		for (int index = 0; index < longPollResponse->GetUpdates()->GetCount(); index ++) {
			LongPollObject *pObject = static_cast<LongPollObject*>(longPollResponse->GetUpdates()->GetAt(index));

			switch(pObject->GetType()) {
				case LP_DELETE:

					break;
				case LP_FLAG_REPLACE:

						break;
				case LP_FLAG_ADD:

						break;
				case LP_FLAG_RESET: {
					MMessageDao::getInstance().SaveReaded(pObject->__messageId);
					MDialogDao::getInstance().SaveReaded(pObject->__messageId);

					AppLogDebug("SOMEONE READ A MESSAGE! %d", pObject->__messageId);
					UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
					if (pChatForm) {
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->__messageId));
						pChatForm->SendUserEvent(UPDATE_READ_STATE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_READ_STATE, 0);
						pArgs = null;
					}

					UiMessagesPanel* pMessagePanel = static_cast< UiMessagesPanel* >(pFrame->GetControl("UiMessagesPanel", true));
					if (pMessagePanel) {
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->__messageId));

						pMessagePanel->SendUserEvent(UPDATE_READ_STATE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_READ_STATE, 0);
						pArgs = null;
					}

					MainForm* pMainForm = static_cast< MainForm* >(pFrame->GetControl("MainForm", true));
					if (pMainForm) {
						pMainForm->SendUserEvent(UPDATE_READ_STATE, 0);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_READ_STATE, 0);
					}

				}
						break;
				case LP_MESSAGE_ADD: {
					pLastText = pObject->__pText;
					lastMid = pObject->__messageId;
				}
					break;
				case LP_MESSAGE_ADD_FULL: {

					if (lastMid != -1 && lastMid == pObject->GetMessage()->GetMid()) {
						pObject->GetMessage()->SetText(pLastText);
					}

					MMessageDao::getInstance().Save(pObject->GetMessage());
					MUserDao::getInstance().Save(pObject->GetUsers());

					MDialogDao::getInstance().Save(pObject->GetMessage(), static_cast<MUser *>(pObject->GetUsers()->GetAt(0)));

					UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
					if (pChatForm && PostMan::getInstance().ValidateIncomingMessage(pObject->GetMessage())) {
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->GetMessage()->GetUid()));
						pArgs->Add(pObject->GetMessage());
						pChatForm->SendUserEvent(UPDATE_MESSAGE_ARRIVED, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_MESSAGE_ARRIVED, 0);
						pArgs = null;
					}

					UiMessagesPanel* pMessagePanel = static_cast< UiMessagesPanel* >(pFrame->GetControl("UiMessagesPanel", true));
					if (pMessagePanel)
					{
						pMessagePanel->SendUserEvent(UPDATE_MESSAGE_ARRIVED, 0);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_MESSAGE_ARRIVED, 0);
						pArgs = null;
					}

					MainForm* pMainForm = static_cast< MainForm* >(pFrame->GetControl("MainForm", true));
					if (pMainForm) {
						pMainForm->SendUserEvent(UPDATE_READ_STATE, 0);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_READ_STATE, 0);
					}

					if ((!pChatForm ||
							Tizen::App::UiApp::GetInstance()->GetAppUiState() != APP_UI_STATE_FOREGROUND ||
							(pChatForm && pChatForm->__userId != pObject->GetMessage()->GetUid())) &&
							pObject->GetMessage()->GetOut() != 1
							) {

						MUser *pUser = null;

						for (int i = 0; i < pObject->GetUsers()->GetCount(); i++) {
							MUser *user = static_cast<MUser *>(pObject->GetUsers()->GetAt(i));
							if (user->GetUid() == pObject->GetMessage()->GetUid()) {
								pUser = user;
							}
						}

						ShowNotification(pObject->GetMessage(), pUser);

						pLastText = null;
						lastMid = -1;
					}
				}
						break;
				case LP_USER_ONLINE: {
					AppLogDebug("USER %d ONLINE", pObject->GetUserId());

					UiMessagesPanel* pMessagePanel = static_cast< UiMessagesPanel* >(pFrame->GetControl("UiMessagesPanel", true));
					if (pMessagePanel)
					{
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->GetUserId()));
						pMessagePanel->SendUserEvent(UPDATE_USER_ONLINE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_ONLINE, 0);
						pArgs = null;
					}

					UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
					if (pChatForm) {
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->GetUserId()));
						pChatForm->SendUserEvent(UPDATE_USER_ONLINE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_ONLINE, 0);
						pArgs = null;
					}

					MUserDao::getInstance().UpdateUserOnlineStatusById(1, pObject->GetUserId());
					MDialogDao::getInstance().UpdateDialogOnlineStatusById(1, pObject->GetUserId());
				}
						break;
				case LP_USER_OFFLINE: {
					AppLogDebug("USER %d GOES OFFLINE", pObject->GetUserId());

					UiMessagesPanel* pMessagePanel1 = static_cast< UiMessagesPanel* >(pFrame->GetControl("UiMessagesPanel", true));
					if (pMessagePanel1)
					{
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->GetUserId()));
						pMessagePanel1->SendUserEvent(UPDATE_USER_OFFLINE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_OFFLINE, 0);
						pArgs = null;
					}

					UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
					if (pChatForm) {
						pArgs = new LinkedList();
						pArgs->Add(new Integer(pObject->GetUserId()));
						pChatForm->SendUserEvent(UPDATE_USER_OFFLINE, pArgs);
						Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_ONLINE, 0);
						pArgs = null;
					}

					MUserDao::getInstance().UpdateUserOnlineStatusById(0, pObject->GetUserId());
					MDialogDao::getInstance().UpdateDialogOnlineStatusById(0, pObject->GetUserId());
				}
						break;
				case LP_CHAT_UPDATED:

						break;
				case LP_USER_PRINT_MSG: {
						UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
						if (pChatForm) {
							pArgs = new LinkedList();
							pArgs->Add(new Integer(pObject->GetUserId()));

							pChatForm->SendUserEvent(UPDATE_USER_PRINTING, pArgs);
							Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_PRINTING, 0);
							pArgs = null;
						}
				}
						break;
				case LP_USER_PRINT_MSG_CHAT: {
						UiChatForm* pChatForm = static_cast< UiChatForm* >(pFrame->GetControl("UiChatForm", true));
						if (pChatForm) {
							pArgs = new LinkedList();
							pArgs->Add(new Integer(pObject->GetUserId()));
							if (pObject->GetChatId() != -1) {
								pArgs->Add(new Integer(pObject->GetChatId()));
							}

							pChatForm->SendUserEvent(UPDATE_USER_PRINTING_IN_CONVERSATION, pArgs);
							Tizen::App::UiApp::GetInstance()->SendUserEvent(UPDATE_USER_PRINTING_IN_CONVERSATION, 0);
							pArgs = null;
						}
				}
						break;
				}
		}


		if (__IsRunning) {
			App::GetInstance()->SendUserEvent(LONGPOLL_CONNECTION, 0);
		}
	}
}

void
LongPollConnection::OnErrorN(Error *error) {
	if (__pLongPollServerDataOperation) {
		__pLongPollServerDataOperation->AddEventListener(null);
		__pLongPollServerDataOperation = null;
	}

	if (__pLongPollConnectionOperation) {
		__pLongPollConnectionOperation->AddEventListener(null);
		__pLongPollConnectionOperation = null;
	}

	AppLog("LongPollConnection::OnErrorN");
	if (error && error->GetCode() == LONG_POLL_REQUEST_RECONNECT) {
		if (__IsRunning) {


			App::GetInstance()->SendUserEvent(LONGPOLL_RE_REQUEST_DATA, 0);
		}
	} else {
		AppLog("!!!LongPollConnection::OnErrorN");

		__pendingRestart = true;

		App::GetInstance()->SendUserEvent(LONGPOLL_RECONNECTION, 0);

	}

	delete error;
}

bool
LongPollConnection::PendingRestart() {
	return __IsRunning && __pendingRestart;
}

void
LongPollConnection::RunTimer() {
	__mutex.Acquire();
	if (this->__pPendingTimer) {
		this->CancelTimer();
	}

	AppLog("RUN TIMER");

	this->__pPendingTimer = new Timer();
	AppLog("RUN TIMER1");
	this->__pPendingTimer->Construct(*this);
	AppLog("RUN TIMER2");
	this->__pPendingTimer->Start(10000);
	AppLog("RUN TIMER3");
	__mutex.Release();
}

void
LongPollConnection::CancelTimer() {
	this->__pPendingTimer->Cancel();
	delete this->__pPendingTimer;
	this->__pPendingTimer = null;
}

void
LongPollConnection::OnTimerExpired (Timer &timer) {
	delete this->__pPendingTimer;
	this->__pPendingTimer = null;

	if (this->PendingRestart()) {
		this->Reconnect();
		this->RunTimer();
	}
}

void
LongPollConnection::ShowNotification(MMessage *pMessage, MUser *pUser) {
	result r;
	String *title = null;

	AppLog("ShowNotification");

	if (pUser) {
		String *fullName = new String();
		fullName->Append(pUser->GetFirstName()->GetPointer());
		fullName->Append(L" ");
		fullName->Append(pUser->GetLastName()->GetPointer());
		title = fullName;
		title->Append(L" пишет...");
	}

	if (!title) {
		title = new String("New message");
	}

	NotificationRequest request;
	request.SetAlertText(pMessage->GetText()->GetPointer());
	request.SetTitleText(title->GetPointer());

	if (pUser) {
		String uid;
		uid.Format(20, L"%d", pUser->GetUid());
		request.SetAppMessage(uid);
	}

	request.SetNotificationStyle(NOTIFICATION_STYLE_NORMAL);
	//Добавить проверку на отсутствие иконки!
	if (pUser) {
		request.SetIconFilePath(ImageCache::getInstance().PathForUrl(pUser->GetPhoto()));
	}

	r = __pNotificationManager->Notify(request);
}
