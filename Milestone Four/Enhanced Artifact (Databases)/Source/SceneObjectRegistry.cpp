///////////////////////////////////////////////////////////////////////////////
// SceneObjectRegistry.cpp
// =======================
// AUTHOR: Crystal Schippers
// 
// PURPOSE: Implements scene-object registration, transformed bounds, ray-to-
// AABB intersection testing, and nearest-object selection.
// 
// CAPSTONE ENHANCEMENT: Adds efficient lookup and geometric selection algorithms
// with validation and deterministic nearest-hit behavior.
// 
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#include "SceneObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	const float DIRECTION_EPSILON = 0.000001f;

	bool IsFiniteVector(const glm::vec3& value)
	{
		return std::isfinite(value.x) &&
			std::isfinite(value.y) &&
			std::isfinite(value.z);
	}
}

bool SceneObjectRegistry::AddObject(const SceneObject& object)
{
	if (object.id.empty() || m_indexById.find(object.id) != m_indexById.end())
	{
		return false;
	}

	if (!IsFiniteVector(object.localBounds.minimum) ||
		!IsFiniteVector(object.localBounds.maximum) ||
		glm::any(glm::greaterThan(object.localBounds.minimum, object.localBounds.maximum)))
	{
		return false;
	}

	m_indexById[object.id] = m_objects.size();
	m_objects.push_back(object);
	return true;
}

SceneObject* SceneObjectRegistry::FindById(const std::string& id)
{
	auto found = m_indexById.find(id);
	return found == m_indexById.end() ? nullptr : &m_objects[found->second];
}

const SceneObject* SceneObjectRegistry::FindById(const std::string& id) const
{
	auto found = m_indexById.find(id);
	return found == m_indexById.end() ? nullptr : &m_objects[found->second];
}

void SceneObjectRegistry::ClearSelection()
{
	for (SceneObject& object : m_objects)
	{
		object.selected = false;
	}
}

bool SceneObjectRegistry::ReplaceObjects(const std::vector<SceneObject>& objects)
{
	SceneObjectRegistry replacement;
	for (const SceneObject& object : objects)
	{
		if (!replacement.AddObject(object))
		{
			return false;
		}
	}
	m_objects.swap(replacement.m_objects);
	m_indexById.swap(replacement.m_indexById);
	return true;
}

const std::vector<SceneObject>& SceneObjectRegistry::Objects() const
{
	return m_objects;
}

BoundingBox SceneObjectRegistry::TransformBounds(
	const BoundingBox& localBounds,
	const glm::mat4& modelMatrix)
{
	BoundingBox worldBounds = {
		glm::vec3(std::numeric_limits<float>::max()),
		glm::vec3(std::numeric_limits<float>::lowest())
	};

	for (int x = 0; x < 2; ++x)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int z = 0; z < 2; ++z)
			{
				const glm::vec3 corner(
					x == 0 ? localBounds.minimum.x : localBounds.maximum.x,
					y == 0 ? localBounds.minimum.y : localBounds.maximum.y,
					z == 0 ? localBounds.minimum.z : localBounds.maximum.z);
				const glm::vec3 transformed = glm::vec3(modelMatrix * glm::vec4(corner, 1.0f));
				worldBounds.minimum = glm::min(worldBounds.minimum, transformed);
				worldBounds.maximum = glm::max(worldBounds.maximum, transformed);
			}
		}
	}

	return worldBounds;
}

bool SceneObjectRegistry::IntersectRayAABB(
	const Ray& ray,
	const BoundingBox& bounds,
	float& hitDistance)
{
	if (!IsFiniteVector(ray.origin) || !IsFiniteVector(ray.direction) ||
		glm::dot(ray.direction, ray.direction) <= DIRECTION_EPSILON)
	{
		return false;
	}

	float nearDistance = 0.0f;
	float farDistance = std::numeric_limits<float>::max();

	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::fabs(ray.direction[axis]) < DIRECTION_EPSILON)
		{
			if (ray.origin[axis] < bounds.minimum[axis] ||
				ray.origin[axis] > bounds.maximum[axis])
			{
				return false;
			}
			continue;
		}

		const float inverseDirection = 1.0f / ray.direction[axis];
		float first = (bounds.minimum[axis] - ray.origin[axis]) * inverseDirection;
		float second = (bounds.maximum[axis] - ray.origin[axis]) * inverseDirection;
		if (first > second)
		{
			std::swap(first, second);
		}

		nearDistance = std::max(nearDistance, first);
		farDistance = std::min(farDistance, second);
		if (nearDistance > farDistance)
		{
			return false;
		}
	}

	if (farDistance < 0.0f)
	{
		return false;
	}

	hitDistance = nearDistance >= 0.0f ? nearDistance : farDistance;
	return true;
}

const SceneObject* SceneObjectRegistry::SelectNearest(const Ray& ray)
{
	ClearSelection();
	std::size_t nearestIndex = m_objects.size();
	float nearestDistance = std::numeric_limits<float>::max();

	for (std::size_t index = 0; index < m_objects.size(); ++index)
	{
		const BoundingBox worldBounds = TransformBounds(
			m_objects[index].localBounds,
			m_objects[index].modelMatrix);
		float hitDistance = 0.0f;
		if (IntersectRayAABB(ray, worldBounds, hitDistance) &&
			hitDistance < nearestDistance)
		{
			nearestDistance = hitDistance;
			nearestIndex = index;
		}
	}

	if (nearestIndex == m_objects.size())
	{
		return nullptr;
	}

	m_objects[nearestIndex].selected = true;
	return &m_objects[nearestIndex];
}
