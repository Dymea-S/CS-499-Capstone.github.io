PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS scenes (
    scene_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE CHECK(length(name) BETWEEN 1 AND 80),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS scene_objects (
    object_id INTEGER PRIMARY KEY,
    scene_id INTEGER NOT NULL,
    object_key TEXT NOT NULL CHECK(length(object_key) BETWEEN 1 AND 80),
    display_name TEXT NOT NULL CHECK(length(display_name) BETWEEN 1 AND 120),
    selected INTEGER NOT NULL DEFAULT 0 CHECK(selected IN (0, 1)),
    min_x REAL NOT NULL,
    min_y REAL NOT NULL,
    min_z REAL NOT NULL,
    max_x REAL NOT NULL,
    max_y REAL NOT NULL,
    max_z REAL NOT NULL,
    m00 REAL NOT NULL, m01 REAL NOT NULL, m02 REAL NOT NULL, m03 REAL NOT NULL,
    m10 REAL NOT NULL, m11 REAL NOT NULL, m12 REAL NOT NULL, m13 REAL NOT NULL,
    m20 REAL NOT NULL, m21 REAL NOT NULL, m22 REAL NOT NULL, m23 REAL NOT NULL,
    m30 REAL NOT NULL, m31 REAL NOT NULL, m32 REAL NOT NULL, m33 REAL NOT NULL,
    FOREIGN KEY(scene_id) REFERENCES scenes(scene_id) ON DELETE CASCADE,
    UNIQUE(scene_id, object_key),
    CHECK(min_x <= max_x AND min_y <= max_y AND min_z <= max_z)
);

CREATE INDEX IF NOT EXISTS idx_scene_objects_scene_id
ON scene_objects(scene_id);

CREATE TABLE IF NOT EXISTS application_settings (
    setting_key TEXT PRIMARY KEY,
    setting_value TEXT NOT NULL
);
