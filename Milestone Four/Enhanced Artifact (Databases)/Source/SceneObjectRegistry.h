///////////////////////////////////////////////////////////////////////////////
// SceneObjectRegistry.h
// =====================
// AUTHOR: Crystal Schippers
// 
// PURPOSE: Declares scene-object data structures, bounding boxes, rays, and the
// registry used to locate and select logical objects in the 3D scene.
// 
// CAPSTONE ENHANCEMENT: Introduces reusable data structures, constant-time ID
// lookup, and interfaces for ray-casting and nearest-hit algorithms.
// 
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>
#include <vector>

struct Ray
{
	glm::vec3 origin;
	glm::vec3 direction;
};

struct BoundingBox
{
	glm::vec3 minimum;
	glm::vec3 maximum;
};

struct SceneObject
{
	std::string id;
	std::string displayName;
	BoundingBox localBounds;
	glm::mat4 modelMatrix;
	bool selected;
};

class SceneObjectRegistry
{
public:
	bool AddObject(const SceneObject& object);
	SceneObject* FindById(const std::string& id);
	const SceneObject* FindById(const std::string& id) const;
	const SceneObject* SelectNearest(const Ray& ray);
	void ClearSelection();
	bool ReplaceObjects(const std::vector<SceneObject>& objects);
	const std::vector<SceneObject>& Objects() const;

	static BoundingBox TransformBounds(
		const BoundingBox& localBounds,
		const glm::mat4& modelMatrix);
	static bool IntersectRayAABB(
		const Ray& ray,
		const BoundingBox& bounds,
		float& hitDistance);

private:
	std::vector<SceneObject> m_objects;
	std::unordered_map<std::string, std::size_t> m_indexById;
};
