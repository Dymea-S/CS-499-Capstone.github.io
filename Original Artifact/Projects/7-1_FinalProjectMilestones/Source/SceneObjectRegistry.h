///////////////////////////////////////////////////////////////////////////////
// SceneObjectRegistry.h
// =====================
// Data structures and geometric algorithms used to select logical objects in
// the CS 330 scene.
//
// ENHANCED BY: Dymea Schippers - CS 499 Computer Science Capstone, 2026
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
