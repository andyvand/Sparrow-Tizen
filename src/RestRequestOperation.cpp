/*
 * RestRequestOperation.cpp
 *
 *  Created on: Nov 6, 2013
 *      Author: developer
 */

#include "RestRequestOperation.h"
#include "RestClient.h"
#include "RestResponse.h"
#include "dispatch/dispatch.h"

#include "Error.h"

#include <FNet.h>
#include <FWeb.h>

using namespace Tizen::Base;
using namespace Tizen::Base::Utility;
using namespace Tizen::Net::Http;
using namespace Tizen::Web::Json;

RestRequestOperation::RestRequestOperation(String *_uri, long operationCode, String *method, HashMap *params) {
	Init(_uri, operationCode, method, params);
}

RestRequestOperation::RestRequestOperation(long operationCode, String *method, HashMap *params) {
	Init(new String(L"https://api.vk.com/method/"), operationCode, method, params);
}

void
RestRequestOperation::Init(String *_uri, long operationCode, String *method, HashMap *params) {
	__method = method;
	__operationCode = operationCode;

	String uri = _uri->GetPointer();

	if (method) {
		uri.Append(method->GetPointer());
	}

	uri.Append(L"?");

	IMapEnumerator* pMapEnum = params->GetMapEnumeratorN();
	String* pKey = null;
	String* pValue = null;
	int index = 0;
	while (pMapEnum->MoveNext() == E_SUCCESS)
	{
		if (index != 0) {
			uri.Append(L"&");
		}

		pKey = static_cast< String* > (pMapEnum->GetKey());
		pValue = static_cast< String* > (pMapEnum->GetValue());

		uri.Append(pKey->GetPointer());
		uri.Append(L"=");
		String encodedValue;
		UrlEncoder::Encode(pValue->GetPointer(), L"UTF-8", encodedValue);
		uri.Append(encodedValue.GetPointer());
		index++;
	}

	uri.Append(L"&v=5.3");

	//AppLogDebug("uri = %S", uri.GetPointer());

	delete pMapEnum;
	delete params;

	HttpHeader* pHeader = null;

	__pHttpTransaction = RestClient::getInstance().GetActiveSession()->OpenTransactionN();

	__pHttpTransaction->AddHttpTransactionListener(*this);

	HttpRequest* pHttpRequest = __pHttpTransaction->GetRequest();

	pHttpRequest->SetMethod(NET_HTTP_METHOD_GET);

	pHttpRequest->SetUri(uri);
	pHeader = pHttpRequest->GetHeader();
	pHeader->AddField(L"Accept", L"application/json");

	__restRequestListener = null;
	__responseDescriptor = null;
	__pRequestOwner = null;
	this->__pByteBuffer = null;
	__isComplited = false;
	__isError = false;
}

RestRequestOperation::~RestRequestOperation() {
//	AppLogDebug("RestRequestOperation::~RestRequestOperation");
	delete __method;
	__method = null;
	delete __pHttpTransaction;
	__pHttpTransaction = null;
	delete __pByteBuffer;
}

void RestRequestOperation::perform() {
	if (__pHttpTransaction != null) {
		__pHttpTransaction->Submit();
	} else {
		AppLogDebug("Ошибка при попытке выполнить HTTP запрос");
	}
}

long RestRequestOperation::GetOperationCode() {
	return __operationCode;
}

//=========================================================

void RestRequestOperation::AddEventListener(IRestRequestListener *listener) {
	__restRequestListener = listener;
}

void RestRequestOperation::SetRequestOwner(IRestRequestOwner *owner) {
	__pRequestOwner = owner;
}

//=========================================================

