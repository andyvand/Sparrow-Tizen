/*
 * SettingsForm.cpp
 *
 *  Created on: Nov 4, 2013
 *      Author: developer
 */

#include "SettingsForm.h"
#include <FGraphics.h>
#include <FMedia.h>
#include <FApp.h>
#include <FBase.h>
#include <FNet.h>
#include <FWeb.h>
#include "SceneRegister.h"
#include "Util.h"

#include "UiUpdateConstants.h"

#include "MUser.h"
#include "RestRequestOperation.h"
#include "UserDescriptor.h"
#include "UserRestResponse.h"
#include "RestClient.h"
#include "AuthManager.h"

#include "MUserDao.h"
#include "ImageCache.h"
#include "Resources.h"
#include "AppResourceId.h"

#include "ImageAttachmentOperation.h"

using namespace Tizen::App;
using namespace Tizen::Base;
using namespace Tizen::Base::Utility;
using namespace Tizen::Base::Collection;
using namespace Tizen::Ui;
using namespace Tizen::Io;
using namespace Tizen::Ui::Controls;
using namespace Tizen::Ui::Scenes;
using namespace Tizen::Graphics;
using namespace Tizen::Media;
using namespace Tizen::Net::Http;
using namespace Tizen::Web::Json;

const int imageOffset = 100;
const int imageSize = 300;
const int imageBottomOffset = 100;

SettingsForm::SettingsForm() {
	Form::Construct(FORM_STYLE_HEADER);
	SetFormBackEventListener(this);

	Color *pFormBackgroundColor = new (std::nothrow) Color(0, 0, 0, 255);
	this->SetBackgroundColor(*pFormBackgroundColor);

	Color *pHeaderBackgroundColor = new (std::nothrow) Color(65, 97, 137, 255);
	Color *pHeaderTextColor = new (std::nothrow) Color(255, 255, 255, 255);

	Header* pHeader = this->GetHeader();

	pHeader->SetStyle(HEADER_STYLE_TITLE);
	pHeader->SetColor(*pHeaderBackgroundColor);
	pHeader->SetTitleTextColor(*pHeaderTextColor);

	delete pHeaderBackgroundColor;
	delete pHeaderTextColor;
	delete pFormBackgroundColor;
	__user = null;
	__userRequestOperation = null;

	__bitmap = null;




	Rectangle rect = GetBounds();

	__pSectionTableView = new SectionTableView();
	__pSectionTableView->Construct(Rectangle(0, 250, rect.width, rect.height - 250), true, TABLE_VIEW_SCROLL_BAR_STYLE_NONE);
	__pSectionTableView->SetItemProviderF(this);
	__pSectionTableView->AddSectionTableViewItemEventListener(*this);
	//AddControl(__pSectionTableView);

	Color *textNormalColor = new (std::nothrow) Color(255, 255, 255, 255);
	Color *textSelectedColor = new (std::nothrow) Color(0, 0, 0, 255);
	Color *buttonNormalColor = new (std::nothrow) Color(52, 87, 119, 255);
	Color *buttonSelectedColor = new (std::nothrow) Color(255, 255, 255, 255);

	rect = this->GetClientAreaBounds();

	int buttonWidth = 0;

	buttonWidth = rect.width;

	__pExitButton = new (std::nothrow) Button();
	__pExitButton->Construct(Rectangle(rect.width/2 - buttonWidth/2, rect.height - 100, buttonWidth, 100));
	__pExitButton->SetText(L"Выход");
	__pExitButton->SetActionId(2);
	__pExitButton->AddActionEventListener(*this);
	__pExitButton->SetTextColor(*textNormalColor);
	__pExitButton->SetHighlightedTextColor(*textSelectedColor);
	__pExitButton->SetColor(BUTTON_STATUS_NORMAL, *buttonNormalColor);
	__pExitButton->SetColor(BUTTON_STATUS_PRESSED, *buttonSelectedColor);

	AddControl(__pExitButton);

	Image image;
	image.Construct();
	String filepath = App::GetInstance()->GetAppResourcePath() + L"empty_bg.png";
	Bitmap *pBackgroundBitmap = image.DecodeN(filepath, BITMAP_PIXEL_FORMAT_ARGB8888);


	Color *labelTextColor = new (std::nothrow) Color(250, 250, 250, 255);

	buttonWidth = (rect.width * 82)/100;

	pInfoLabel = new (std::nothrow) TextBox();
	pInfoLabel->Construct(Rectangle(rect.width/2 - buttonWidth/2, imageOffset * 2 + imageSize + 50, buttonWidth, 160), TEXT_BOX_BORDER_NONE);

	pInfoLabel->SetTextAlignment(ALIGNMENT_CENTER);
	pInfoLabel->SetTextColor(TEXT_BOX_TEXT_COLOR_NORMAL, *labelTextColor);
	pInfoLabel->SetTextColor(TEXT_BOX_TEXT_COLOR_DISABLED, *labelTextColor);
	pInfoLabel->SetTextSize(50);
	pInfoLabel->SetFocusable(false);
	pInfoLabel->SetEnabled(false);
	pInfoLabel->SetDragEnabled(false);
	pInfoLabel->SetDropEnabled(false);

	pInfoLabel->SetBackgroundBitmap(TEXT_BOX_STATUS_NORMAL, *pBackgroundBitmap);
	pInfoLabel->SetBackgroundBitmap(TEXT_BOX_STATUS_HIGHLIGHTED, *pBackgroundBitmap);
	pInfoLabel->SetBackgroundBitmap(TEXT_BOX_STATUS_DISABLED, *pBackgroundBitmap);

	AddControl(pInfoLabel);



}

