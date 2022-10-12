#include "ECS.h"
namespace ecs
{
	void DestroyEntity(ComponentManager* mgr, Entity& entity)
	{
		for (uint32_t i = 0; i < MAX_COMPONENTS; ++i)
		{
			std::shared_ptr<IComponentArray> comp = mgr->GetBaseComponentArray(i);
			if(comp)
				comp->removeEntity(entity);
		}
		entity = 0;
	}

}