#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdbool.h>

bool file_exists(const char *filename);
bool download_file(const char *url, const char *filename);

#endif // DOWNLOAD_H