SettingsForm::~SettingsForm() {
	if (__user) {
		delete __user;
	}
	AppLogDebug("SettingsForm::~SettingsForm");

	this->RemoveAllControls();

	if (__userRequestOperation) {
		__userRequestOperation->AddEventListener(null);
	}

	AppLogDebug("SettingsForm::~SettingsForm");
	delete __bitmap;
}

void
SettingsForm::OnSceneActivatedN(const Tizen::Ui::Scenes::SceneId& previousSceneId,
									   const Tizen::Ui::Scenes::SceneId& currentSceneId, Tizen::Base::Collection::IList* pArgs) {
	String *uidString = AuthManager::getInstance().UserId();

	AppLog("uidString %S", uidString->GetPointer());

	int uid;
	Integer::Parse(uidString->GetPointer(), uid);

	MUser *user = MUserDao::getInstance().GetUserN(uid);
	if (user) {
		__user = user;
		UpdateInterfaceForCurrentUser();
	} else {
		SendRequest();
	}
}
void
SettingsForm::OnSceneDeactivated(const Tizen::Ui::Scenes::SceneId& currentSceneId,
								const Tizen::Ui::Scenes::SceneId& nextSceneId) {

}

result
SettingsForm::OnInitializing(void) {
	return E_SUCCESS;
}
result
SettingsForm::OnTerminating(void) {
	AppLogDebug("SettingsForm::~OnTerminating");
	ImageCache::getInstance().CancelLoadingForTarget(this);
	return E_SUCCESS;
}

void
SettingsForm::OnFormBackRequested(Tizen::Ui::Controls::Form& source)
{

	SceneManager* pSceneManager = SceneManager::GetInstance();
	AppAssert(pSceneManager);
	pSceneManager->GoBackward(BackwardSceneTransition(SCENE_TRANSITION_ANIMATION_TYPE_RIGHT));
}

void
SettingsForm::SendRequest() {

	HashMap *params = new HashMap();
	params->Construct();
	params->Add(new String(L"user_ids"), AuthManager::getInstance().UserId());
	params->Add(new String(L"fields"), new String(USER_FILEDS));

	if (!__userRequestOperation) {
		__userRequestOperation = new RestRequestOperation(GET_USER, new String(L"users.get"), params);
		__userRequestOperation->AddEventListener(this);
		__userRequestOperation->SetResponseDescriptor(new UserDescriptor());
		//RestClient::getInstance().PerformOperation(__userRequestOperation);
	}
}

void
SettingsForm::OnSuccessN(RestResponse *response) {

	if (response->GetOperationCode() == GET_USER) {

		if(__userRequestOperation) {
			__userRequestOperation->AddEventListener(null);
			__userRequestOperation = null;
		}

		AppLogDebug("OnSuccessNOnSuccessNOnSuccessNOnSuccessNOnSuccessN");

		UserRestResponse *userResponse = static_cast<UserRestResponse *>(response);

		MUser *user = userResponse->GetUser();

		if (user != null && user->GetFirstName() != null) {
			__user = user;
		}
	}

	this->SendUserEvent(response->GetOperationCode(), 0);
	Tizen::App::App::GetInstance()->SendUserEvent(response->GetOperationCode(), 0);
}

void SettingsForm::OnUserEventReceivedN(RequestId requestId, Tizen::Base::Collection::IList *pArgs) {

	if (requestId == GET_USER) {
		MUser *user = MUserDao::getInstance().GetUserN(__user->GetUid());
		__user = user;

		UpdateInterfaceForCurrentUser();
	}

	this->RequestRedraw();
	delete pArgs;
}

void SettingsForm::OnErrorN(Error *error) {
	if (__userRequestOperation) {
		__userRequestOperation->AddEventListener(null);
		__userRequestOperation = null;
	}

	delete error;
}

