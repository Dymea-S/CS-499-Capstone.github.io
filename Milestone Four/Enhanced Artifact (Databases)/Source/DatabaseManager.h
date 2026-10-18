///////////////////////////////////////////////////////////////////////////////
// DatabaseManager.h
// =================
// AUTHOR: Crystal Schippers
// 
// PURPOSE: Declares the database interface and scene-summary model used to
// persist named 3D scenes and their logical objects.
// 
// CAPSTONE ENHANCEMENT: Defines secure CRUD operations, validation boundaries,
// error reporting, and an implementation-hidden SQLite integration.
// 
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "SceneObjectRegistry.h"

#include <memory>
#include <string>
#include <vector>

struct SceneSummary
{
	long long sceneId;
	std::string name;
	std::string updatedAt;
	int objectCount;
};

class DatabaseManager
{
public:
	struct Implementation;

	DatabaseManager();
	~DatabaseManager();

	DatabaseManager(const DatabaseManager&) = delete;
	DatabaseManager& operator=(const DatabaseManager&) = delete;

	bool Open(const std::string& databasePath);
	void Close();
	bool IsOpen() const;
	bool InitializeSchema();

	bool SaveScene(const std::string& sceneName, const std::vector<SceneObject>& objects);
	bool LoadScene(const std::string& sceneName, std::vector<SceneObject>& objects);
	bool DeleteScene(const std::string& sceneName);
	bool ListScenes(std::vector<SceneSummary>& scenes);

	const std::string& LastError() const;

private:
	std::unique_ptr<Implementation> m_impl;
	std::string m_lastError;

	bool Execute(const char* sql);
	bool ValidateSceneName(const std::string& sceneName);
	void SetDatabaseError(const std::string& context);
};