void
RestRequestOperation::OnTransactionReadyToRead(HttpSession& httpSession, HttpTransaction& httpTransaction, int availableBodyLen)
{


	HttpResponse* pHttpResponse = httpTransaction.GetResponse();

	//AppLog("RestRequestOperation::OnTransactionReadyToRead %d", pHttpResponse->GetHttpStatusCode());

	if (pHttpResponse->GetHttpStatusCode() == HTTP_STATUS_OK)
	{
		HttpHeader* pHttpHeader = pHttpResponse->GetHeader();
		if (pHttpHeader != null)
		{
			String* tempHeaderString = pHttpHeader->GetRawHeaderN();
			ByteBuffer* pBuffer = pHttpResponse->ReadBodyN();

			if (__pByteBuffer == null) {
				__pByteBuffer = new ByteBuffer();
				__pByteBuffer->Construct(availableBodyLen);
			} else {
				int newCapacity = __pByteBuffer->GetCapacity() + availableBodyLen;
				__pByteBuffer->ExpandCapacity(newCapacity);
			}

			__pByteBuffer->CopyFrom(*pBuffer);

			delete pBuffer;
			delete tempHeaderString;
		}
	} else {
		__isError = true;
	}

}

void
RestRequestOperation::OnTransactionAborted(HttpSession& httpSession, HttpTransaction& httpTransaction, result r)
{
	AppLog("RestRequestOperation::OnTransactionAborted(%s)", GetErrorMessage(r));
	__pRequestOwner->OnCompliteN(this);
}

void
RestRequestOperation::OnTransactionReadyToWrite(HttpSession& httpSession, HttpTransaction& httpTransaction, int recommendedChunkSize)
{
	//AppLog("RestRequestOperation::OnTransactionReadyToWrite");
}

void
RestRequestOperation::OnTransactionHeaderCompleted(HttpSession& httpSession, HttpTransaction& httpTransaction, int headerLen, bool authRequired)
{
	//AppLog("RestRequestOperation::OnTransactionHeaderCompleted");
}

void
RestRequestOperation::OnTransactionCompleted(HttpSession& httpSession, HttpTransaction& httpTransaction)
{
	//AppLog("RestRequestOperation::OnTransactionCompleted");

	dispatch_async(dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		Execute();
		__pRequestOwner->OnCompliteN(this);
	});

}

void
RestRequestOperation::OnTransactionCertVerificationRequiredN(HttpSession& httpSession, HttpTransaction& httpTransaction, Tizen::Base::String* pCert)
{
	AppLog("RestRequestOperation::OnTransactionCertVerificationRequiredN");

	httpTransaction.Resume();
	delete pCert;
}

void RestRequestOperation::SetResponseDescriptor(ResponseDescriptor *responseDescriptor) {
	__responseDescriptor = responseDescriptor;
}

bool RestRequestOperation::GetIsComplited() {
	return __isComplited;
}

void
RestRequestOperation::Execute() {
	if (this->__isError || !this->__pByteBuffer) {
		AppLogDebug("ERROR");

		if (__pByteBuffer) {
			String *text = new String ((const char*)(this->__pByteBuffer->GetPointer()));

			AppLogDebug("body: %S", text->GetPointer());

			delete text;
		}

		__restRequestListener->OnErrorN(new Error(REST_BAD_RESPONSE));
		return;
	}

	String *text = new String ((const char*)(this->__pByteBuffer->GetPointer()));

	AppLogDebug("body: %S", text->GetPointer());

	delete text;

	IJsonValue* pJson = JsonParser::ParseN(*this->__pByteBuffer);
	JsonObject* pObject = static_cast< JsonObject* >(pJson);

	RestResponse *response = null;

	if (__responseDescriptor) {
		response = __responseDescriptor->performObjectMappingN(pObject);
		response->SetOperationCode(__operationCode);
	} else {
		AppLogDebug("Вы не предоставили дескриптор для запроса!");
	}

	if (__restRequestListener) {
		if (response) {
			if (response->GetError()) {
				__restRequestListener->OnErrorN(response->GetError());
			} else {
				__restRequestListener->OnSuccessN(response);
			}
		} else {
			__restRequestListener->OnErrorN(new Error());
		}
	}

	delete pJson;
}
