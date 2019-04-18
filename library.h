#ifndef CLOUD_STORAGE_LIBRARY_H
#define CLOUD_STORAGE_LIBRARY_H

#include <vector>
#include "common.h"

typedef struct {
    int nCount;
    std::vector<WIN32_FIND_DATAW> resource_array;
} tResources, *pResources;

typedef std::basic_string<WCHAR> wcharstring;

#endif