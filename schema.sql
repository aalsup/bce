PRAGMA journal_mode = WAL;
PRAGMA foreign_keys = 1;

-- This value allows us to upgrade determine if the schema needs to be upgraded
PRAGMA user_version = 1;

DROP TABLE IF EXISTS command_opt;
DROP TABLE IF EXISTS command_arg;
DROP TABLE IF EXISTS command_alias;
DROP TABLE IF EXISTS command;

CREATE TABLE IF NOT EXISTS command (
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    parent_cmd TEXT,
    FOREIGN KEY(parent_cmd) REFERENCES command(uuid) ON DELETE CASCADE
);

CREATE UNIQUE INDEX command_name_idx
    ON command (name);

CREATE INDEX command_parent_idx
    ON command (parent_cmd);

CREATE TABLE IF NOT EXISTS command_alias (
    uuid TEXT PRIMARY KEY,
    cmd_uuid TEXT NOT NULL,
    name TEXT NOT NULL,
    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) ON DELETE CASCADE
);

CREATE INDEX command_alias_name_idx
    ON command_alias (name);

CREATE INDEX command_alias_cmd_uuid_idx
    ON command_alias (cmd_uuid);

CREATE UNIQUE INDEX command_alias_cmd_name_idx
    ON command_alias (cmd_uuid, name);

CREATE TABLE IF NOT EXISTS command_arg (
    uuid TEXT PRIMARY KEY,
    cmd_uuid TEXT NOT NULL,
    arg_type TEXT NOT NULL
        -- valid arg_type values
        CHECK (arg_type IN ('NONE', 'OPTION', 'FILE', 'TEXT')),
    description TEXT NOT NULL,
    long_name TEXT,
    short_name TEXT,
    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid) ON DELETE CASCADE,
    -- ensure either long_name or short_name has data
    CHECK ( (long_name IS NOT NULL) OR (short_name IS NOT NULL) )
);

CREATE INDEX command_arg_cmd_uuid_idx
    ON command_arg (cmd_uuid);

CREATE UNIQUE INDEX command_arg_longname_idx
    ON command_arg (cmd_uuid, long_name);

CREATE TABLE IF NOT EXISTS command_opt (
    uuid TEXT PRIMARY KEY,
    cmd_arg_uuid TEXT NOT NULL,
    name TEXT NOT NULL,
    FOREIGN KEY(cmd_arg_uuid) REFERENCES command_arg(uuid) ON DELETE CASCADE
);

CREATE INDEX command_opt_cmd_arg_idx
    ON command_opt (cmd_arg_uuid);

CREATE UNIQUE INDEX command_opt_arg_name_idx
    ON command_opt (cmd_arg_uuid, name);