void SettingsForm::UpdateInterfaceForCurrentUser() {
	if (!__user) {
		return;
	}

	String title(L"");
	title.Append(__user->GetFirstName()->GetPointer());
	title.Append(L" ");
	title.Append(__user->GetLastName()->GetPointer());

	if (__user->__pBigPhoto) {
		ImageCache::getInstance().LoadImageForTarget(__user->__pBigPhoto, this);
	} else if (__user->GetPhoto()) {
		ImageCache::getInstance().LoadImageForTarget(__user->GetPhoto(), this);
	}

	pInfoLabel->SetText(title);
	Draw();
}


void SettingsForm::OnImageLoadedN(Bitmap *result, Integer *code) {
	__bitmap = result;
	this->SendUserEvent(0, 0);
	Tizen::App::App::GetInstance()->SendUserEvent(0, 0);
}


result SettingsForm::OnDraw() {
	result r = E_SUCCESS;

	Canvas* pCanvas = GetCanvasN();

	if (pCanvas != null && __bitmap != null)
	{
		Rectangle bounds = GetBounds();

		Rectangle rect = Rectangle(bounds.width/2 - imageSize/2, imageOffset * 2, imageSize, imageSize);
		r = pCanvas->DrawBitmap(rect, *__bitmap);
	} else {

		Rectangle bounds = GetBounds();

		Rectangle rect = Rectangle(bounds.width/2 - imageSize/2, imageOffset * 2, imageSize, imageSize);
		r = pCanvas->DrawBitmap(rect, *Resources::getInstance().GetNoUserAvatar());

	}


	String importantString;
	Application::GetInstance()->GetAppResource()->GetString(IDS_LOGOUT, importantString);
	__pExitButton->SetText(importantString);

	String titleString;
	Application::GetInstance()->GetAppResource()->GetString(IDS_MAIN_FORM_SETTINGS, titleString);
	this->GetHeader()->SetTitleText(titleString);

	delete pCanvas;

	return r;
}

// *********************** TABLE ***************************/

int
SettingsForm::GetSectionCount(void)
{
	return 3;
}

int
SettingsForm::GetItemCount(int sectionIndex)
{
	int itemCount = 0;

	switch (sectionIndex)
	{
	case 0:
		itemCount =3;
		break;
	case 1:
		itemCount = 1;
		break;
	case 2:
		itemCount = 1;
		break;
	default :
		break;
	}

	return itemCount;
}

TableViewItem*
SettingsForm::CreateItem(int sectionIndex, int itemIndex, float itemWidth)
{
    TableViewAnnexStyle style = TABLE_VIEW_ANNEX_STYLE_NORMAL;
    TableViewItem* pItem = new TableViewItem();
    String text = L"";
    Label* pLabel = new Label();
    pLabel->Construct(FloatRectangle(0.0f, 0.0f, itemWidth, GetDefaultItemHeight()), text);
    pItem->Construct(FloatDimension(itemWidth, GetDefaultItemHeight()), style);

    pLabel->SetText(L"test");
	pLabel->SetTextHorizontalAlignment(ALIGNMENT_LEFT);
	pItem->AddControl(pLabel);

    return pItem;
}

void
SettingsForm::UpdateItem(int sectionIndex, int itemIndex, TableViewItem* pItem)
{

}

float
SettingsForm::GetDefaultItemHeight(void)
{
	return 110.0f;
}

bool
SettingsForm::DeleteItem(int sectionIndex, int itemIndex, TableViewItem* pItem)
{
	delete pItem;

	return true;
}

Tizen::Base::String
SettingsForm::GetSectionHeader(int sectionIndex)
{
	String headerText;

	switch (sectionIndex)
	{
	case 0:
		headerText = L"Connectivity";
		break;
	case 1:
		headerText = L"Share and transfer";
		break;
	case 2:
		headerText = L"Mode";
		break;
	default :
		break;
	}

	return headerText;
}

bool
SettingsForm::HasSectionHeader(int sectionIndex)
{
	return true;
}

Tizen::Base::String
SettingsForm::GetSectionFooter(int sectionIndex)
{
	return L"";
}

bool
SettingsForm::HasSectionFooter(int sectionIndex)
{
	return false;
}

void
SettingsForm::OnSectionTableViewItemStateChanged(Tizen::Ui::Controls::SectionTableView& tableView, int sectionIndex, int itemIndex, Tizen::Ui::Controls::TableViewItem* pItem, Tizen::Ui::Controls::TableViewItemStatus status)
{
}

void
SettingsForm::OnSectionTableViewContextItemActivationStateChanged(Tizen::Ui::Controls::SectionTableView& tableView, int sectionIndex, int itemIndex, Tizen::Ui::Controls::TableViewContextItem* pContextItem, bool activated)
{
}

void
SettingsForm::OnActionPerformed(const Tizen::Ui::Control& source, int actionId)
{
	AuthManager::getInstance().SetForced(true);
	Tizen::App::UiApp::GetInstance()->SendUserEvent(LOGOUT, 0);
}
