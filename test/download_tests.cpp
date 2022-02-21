#include "catch.hpp"
#include <stdio.h>

extern "C" {
#include "../error.h"
#include "../download.h"
};

TEST_CASE("download file") {
    SECTION("HTTPS with redirect") {
        const char *url = "https://github.com/Homebrew/homebrew-core/archive/refs/heads/master.zip";
        const char *filename = "homebrew-core_master.zip";
        bool downloaded = download_file(url, filename);
        CHECK(downloaded == true);
        if (file_exists(filename)) {
            remove(filename);
        }
    }

}
