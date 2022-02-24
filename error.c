#include "error.h"
#include <stdlib.h>

static const size_t MAX_MSG_LEN = 1024;

char *get_bce_error_msg(bce_error_t err) {
    char *msg = calloc(MAX_MSG_LEN + 1, sizeof(char));
    switch (err) {
        case ERR_NONE:
            break;
        case ERR_SQLITE_ERROR:
            break;
        case ERR_MISSING_ENV_COMP_LINE:
            break;
        case ERR_MISSING_ENV_COMP_POINT:
            break;
        case ERR_INVALID_ENV_COMP_POINT:
            break;
        case ERR_INVALID_CLI_ARGUMENT:
            break;
        case ERR_NO_DATABASE_CONNECTION:
            break;
        case ERR_INVALID_CMD_NAME:
            break;
        case ERR_INVALID_CMD:
            break;
        case ERR_INVALID_ALIAS:
            break;
        case ERR_INVALID_ARG:
            break;
        case ERR_INVALID_OPT:
            break;
        case ERR_READ_FILE:
            break;
        case ERR_DATABASE_SCHEMA_VERSION_MISMATCH:
            break;
        case ERR_OPEN_DATABASE:
            break;
        case ERR_DATABASE_PRAGMA:
            break;
        case ERR_DATABASE_CREATE_TABLE:
            break;
        case ERR_UUID_ERR:
            break;
        case ERR_DOWNLOAD_ERR:
            break;
        case ERR_INVALID_URL:
            break;
    }
    return msg;
}