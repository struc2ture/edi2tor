#pragma once

#include <stdarg.h>
#include <stdio.h>

#define hub_trace(FMT, ...) printf("[HUB:TRACE:%s:%d(%s)] " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define hub_warning(FMT, ...) printf("[HUB:WARNING:%s:%d(%s)] " FMT "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
