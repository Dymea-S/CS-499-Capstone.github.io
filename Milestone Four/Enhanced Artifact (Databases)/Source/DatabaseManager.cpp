///////////////////////////////////////////////////////////////////////////////
// DatabaseManager.cpp
// ===================
// AUTHOR: Crystal Schippers
// 
// PURPOSE: Loads SQLite at runtime and provides validated, parameterized,
// transactional CRUD operations for persistent 3D scene data.
// 
// CAPSTONE ENHANCEMENT: Adds secure database persistence, scene save/load,
// update, listing, deletion, rollback protection, and Win32 API compatibility.
// 
// COURSE: CS 499 Computer Science Capstone, 2026
///////////////////////////////////////////////////////////////////////////////

#include "DatabaseManager.h"

#include <cmath>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <Windows.h>
#define SQLITE_RUNTIME_CALL __stdcall
#else
#include <dlfcn.h>
#define SQLITE_RUNTIME_CALL
#endif

namespace
{
	struct sqlite3;
	struct sqlite3_stmt;

	const int SQLITE_OK = 0;
	const int SQLITE_ROW = 100;
	const int SQLITE_DONE = 101;
	const int SQLITE_OPEN_READWRITE = 0x00000002;
	const int SQLITE_OPEN_CREATE = 0x00000004;
	const int SQLITE_OPEN_FULLMUTEX = 0x00010000;
	typedef void (SQLITE_RUNTIME_CALL* SqliteDestructor)(void*);
	typedef int (SQLITE_RUNTIME_CALL* SqliteExecCallback)(void*, int, char**, char**);
	SqliteDestructor SQLITE_TRANSIENT_VALUE = reinterpret_cast<SqliteDestructor>(-1);

	bool IsFiniteMatrix(const glm::mat4& matrix)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
				{
					return false;
				}
			}
		}
		return true;
	}
}

struct DatabaseManager::Implementation
{
#ifdef _WIN32
	HMODULE library = nullptr;
#else
	void* library = nullptr;
#endif
	sqlite3* database = nullptr;

	// Windows ships winsqlite3.dll with __stdcall exports. Declaring these
	// pointers explicitly prevents a Win32 stack imbalance (ESP failure).
	int (SQLITE_RUNTIME_CALL* open_v2)(const char*, sqlite3**, int, const char*) = nullptr;
	int (SQLITE_RUNTIME_CALL* close_v2)(sqlite3*) = nullptr;
	int (SQLITE_RUNTIME_CALL* exec)(sqlite3*, const char*, SqliteExecCallback, void*, char**) = nullptr;
	void (SQLITE_RUNTIME_CALL* free_memory)(void*) = nullptr;
	const char* (SQLITE_RUNTIME_CALL* errmsg)(sqlite3*) = nullptr;
	int (SQLITE_RUNTIME_CALL* busy_timeout)(sqlite3*, int) = nullptr;
	int (SQLITE_RUNTIME_CALL* prepare_v2)(sqlite3*, const char*, int, sqlite3_stmt**, const char**) = nullptr;
	int (SQLITE_RUNTIME_CALL* step)(sqlite3_stmt*) = nullptr;
	int (SQLITE_RUNTIME_CALL* finalize)(sqlite3_stmt*) = nullptr;
	int (SQLITE_RUNTIME_CALL* reset)(sqlite3_stmt*) = nullptr;
	int (SQLITE_RUNTIME_CALL* clear_bindings)(sqlite3_stmt*) = nullptr;
	int (SQLITE_RUNTIME_CALL* bind_text)(sqlite3_stmt*, int, const char*, int, SqliteDestructor) = nullptr;
	int (SQLITE_RUNTIME_CALL* bind_int)(sqlite3_stmt*, int, int) = nullptr;
	int (SQLITE_RUNTIME_CALL* bind_int64)(sqlite3_stmt*, int, long long) = nullptr;
	int (SQLITE_RUNTIME_CALL* bind_double)(sqlite3_stmt*, int, double) = nullptr;
	const unsigned char* (SQLITE_RUNTIME_CALL* column_text)(sqlite3_stmt*, int) = nullptr;
	int (SQLITE_RUNTIME_CALL* column_int)(sqlite3_stmt*, int) = nullptr;
	long long (SQLITE_RUNTIME_CALL* column_int64)(sqlite3_stmt*, int) = nullptr;
	double (SQLITE_RUNTIME_CALL* column_double)(sqlite3_stmt*, int) = nullptr;
	long long (SQLITE_RUNTIME_CALL* last_insert_rowid)(sqlite3*) = nullptr;

