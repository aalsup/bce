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
    ('00000000-0000-0000-0003-000000000000', '00000000-0000-0000-0000-000000000001', 'bbb');

INSERT INTO command_alias
(uuid, cmd_uuid, name)
VALUES
    ('00000000-0000-0000-0003-000000000001', '00000000-0000-0000-0000-000000000003', 'pod');

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
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000001',
    '00000000-0000-0000-0000-000000000002',
    'OPTION',
    'Output format',
    '--output',
    '-o'
);

INSERT INTO command_arg
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000002',
    '00000000-0000-0000-0000-000000000001',
    'FILE',
    'read/write data using the provided file',
    '--file',
    '-f'
);

INSERT INTO command_arg
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000003',
    '00000000-0000-0000-0000-000000000001',
    'TEXT',
    'Use the specified namespace',
    '--namespace',
    '-n'
);

INSERT INTO command_arg
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000004',
    '00000000-0000-0000-0000-000000000001',
    'NONE',
    'Use all namespaces',
    '--all-namespaces',
    '-A'
);

INSERT INTO command_arg
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000005',
    '00000000-0000-0000-0000-000000000002',
    'TEXT',
    'Build a kustomization target from a directory or URL',
    '--kustomize',
    '-k'
);

INSERT INTO command_arg
(uuid, cmd_uuid, arg_type, description, long_name, short_name)
VALUES
(
    '00000000-0000-0000-1111-000000000006',
    '00000000-0000-0000-0000-000000000002',
    'TEXT',
    'comma separated list of labels to be presented as columns',
    '--label-columns',
    '-L'
);

INSERT INTO command_opt
(uuid, cmd_arg_uuid, name)
VALUES
(
    '00000000-0000-0000-2222-000000000001',
    '00000000-0000-0000-1111-000000000001',
    'json'
);

INSERT INTO command_opt
(uuid, cmd_arg_uuid, name)
VALUES
(
    '00000000-0000-0000-2222-000000000002',
    '00000000-0000-0000-1111-000000000001',
    'wide'
);

INSERT INTO command_opt
(uuid, cmd_arg_uuid, name)
VALUES
(
    '00000000-0000-0000-2222-000000000003',
    '00000000-0000-0000-1111-000000000001',
    'yaml'
);

INSERT INTO command_opt
(uuid, cmd_arg_uuid, name)
VALUES
(
    '00000000-0000-0000-2222-000000000004',
    '00000000-0000-0000-1111-000000000001',
    'name'
);

COMMIT;
