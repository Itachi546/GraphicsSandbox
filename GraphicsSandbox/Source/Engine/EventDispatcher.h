#pragma once

#include "Platform.h"
#include <string>
#include <functional>

#define BIND_EVENT_FN(fn) [this](const Event& evt) -> bool { return this->fn(evt); }

enum class EventType
{
	WindowResize,
	CubemapChanged
};

class Event
{
public:
	Event(EventType eventType) : mEventType(eventType) {}

	EventType GetEventType() const { return mEventType; }

	virtual std::string ToString() const { return "DefaultEvent"; };
protected:
	EventType mEventType;
};

class WindowResizeEvent : public Event
{
public:
	WindowResizeEvent(int width, int height) : Event(EventType::WindowResize), mWidth(width), mHeight(height)
	{
	}

	std::string ToString() const override
	{
		return "WindowResizeEvent";
	}

	int getWidth()  const  { return mWidth; }
	int getHeight() const  { return mHeight; }

protected:
	int mWidth;
	int mHeight;
};
/*
* For now we have simple eventdispatcher with a functionality to dispatch event
* and subscribe for it. We can also have a subscriber that only subscribe for event 
* once
*/
namespace EventDispatcher
{

	void  DispatchEvent(const Event& e);

	void  Subscribe(EventType eventType, std::function<bool(const Event&)> handler);

}