	void* Symbol(const char* name)
	{
#ifdef _WIN32
		return reinterpret_cast<void*>(GetProcAddress(library, name));
#else
		return dlsym(library, name);
#endif
	}

	template <typename T>
	bool LoadSymbol(T& destination, const char* name)
	{
		destination = reinterpret_cast<T>(Symbol(name));
		return destination != nullptr;
	}

	bool LoadLibraryFile()
	{
#ifdef _WIN32
		library = LoadLibraryA("winsqlite3.dll");
#else
		library = dlopen("libsqlite3.so.0", RTLD_NOW | RTLD_LOCAL);
		if (library == nullptr)
		{
			library = dlopen("libsqlite3.so", RTLD_NOW | RTLD_LOCAL);
		}
#endif
		if (library == nullptr)
		{
			return false;
		}

		const bool closeLoaded = LoadSymbol(close_v2, "sqlite3_close_v2") ||
			LoadSymbol(close_v2, "sqlite3_close");
		return LoadSymbol(open_v2, "sqlite3_open_v2") &&
			closeLoaded &&
			LoadSymbol(exec, "sqlite3_exec") &&
			LoadSymbol(free_memory, "sqlite3_free") &&
			LoadSymbol(errmsg, "sqlite3_errmsg") &&
			LoadSymbol(busy_timeout, "sqlite3_busy_timeout") &&
			LoadSymbol(prepare_v2, "sqlite3_prepare_v2") &&
			LoadSymbol(step, "sqlite3_step") &&
			LoadSymbol(finalize, "sqlite3_finalize") &&
			LoadSymbol(reset, "sqlite3_reset") &&
			LoadSymbol(clear_bindings, "sqlite3_clear_bindings") &&
			LoadSymbol(bind_text, "sqlite3_bind_text") &&
			LoadSymbol(bind_int, "sqlite3_bind_int") &&
			LoadSymbol(bind_int64, "sqlite3_bind_int64") &&
			LoadSymbol(bind_double, "sqlite3_bind_double") &&
			LoadSymbol(column_text, "sqlite3_column_text") &&
			LoadSymbol(column_int, "sqlite3_column_int") &&
			LoadSymbol(column_int64, "sqlite3_column_int64") &&
			LoadSymbol(column_double, "sqlite3_column_double") &&
			LoadSymbol(last_insert_rowid, "sqlite3_last_insert_rowid");
	}

	void UnloadLibraryFile()
	{
		if (library == nullptr)
		{
			return;
		}
#ifdef _WIN32
		FreeLibrary(library);
#else
		dlclose(library);
#endif
		library = nullptr;
	}
};

namespace
{
	class Statement
	{
	public:
		Statement(DatabaseManager::Implementation* implementation, const char* sql)
			: m_implementation(implementation), m_statement(nullptr)
		{
			if (implementation != nullptr && implementation->database != nullptr)
			{
				implementation->prepare_v2(
					implementation->database, sql, -1, &m_statement, nullptr);
			}
		}

		~Statement()
		{
			if (m_statement != nullptr)
			{
				m_implementation->finalize(m_statement);
			}
		}

		bool IsValid() const { return m_statement != nullptr; }
		sqlite3_stmt* Get() const { return m_statement; }

	private:
		DatabaseManager::Implementation* m_implementation;
		sqlite3_stmt* m_statement;
	};

	std::string ColumnString(DatabaseManager::Implementation* implementation,
		sqlite3_stmt* statement, int column)
	{
		const unsigned char* value = implementation->column_text(statement, column);
		return value == nullptr ? std::string() : reinterpret_cast<const char*>(value);
	}
}

DatabaseManager::DatabaseManager()
	: m_impl(new Implementation())
{
}

DatabaseManager::~DatabaseManager()
{
	Close();
}

bool DatabaseManager::Open(const std::string& databasePath)
{
	Close();
	if (databasePath.empty())
	{
		m_lastError = "The database path cannot be empty.";
		return false;
	}
	if (!m_impl->LoadLibraryFile())
	{
		m_lastError = "The platform SQLite library could not be loaded.";
		return false;
	}

	const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
	if (m_impl->open_v2(databasePath.c_str(), &m_impl->database, flags, nullptr) != SQLITE_OK)
	{
		SetDatabaseError("Unable to open the scene database");
		Close();
		return false;
	}
	m_impl->busy_timeout(m_impl->database, 3000);
	return Execute("PRAGMA foreign_keys = ON;") && InitializeSchema();
}

