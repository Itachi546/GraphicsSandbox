#include "ECS.h"
namespace ecs
{
	void DestroyEntity(ComponentManager* mgr, Entity entity)
	{

		uint64_t signature = entity.signature;
		for (uint32_t i = 0; i < MAX_COMPONENTS; ++i)
		{
			uint64_t compHash = (1Ui64 << i);
			if ((signature & compHash) == compHash)
			{
				// Component is present
				std::shared_ptr<IComponentArray> comp = mgr->GetComponentArray(i);
				comp->removeEntity(entity);
			}
		}

		entity.handle = INVALID_ENTITY;
		entity.signature = 0;
	}

}