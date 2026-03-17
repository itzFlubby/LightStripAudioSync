#include "logger.hpp"

void log(const char* format, ...) {
    char output[BUFFER_SIZE] = { 0 };

    va_list arguments;
    va_start(arguments, format);
    vsnprintf(output, BUFFER_SIZE, format, arguments);
    printf("[LSAS] %s\n", output);
    va_end(arguments);
}

void log_ws(const char* message) {
    printf("[ WS ] %s", message);
}