void DatabaseManager::Close()
{
	if (m_impl->database != nullptr && m_impl->close_v2 != nullptr)
	{
		m_impl->close_v2(m_impl->database);
		m_impl->database = nullptr;
	}
	m_impl->UnloadLibraryFile();
}

bool DatabaseManager::IsOpen() const
{
	return m_impl->database != nullptr;
}

const std::string& DatabaseManager::LastError() const
{
	return m_lastError;
}

void DatabaseManager::SetDatabaseError(const std::string& context)
{
	m_lastError = context;
	if (m_impl->database != nullptr && m_impl->errmsg != nullptr)
	{
		m_lastError += ": ";
		m_lastError += m_impl->errmsg(m_impl->database);
	}
}

bool DatabaseManager::Execute(const char* sql)
{
	char* errorMessage = nullptr;
	const int result = m_impl->exec(m_impl->database, sql, nullptr, nullptr, &errorMessage);
	if (result == SQLITE_OK)
	{
		return true;
	}
	m_lastError = errorMessage == nullptr ? "SQLite command failed." : errorMessage;
	if (errorMessage != nullptr)
	{
		m_impl->free_memory(errorMessage);
	}
	return false;
}

bool DatabaseManager::InitializeSchema()
{
	const char* schema =
		"CREATE TABLE IF NOT EXISTS scenes ("
		" scene_id INTEGER PRIMARY KEY,"
		" name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 80),"
		" created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
		" updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
		");"
		"CREATE TABLE IF NOT EXISTS scene_objects ("
		" object_id INTEGER PRIMARY KEY,"
		" scene_id INTEGER NOT NULL,"
		" object_key TEXT NOT NULL CHECK(length(object_key) BETWEEN 1 AND 80),"
		" display_name TEXT NOT NULL CHECK(length(display_name) BETWEEN 1 AND 120),"
		" selected INTEGER NOT NULL DEFAULT 0 CHECK(selected IN (0, 1)),"
		" min_x REAL NOT NULL, min_y REAL NOT NULL, min_z REAL NOT NULL,"
		" max_x REAL NOT NULL, max_y REAL NOT NULL, max_z REAL NOT NULL,"
		" m00 REAL NOT NULL, m01 REAL NOT NULL, m02 REAL NOT NULL, m03 REAL NOT NULL,"
		" m10 REAL NOT NULL, m11 REAL NOT NULL, m12 REAL NOT NULL, m13 REAL NOT NULL,"
		" m20 REAL NOT NULL, m21 REAL NOT NULL, m22 REAL NOT NULL, m23 REAL NOT NULL,"
		" m30 REAL NOT NULL, m31 REAL NOT NULL, m32 REAL NOT NULL, m33 REAL NOT NULL,"
		" FOREIGN KEY(scene_id) REFERENCES scenes(scene_id) ON DELETE CASCADE,"
		" UNIQUE(scene_id, object_key),"
		" CHECK(min_x <= max_x AND min_y <= max_y AND min_z <= max_z)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_scene_objects_scene_id "
		"ON scene_objects(scene_id);"
		"CREATE TABLE IF NOT EXISTS application_settings ("
		" setting_key TEXT PRIMARY KEY,"
		" setting_value TEXT NOT NULL"
		");";
	return Execute(schema);
}

bool DatabaseManager::ValidateSceneName(const std::string& sceneName)
{
	if (sceneName.empty() || sceneName.size() > 80)
	{
		m_lastError = "Scene names must contain between 1 and 80 characters.";
		return false;
	}
	for (unsigned char character : sceneName)
	{
		if (character < 32 || character == 127)
		{
			m_lastError = "Scene names cannot contain control characters.";
			return false;
		}
	}
	return true;
}

