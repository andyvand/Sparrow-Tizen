/*
 * MMessage.h
 *
 *  Created on: Nov 16, 2013
 *      Author: developer
 */

#ifndef MMESSAGE_H_
#define MMESSAGE_H_

#include <FBase.h>
#include "Bean.h"

using namespace Tizen::Base;

class MMessage
 : public Bean
{
public:
	MMessage();
	virtual ~MMessage();

private:
	//message info part
	int __mid;
	int __uid;
	int __fromUid;
	long __date;
	int __out;
	int __readState;
	String *__text;

public:
	static String* TableDescription();

	int GetMid();
	int GetUid();
	int GetFromUid();
	long GetDate();
	int GetOut();
	int GetReadState();
	String *GetText();

	void SetMid(int id);
	void SetUid(int id);
	void SetFromUid(int id);
	void SetDate(long date);
	void SetOut(int out);
	void SetReadState(int state);
	void SetText(String *text);

	static MMessage * CreateFromJsonN(const Tizen::Web::Json::JsonObject &pUserObject, const Tizen::Web::Json::JsonObject &pMessageObject);
};

#endif /* MMESSAGE_H_ */
