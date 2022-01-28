PRAGMA journal_mode = 'WAL';
PRAGMA foreign_keys = 1;

CREATE TABLE IF NOT EXISTS command (
    uuid TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    parent_cmd TEXT
);

CREATE UNIQUE INDEX command_name_idx
    ON command (name);

CREATE TABLE IF NOT EXISTS command_alias (
    uuid TEXT PRIMARY KEY,
    cmd_uuid TEXT NOT NULL,
    name TEXT NOT NULL
);

CREATE UNIQUE INDEX command_alias_name_idx
    ON command_alias (name);

CREATE TABLE IF NOT EXISTS command_arg (
    uuid TEXT PRIMARY KEY,
    cmd_uuid TEXT NOT NULL,
    cmd_type TEXT NOT NULL,
    long_name TEXT,
    short_name TEXT,
    FOREIGN KEY(cmd_uuid) REFERENCES command(uuid)
);

CREATE INDEX command_arg_cmd_uuid_idx
    ON command_arg (cmd_uuid);

CREATE TABLE IF NOT EXISTS command_opt (
    uuid TEXT PRIMARY KEY,
    cmd_arg_uuid TEXT NOT NULL,
    name TEXT NOT NULL,
    FOREIGN KEY(cmd_arg_uuid) REFERENCES command_arg(uuid)
);

CREATE INDEX command_opt_cmd_arg_idx
    ON command_opt (cmd_arg_uuid);

BEGIN TRANSACTION;

INSERT INTO command
    (uuid, name, parent_cmd)
    VALUES
    ('00000000-0000-0000-0000-000000000001', 'kubectl', NULL);

INSERT INTO command
    (uuid, name, parent_cmd)
    VALUES
    ('00000000-0000-0000-0000-000000000002', 'get', '00000000-0000-0000-0000-000000000001');

INSERT INTO command
    (uuid, name, parent_cmd)
    VALUES
    ('00000000-0000-0000-0000-000000000003', 'pods', '00000000-0000-0000-0000-000000000002');

INSERT INTO command
    (uuid, name, parent_cmd)
    VALUES
    ('00000000-0000-0000-0000-000000000004', 'replicasets', '00000000-0000-0000-0000-000000000002');

INSERT INTO command_alias
    (uuid, cmd_uuid, name)
    VALUES
    ('00000000-0000-0000-0003-000000000000', '00000000-0000-0000-0000-000000000003', 'pod');

INSERT INTO command_alias
    (uuid, cmd_uuid, name)
    VALUES
    ('00000000-0000-0000-0003-000000000002', '00000000-0000-0000-0000-000000000003', 'po');

INSERT INTO command_alias
    (uuid, cmd_uuid, name)
    VALUES
    ('00000000-0000-0000-0003-000000000003', '00000000-0000-0000-0000-000000000004', 'replicaset');

INSERT INTO command_alias
    (uuid, cmd_uuid, name)
    VALUES
    ('00000000-0000-0000-0003-000000000004', '00000000-0000-0000-0000-000000000004', 'rs');

INSERT INTO command_arg
    (uuid, cmd_uuid, cmd_type, long_name, short_name)
    VALUES
    ('00000000-0000-0000-1111-000000000001', '00000000-0000-0000-0000-000000000002', 'OPTION', '--output', '-o');

INSERT INTO command_arg
    (uuid, cmd_uuid, cmd_type, long_name, short_name)
    VALUES
    ('00000000-0000-0000-1111-000000000002', '00000000-0000-0000-0000-000000000001', 'FILE', '--file', '-f');

INSERT INTO command_arg
    (uuid, cmd_uuid, cmd_type, long_name, short_name)
    VALUES
    ('00000000-0000-0000-1111-000000000003', '00000000-0000-0000-0000-000000000001', 'TEXT', '--namespace', '-n');

INSERT INTO command_opt
    (uuid, cmd_arg_uuid, name)
    VALUES
    ('00000000-0000-0000-2222-000000000001', '00000000-0000-0000-1111-000000000001', 'json');

INSERT INTO command_opt
    (uuid, cmd_arg_uuid, name)
    VALUES
    ('00000000-0000-0000-2222-000000000002', '00000000-0000-0000-1111-000000000001', 'wide');

INSERT INTO command_opt
    (uuid, cmd_arg_uuid, name)
    VALUES
    ('00000000-0000-0000-2222-000000000003', '00000000-0000-0000-1111-000000000001', 'yaml');

COMMIT;


SELECT *
FROM command_opt co
JOIN command_arg ca ON ca.uuid = co.cmd_arg_uuid
WHERE ca.long_name = ?
OR ca.short_name = ?