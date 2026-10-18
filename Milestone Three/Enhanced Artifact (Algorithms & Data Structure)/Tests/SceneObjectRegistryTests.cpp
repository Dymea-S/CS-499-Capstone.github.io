#include "../Projects/7-1_FinalProjectMilestones/Source/SceneObjectRegistry.h"

#include <glm/gtx/transform.hpp>

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{
	const BoundingBox UNIT_BOX = { glm::vec3(-0.5f), glm::vec3(0.5f) };

	SceneObject MakeObject(
		const std::string& id,
		const glm::vec3& center,
		const glm::vec3& dimensions = glm::vec3(1.0f))
	{
		return { id, id, UNIT_BOX,
			glm::translate(center) * glm::scale(dimensions), false };
	}

	bool NearlyEqual(float first, float second)
	{
		return std::fabs(first - second) < 0.0001f;
	}
}

int main()
{
	SceneObjectRegistry registry;
	assert(registry.AddObject(MakeObject("near", glm::vec3(0.0f, 0.0f, -3.0f))));
	assert(registry.AddObject(MakeObject("far", glm::vec3(0.0f, 0.0f, -7.0f))));
	assert(!registry.AddObject(MakeObject("near", glm::vec3(1.0f))));
	assert(registry.FindById("near") != nullptr);
	assert(registry.FindById("missing") == nullptr);

	const Ray forward = { glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
	const SceneObject* selected = registry.SelectNearest(forward);
	assert(selected != nullptr && selected->id == "near");
	assert(registry.FindById("near")->selected);
	assert(!registry.FindById("far")->selected);

	const Ray miss = { glm::vec3(5.0f, 5.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
	assert(registry.SelectNearest(miss) == nullptr);
	assert(!registry.FindById("near")->selected);

	float hitDistance = 0.0f;
	const BoundingBox worldBox = SceneObjectRegistry::TransformBounds(
		UNIT_BOX,
		glm::translate(glm::vec3(2.0f, 0.0f, -4.0f)) *
		glm::scale(glm::vec3(2.0f, 1.0f, 1.0f)));
	const Ray transformedHit = { glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f) };
	assert(SceneObjectRegistry::IntersectRayAABB(transformedHit, worldBox, hitDistance));
	assert(NearlyEqual(hitDistance, 3.5f));

	const Ray parallelMiss = { glm::vec3(4.0f, 0.0f, -4.0f), glm::vec3(0.0f, 1.0f, 0.0f) };
	assert(!SceneObjectRegistry::IntersectRayAABB(parallelMiss, worldBox, hitDistance));

	const Ray pointsAway = { glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f) };
	assert(registry.SelectNearest(pointsAway) == nullptr);

	const Ray invalidRay = { glm::vec3(0.0f), glm::vec3(0.0f) };
	assert(!SceneObjectRegistry::IntersectRayAABB(invalidRay, worldBox, hitDistance));

	std::cout << "All 8 SceneObjectRegistry test cases passed." << std::endl;
	return 0;
}
