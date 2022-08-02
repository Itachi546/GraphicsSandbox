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

	struct Entity
	{
		uint32_t handle = INVALID_ENTITY;
		uint64_t signature = 0;

        Entity() {}
		Entity(uint32_t handle, uint64_t signature) : handle(handle), signature(signature){}


		bool IsValid() const { return handle != INVALID_ENTITY; }
	};
	/*
	* This doesn't seem to work when the dll is made and
	* called from exe?
	*/
	static uint32_t GetId()
	{
		static uint32_t gComponentId = 0;
		return gComponentId++;
	}

	template<typename T>
	static uint32_t GetComponentTypeId()
	{
		static uint32_t id = GetId();
		return id;
	}

	class IComponentArray
	{
	public:
		virtual ~IComponentArray() = default;

		virtual bool removeEntity(const Entity& handle) { return false; }
	};

	template<typename T>
	class ComponentArray : public IComponentArray
	{
	public:
		ComponentArray() {}

		// Remove copy constructor and copy-assignment constructor
		ComponentArray(const ComponentArray&) = delete; 
		void operator=(const ComponentArray&) = delete;

		T& addComponent(const Entity& entity) {
			assert(components.size() == entities.size());
			assert(entities.size() == lookup_.size());
			assert(entity.IsValid());

			auto found = lookup_.find(entity.handle);
			if (found != lookup_.end())
				return components[found->second];

			lookup_[entity.handle] = components.size();
			components.push_back(T());
			entities.push_back(entity);

			return components.back();
		}

		template<typename ...Args>
		T& addComponent(const Entity& entity, Args&& ...args)
		{
			assert(components.size() == entities.size());
			assert(entities.size() == lookup_.size());

			auto found = lookup_.find(entity.handle);
			if (found != lookup_.end())
				return components[found->second];

			lookup_[entity.handle] = components.size();
			components.push_back(T(std::forward<Args>(args)...));
			entities.push_back(entity);
			return components.back();
		}

		bool removeComponent(const Entity& entity) {
			auto found = lookup_.find(entity.handle);
			if (found != lookup_.end())
			{
				uint64_t index = found->second;
				components[index] = std::move(components.back());
				entities[index] = entities.back();
				lookup_[entities[index].handle] = index;
				lookup_.erase(entity.handle);
				components.pop_back();
				entities.pop_back();
				return true;
			}
			return false;
		}

		bool removeEntity(const Entity& entity) override {
			return removeComponent(entity);
		}

		T* getComponent(const Entity& entity) {
			auto found = lookup_.find(entity.handle);
			if (found != lookup_.end())
				return &components[found->second];
			return nullptr;
		}

		std::size_t GetIndex(Entity entity)
		{
			auto found = lookup_.find(entity.handle);
			if (found != lookup_.end())
				return found->second;
			return ~0ull;
		}

		std::size_t GetCount()
		{
			return components.size();
		}

		static uint32_t GetTypeId()
		{
			return GetComponentTypeId<T>();
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
			uint32_t componentId = GetComponentTypeId<T>();
			assert(componentId < 64);

			if (componentArray[componentId] == nullptr)
			{
				componentArray[componentId] = std::make_shared<ComponentArray<T>>();
			}
		}

		template<typename T>
		inline std::shared_ptr<ComponentArray<T>> GetComponentArray()
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			return std::static_pointer_cast<ComponentArray<T>>(componentArray[compId]);
		}

		inline std::shared_ptr<IComponentArray> GetComponentArray(uint64_t index)
		{
			assert(index < MAX_COMPONENTS);

			return componentArray[index];
		}


		template<typename T>
		bool HasComponent(const Entity& entity) {
			uint32_t compId = GetComponentTypeId<T>();
			uint64_t sigHash = (1Ui64 << compId);
			return (entity.signature & sigHash) == sigHash;
		}

		template<typename T>
		T& AddComponent(Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			entity.signature |= (1Ui64 << compId);
			auto comp = GetComponentArray<T>();
			assert(comp != nullptr);
			return comp->addComponent(entity);
		}

		template<typename T, typename ...Args>
		T& AddComponent(Entity& entity, Args&& ...args)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			entity.signature |= (1Ui64 << compId);
			auto comp = GetComponentArray<T>();
			assert(comp != nullptr);
			return comp->addComponent(entity, std::forward<Args>(args)...);
		}

		template<typename T>
		T* GetComponent(const Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			if (HasComponent<T>(entity))
			{
				auto comp = GetComponentArray<T>();
				assert(comp != nullptr);
				return comp->getComponent(entity);
			}
			return nullptr;
		}


		template<typename T>
		bool RemoveComponent(Entity& entity)
		{
			uint32_t compId = GetComponentTypeId<T>();
			assert(compId < MAX_COMPONENTS);

			entity.signature &= ~(1Ui64 << compId);

			auto comp = GetComponentArray<T>();
			assert(comp != nullptr);
			return comp->removeComponent(entity);
		}

		// Global Component Array
		std::array<std::shared_ptr<IComponentArray>, MAX_COMPONENTS> componentArray;
	};

	inline Entity CreateEntity()
	{
		static uint32_t id = 0;
		return Entity{ ++id, 0 };
	}

	void DestroyEntity(ComponentManager* mgr, Entity entity);
}
