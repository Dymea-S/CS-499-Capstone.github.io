///////////////////////////////////////////////////////////////////////////////
// DatabaseManagerTests.cpp
// ========================
// AUTHOR: Crystal Schippers
// PURPOSE: Verifies database schema creation, parameterized CRUD operations,
// transactions, validation, scene transforms, and cascading deletion.
// CAPSTONE ENHANCEMENT: Provides automated evidence that the database and
// security-focused persistence requirements behave as designed.
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#include "../Projects/7-1_FinalProjectMilestones/Source/DatabaseManager.h"

#include <glm/gtx/transform.hpp>

#include <cassert>
#include <cstdio>
#include <iostream>

namespace
{
	SceneObject MakeObject(const std::string& id, float x, bool selected)
	{
		return {
			id,
			"Display " + id,
			{ glm::vec3(-0.5f), glm::vec3(0.5f) },
			glm::translate(glm::vec3(x, 0.0f, 0.0f)),
			selected
		};
	}
}

int main()
{
	const char* databasePath = "/tmp/cs499_scene_database_test.db";
	std::remove(databasePath);

	DatabaseManager database;
	assert(database.Open(databasePath));
	assert(database.IsOpen());

	std::vector<SceneObject> original = {
		MakeObject("cup", 8.0f, true),
		MakeObject("laptop", 5.0f, false)
	};
	assert(database.SaveScene("Portfolio Scene", original));

	std::vector<SceneSummary> scenes;
	assert(database.ListScenes(scenes));
	assert(scenes.size() == 1);
	assert(scenes[0].name == "Portfolio Scene");
	assert(scenes[0].objectCount == 2);

	std::vector<SceneObject> loaded;
	assert(database.LoadScene("Portfolio Scene", loaded));
	assert(loaded.size() == 2);
	assert(loaded[0].id == "cup");
	assert(loaded[0].selected);
	assert(loaded[0].modelMatrix[3][0] == 8.0f);

	// Updating the same scene replaces the child rows inside one transaction.
	std::vector<SceneObject> updated = { MakeObject("plant", 2.0f, true) };
	assert(database.SaveScene("Portfolio Scene", updated));
	assert(database.LoadScene("Portfolio Scene", loaded));
	assert(loaded.size() == 1 && loaded[0].id == "plant");

	// A quote-heavy name is stored as data through a prepared statement.
	assert(database.SaveScene("Robert'); DROP TABLE scenes;--", updated));
	assert(database.ListScenes(scenes));
	assert(scenes.size() == 2);

	assert(!database.SaveScene("", updated));
	assert(!database.LoadScene("Missing Scene", loaded));

	assert(database.DeleteScene("Portfolio Scene"));
	assert(database.ListScenes(scenes));
	assert(scenes.size() == 1);
	assert(scenes[0].name == "Robert'); DROP TABLE scenes;--");

	database.Close();
	assert(!database.IsOpen());
	std::remove(databasePath);

	std::cout << "All 10 DatabaseManager test cases passed." << std::endl;
	return 0;
}
