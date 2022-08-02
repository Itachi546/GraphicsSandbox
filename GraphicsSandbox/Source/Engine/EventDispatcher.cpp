#include "EventDispatcher.h"
#include "Logger.h"

#include <unordered_map>
#include <vector>

namespace EventDispatcher
{
	using EventHandler = std::function<bool(const Event&)>;
	std::unordered_map<EventType, std::vector<EventHandler>> eventHandlers;

	void DispatchEvent(const Event& e)
	{
		auto found = eventHandlers.find(e.GetEventType());
		if (found != eventHandlers.end())
		{
			auto& handlers = found->second;
			for (auto& handler : handlers)
				handler(e);
		}
	}

	void Subscribe(EventType eventType, std::function<bool(const Event&)> handler)
	{
		eventHandlers[eventType].push_back(handler);
	}

}