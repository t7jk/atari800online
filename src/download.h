#ifndef DOWNLOAD_H_
#define DOWNLOAD_H_

#include "config.h"

#ifdef HAVE_DOWNLOAD

#include <stddef.h>

/* Fetches url into a malloc'ed buffer (*data, *size); caller frees it.
   timeout_secs <= 0 means no total timeout. Returns TRUE on success. */
int Download_ToMemory(const char *url, unsigned char **data, size_t *size, long timeout_secs);

/* Extracts files with matching extensions from an in-memory ZIP into dest_dir.
   The buffer is not freed. Returns name of first extracted file or NULL. */
const char *Download_ExtractZipBuffer(const unsigned char *zipdata, size_t zipsize, const char *exts[], const char *dest_dir);

const char *Download_And_Extract(const char *url, const char *exts[], const char *dest_dir);

#endif /* HAVE_DOWNLOAD */

#endif /* DOWNLOAD_H_ */
