#pragma once

#include <cstdarg>
#include <cstdio>

constexpr const unsigned BUFFER_SIZE = 1024;

void log(const char* format, ...);

void log_ws(const char* message);