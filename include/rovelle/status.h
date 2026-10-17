#ifndef ROVELLE_STATUS_H
#define ROVELLE_STATUS_H

typedef enum rovelle_status {
    ROVELLE_OK = 0,
    ROVELLE_NOT_FOUND,
    ROVELLE_CONFLICT,
    ROVELLE_IO_ERROR,
    ROVELLE_INVALID_ARGUMENT,
    ROVELLE_CORRUPT
} rovelle_status_t;

const char *rovelle_status_name(rovelle_status_t status);

#endif
