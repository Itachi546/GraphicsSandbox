#pragma once

#include <vector>
#include <array>
#include <memory>
#include <unordered_map>

#include <stdint.h>
#include <cassert>


namespace ecs
{
	constexpr uint32_t INVALID_ENTITY = 0;
	using ComponentType = std::uint8_t;
	const uint8_t MAX_COMPONENTS = 64;

	using Entity = uint32_t;

	inline bool IsValid(Entity entity) {
		return entity != INVALID_ENTITY;
	}
	/*
	* This doesn't seem to work when the dll is made and
	* called from exe?
	*/
	static uint32_t GetId()
	{
		static uint32_t gComponentId = 0;
		return gComponentId++;
	}

	class IComponentArray
	{
	public:
		virtual ~IComponentArray() = default;

		virtual bool removeEntity(Entity& handle) { return false; }
	};

	template<typename T>
	class ComponentArray : public IComponentArray
	{
	public:
		ComponentArray() {}

		// Remove copy constructor and copy-assignment constructor
		ComponentArray(const ComponentArray&) = delete; 
		void operator=(const ComponentArray&) = delete;

		T& addComponent(Entity entity) {
			assert(components.size() == entities.size());
			assert(entities.size() == lookup_.size());
			assert(IsValid(entity));

			auto found = lookup_.find(entity);
			if (found != lookup_.end())
				return components[found->second];

			lookup_[entity] = components.size();
			components.push_back(T());
			entities.push_back(entity);

			return components.back();
		}

		template<typename ...Args>
		T& addComponent(Entity entity, Args&& ...args)
		{
			assert(components.size() == entities.size());
			assert(entities.size() == lookup_.size());

			auto found = lookup_.find(entity);
			if (found != lookup_.end())
				return components[found->second];

			lookup_[entity] = components.size();
			components.push_back(T(std::forward<Args>(args)...));
			entities.push_back(entity);
			return components.back();
		}

		bool removeComponent(Entity entity) {
			auto found = lookup_.find(entity);
			if (found != lookup_.end())
			{
				uint64_t index = found->second;
				components[index] = std::move(components.back());
				entities[index] = entities.back();
				lookup_[entities[index]] = index;
				lookup_.erase(entity);
				components.pop_back();
				entities.pop_back();
				return true;
			}
			return false;
		}

		bool removeEntity(Entity& entity) override {
			return removeComponent(entity);
		}

		T* getComponent(Entity entity) {
			auto found = lookup_.find(entity);
			if (found != lookup_.end())
				return &components[found->second];
			return nullptr;
		}

		std::size_t GetIndex(Entity entity)
		{
			auto found = lookup_.find(entity);
			if (found != lookup_.end())
				return found->second;
			return ~0ull;
		}

		std::size_t GetCount()
		{
			return components.size();
		}
		
		std::vector<Entity> entities;
		std::vector<T> components;
	private:
		std::unordered_map<uint32_t, uint64_t> lookup_;
	};

	struct ComponentManager
	{
		ComponentManager() = default;
		ComponentManager(const ComponentManager&) = delete;
		void operator=(const ComponentManager&) = delete;

		template<typename T>
		void RegisterComponent()
		{
			std::size_t compHash = typeid(T).hash_code();
			auto found = componentIdMap.find(compHash);
			if (found != componentIdMap.end())
				return;
			uint32_t id = GetId();
			componentArray[id] = std::make_shared<ComponentArray<T>>();
			componentIdMap.insert(std::make_pair(compHash, id));
		}

		template<typename T>
		uint32_t GetComponentTypeId() {
			std::size_t compHash = typeid(T).hash_code();
			auto found = componentIdMap.find(compHash);
			assert(found != componentIdMap.end());
			return found->second;
		}

		template<typename T>
		inline std::shared_ptr<ComponentArray<T>> GetComponentArray()
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);
			return std::static_pointer_cast<ComponentArray<T>>(componentArray[compId]);
		}

		template<typename T>
		inline std::shared_ptr<ComponentArray<T>> GetComponentArray(int index)
		{
			return std::static_pointer_cast<ComponentArray<T>>(componentArray[index]);
		}

		inline std::shared_ptr<IComponentArray> GetBaseComponentArray(uint64_t index)
		{
			assert(index < MAX_COMPONENTS);

			return componentArray[index];
		}


		template<typename T>
		bool HasComponent(const Entity& entity) {
			uint32_t compId = GetComponentTypeId<T>();
			auto comp = GetComponentArray<T>(compId);
			return comp->GetIndex(entity) != ~0ull;
		}

		/*
		* We should be careful when keeping this reference for future use.
		* When the size of vector is not enough, the container is resized
		* which invalidate all the reference and the pointer.
		*/
		template<typename T>
		T& AddComponent(Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);
			auto comp = GetComponentArray<T>(compId);
			assert(comp != nullptr);
			return comp->addComponent(entity);
		}

		template<typename T, typename ...Args>
		T& AddComponent(Entity& entity, Args&& ...args)
		{
			uint32_t compId = GetComponentTypeId<T>(compId);
			assert(compId < MAX_COMPONENTS);
			auto comp = GetComponentArray<T>();
			assert(comp != nullptr);
			return comp->addComponent(entity, std::forward<Args>(args)...);
		}

		template<typename T>
		T* GetComponent(const Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			auto comp = GetComponentArray<T>(compId);
			assert(comp != nullptr);
			return comp->getComponent(entity);
		}


		template<typename T>
		bool RemoveComponent(Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);
			auto comp = GetComponentArray<T>(compId);
			assert(comp != nullptr);
			return comp->removeComponent(entity);
		}

		// Global Component Array
		std::array<std::shared_ptr<IComponentArray>, MAX_COMPONENTS> componentArray;
		std::unordered_map<std::size_t, uint32_t> componentIdMap;
	};

	inline Entity CreateEntity()
	{
		static uint32_t id = 0;
		return ++id;
	}

	void DestroyEntity(ComponentManager* mgr, Entity& entity);
}