bool DatabaseManager::SaveScene(
	const std::string& sceneName,
	const std::vector<SceneObject>& objects)
{
	m_lastError.clear();
	if (!IsOpen() || !ValidateSceneName(sceneName))
	{
		return false;
	}
	for (const SceneObject& object : objects)
	{
		if (object.id.empty() || object.id.size() > 80 ||
			object.displayName.empty() || object.displayName.size() > 120 ||
			!IsFiniteMatrix(object.modelMatrix))
		{
			m_lastError = "The scene contains invalid object data.";
			return false;
		}
	}

	if (!Execute("BEGIN IMMEDIATE TRANSACTION;"))
	{
		return false;
	}
	bool success = false;
	do
	{
		Statement insertScene(m_impl.get(), "INSERT OR IGNORE INTO scenes(name) VALUES(?);");
		if (!insertScene.IsValid() ||
			m_impl->bind_text(insertScene.Get(), 1, sceneName.c_str(), -1, SQLITE_TRANSIENT_VALUE) != SQLITE_OK ||
			m_impl->step(insertScene.Get()) != SQLITE_DONE)
		{
			SetDatabaseError("Unable to create the scene record");
			break;
		}
		Statement updateScene(m_impl.get(),
			"UPDATE scenes SET updated_at=CURRENT_TIMESTAMP WHERE name=?;");
		if (!updateScene.IsValid() ||
			m_impl->bind_text(updateScene.Get(), 1, sceneName.c_str(), -1, SQLITE_TRANSIENT_VALUE) != SQLITE_OK ||
			m_impl->step(updateScene.Get()) != SQLITE_DONE)
		{
			SetDatabaseError("Unable to update the scene timestamp");
			break;
		}

		long long sceneId = 0;
		Statement findScene(m_impl.get(), "SELECT scene_id FROM scenes WHERE name=?;");
		if (!findScene.IsValid() ||
			m_impl->bind_text(findScene.Get(), 1, sceneName.c_str(), -1, SQLITE_TRANSIENT_VALUE) != SQLITE_OK ||
			m_impl->step(findScene.Get()) != SQLITE_ROW)
		{
			SetDatabaseError("Unable to retrieve the saved scene identifier");
			break;
		}
		sceneId = m_impl->column_int64(findScene.Get(), 0);

		Statement clearObjects(m_impl.get(), "DELETE FROM scene_objects WHERE scene_id=?;");
		if (!clearObjects.IsValid() ||
			m_impl->bind_int64(clearObjects.Get(), 1, sceneId) != SQLITE_OK ||
			m_impl->step(clearObjects.Get()) != SQLITE_DONE)
		{
			SetDatabaseError("Unable to replace the previous scene objects");
			break;
		}

		const char* insertSql =
			"INSERT INTO scene_objects("
			"scene_id, object_key, display_name, selected,"
			"min_x,min_y,min_z,max_x,max_y,max_z,"
			"m00,m01,m02,m03,m10,m11,m12,m13,"
			"m20,m21,m22,m23,m30,m31,m32,m33)"
			" VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
		Statement insertObject(m_impl.get(), insertSql);
		if (!insertObject.IsValid())
		{
			SetDatabaseError("Unable to prepare the object insert statement");
			break;
		}

		for (const SceneObject& object : objects)
		{
			m_impl->bind_int64(insertObject.Get(), 1, sceneId);
			m_impl->bind_text(insertObject.Get(), 2, object.id.c_str(), -1, SQLITE_TRANSIENT_VALUE);
			m_impl->bind_text(insertObject.Get(), 3, object.displayName.c_str(), -1, SQLITE_TRANSIENT_VALUE);
			m_impl->bind_int(insertObject.Get(), 4, object.selected ? 1 : 0);
			m_impl->bind_double(insertObject.Get(), 5, object.localBounds.minimum.x);
			m_impl->bind_double(insertObject.Get(), 6, object.localBounds.minimum.y);
			m_impl->bind_double(insertObject.Get(), 7, object.localBounds.minimum.z);
			m_impl->bind_double(insertObject.Get(), 8, object.localBounds.maximum.x);
			m_impl->bind_double(insertObject.Get(), 9, object.localBounds.maximum.y);
			m_impl->bind_double(insertObject.Get(), 10, object.localBounds.maximum.z);
			int parameter = 11;
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					m_impl->bind_double(insertObject.Get(), parameter++, object.modelMatrix[column][row]);
				}
			}
			if (m_impl->step(insertObject.Get()) != SQLITE_DONE)
			{
				SetDatabaseError("Unable to save a scene object");
				break;
			}
			m_impl->reset(insertObject.Get());
			m_impl->clear_bindings(insertObject.Get());
		}
		if (!m_lastError.empty())
		{
			break;
		}
		success = Execute("COMMIT;");
	} while (false);

	if (!success)
	{
		Execute("ROLLBACK;");
	}
	return success;
}

