#include "download.h"
#include <curl/curl.h>
#include <stdio.h>

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *) stream);
    return written;
}

bool file_exists(const char *filename) {
    FILE *file;
    file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }

    return false;
}

bool download_file(const char *url, const char *filename) {
    CURL *curl_handle;

    /* ensure filename doesn't exist */
    if (file_exists(filename)) {
        remove(filename);
    }

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* set URL to get here */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* follow redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    /* Switch on full protocol/debug output while testing */
    curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);

    /* disable progress meter, set to 0L to enable it */
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    /* open the file */
    FILE *outfile = fopen(filename, "wb");
    if (outfile) {
        /* write the page body to this file handle */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, outfile);

        /* get it! */
        curl_easy_perform(curl_handle);

        /* close the file */
        fclose(outfile);
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    /* ensure file was written */
    return file_exists(filename);
}