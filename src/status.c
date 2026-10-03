#include "rovelle/status.h"

const char *rovelle_status_name(rovelle_status_t status)
{
    switch (status) {
    case ROVELLE_OK:
        return "ROVELLE_OK";
    case ROVELLE_NOT_FOUND:
        return "ROVELLE_NOT_FOUND";
    case ROVELLE_CONFLICT:
        return "ROVELLE_CONFLICT";
    case ROVELLE_IO_ERROR:
        return "ROVELLE_IO_ERROR";
    case ROVELLE_INVALID_ARGUMENT:
        return "ROVELLE_INVALID_ARGUMENT";
    default:
        return "ROVELLE_UNKNOWN_STATUS";
    }
}