bool DatabaseManager::LoadScene(
	const std::string& sceneName,
	std::vector<SceneObject>& objects)
{
	objects.clear();
	if (!IsOpen() || !ValidateSceneName(sceneName))
	{
		return false;
	}

	const char* sql =
		"SELECT o.object_key,o.display_name,o.selected,"
		"o.min_x,o.min_y,o.min_z,o.max_x,o.max_y,o.max_z,"
		"o.m00,o.m01,o.m02,o.m03,o.m10,o.m11,o.m12,o.m13,"
		"o.m20,o.m21,o.m22,o.m23,o.m30,o.m31,o.m32,o.m33 "
		"FROM scene_objects o JOIN scenes s ON s.scene_id=o.scene_id "
		"WHERE s.name=? ORDER BY o.object_id;";
	Statement statement(m_impl.get(), sql);
	if (!statement.IsValid() ||
		m_impl->bind_text(statement.Get(), 1, sceneName.c_str(), -1, SQLITE_TRANSIENT_VALUE) != SQLITE_OK)
	{
		SetDatabaseError("Unable to prepare the scene query");
		return false;
	}

	int result = SQLITE_ROW;
	while ((result = m_impl->step(statement.Get())) == SQLITE_ROW)
	{
		SceneObject object;
		object.id = ColumnString(m_impl.get(), statement.Get(), 0);
		object.displayName = ColumnString(m_impl.get(), statement.Get(), 1);
		object.selected = m_impl->column_int(statement.Get(), 2) == 1;
		object.localBounds.minimum = glm::vec3(
			static_cast<float>(m_impl->column_double(statement.Get(), 3)),
			static_cast<float>(m_impl->column_double(statement.Get(), 4)),
			static_cast<float>(m_impl->column_double(statement.Get(), 5)));
		object.localBounds.maximum = glm::vec3(
			static_cast<float>(m_impl->column_double(statement.Get(), 6)),
			static_cast<float>(m_impl->column_double(statement.Get(), 7)),
			static_cast<float>(m_impl->column_double(statement.Get(), 8)));
		int columnIndex = 9;
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				object.modelMatrix[column][row] =
					static_cast<float>(m_impl->column_double(statement.Get(), columnIndex++));
			}
		}
		objects.push_back(object);
	}
	if (result != SQLITE_DONE)
	{
		objects.clear();
		SetDatabaseError("Unable to load the scene objects");
		return false;
	}
	if (objects.empty())
	{
		m_lastError = "The requested scene does not exist or contains no objects.";
		return false;
	}
	m_lastError.clear();
	return true;
}

bool DatabaseManager::DeleteScene(const std::string& sceneName)
{
	if (!IsOpen() || !ValidateSceneName(sceneName))
	{
		return false;
	}
	Statement statement(m_impl.get(), "DELETE FROM scenes WHERE name=?;");
	if (!statement.IsValid() ||
		m_impl->bind_text(statement.Get(), 1, sceneName.c_str(), -1, SQLITE_TRANSIENT_VALUE) != SQLITE_OK ||
		m_impl->step(statement.Get()) != SQLITE_DONE)
	{
		SetDatabaseError("Unable to delete the scene");
		return false;
	}
	m_lastError.clear();
	return true;
}

bool DatabaseManager::ListScenes(std::vector<SceneSummary>& scenes)
{
	scenes.clear();
	if (!IsOpen())
	{
		m_lastError = "The database is not open.";
		return false;
	}
	Statement statement(m_impl.get(),
		"SELECT s.scene_id,s.name,s.updated_at,COUNT(o.object_id) "
		"FROM scenes s LEFT JOIN scene_objects o ON o.scene_id=s.scene_id "
		"GROUP BY s.scene_id,s.name,s.updated_at ORDER BY s.name;");
	if (!statement.IsValid())
	{
		SetDatabaseError("Unable to prepare the scene list query");
		return false;
	}
	int result = SQLITE_ROW;
	while ((result = m_impl->step(statement.Get())) == SQLITE_ROW)
	{
		scenes.push_back({
			m_impl->column_int64(statement.Get(), 0),
			ColumnString(m_impl.get(), statement.Get(), 1),
			ColumnString(m_impl.get(), statement.Get(), 2),
			m_impl->column_int(statement.Get(), 3)
		});
	}
	if (result != SQLITE_DONE)
	{
		SetDatabaseError("Unable to list the saved scenes");
		return false;
	}
	m_lastError.clear();
	return true;
}
