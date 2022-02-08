#include "catch.hpp"
#include <string.h>

extern "C" {
#include "../completion_input.h"
#include "../error.h"
};

void setup_env(const char *comp_line, const char *comp_point) {
    setenv(BASH_LINE_VAR, comp_line, 1);
    setenv(BASH_CURSOR_VAR, comp_point, 1);
}

TEST_CASE("completion_input missing ENV vars") {
    SECTION("missing COMP_LINE") {
        setup_env("", "0");
        int retval = load_completion_input();
        CHECK(retval == ERR_MISSING_ENV_COMP_LINE);
    }

    SECTION("missing COMP_POINT") {
        setup_env("xyz", "");
        int retval = load_completion_input();
        CHECK(retval == ERR_MISSING_ENV_COMP_POINT);
    }

    SECTION("bad COMP_POINT") {
        setup_env("xyz", "abc");
        int retval = load_completion_input();
        CHECK(retval != 0);
    }
}

TEST_CASE("completion_input") {
    const char *line = "kubectl get pods -o wide";
    const char *cursor = "7";
    setup_env(line, cursor);

    SECTION("load completion_input") {
        int retval = load_completion_input();
        CHECK(retval == 0);
    }

    SECTION("line value") {
        CHECK(strlen(completion_input.line) == strlen(line));
    }

    SECTION("cursor value") {
        CHECK(completion_input.cursor_pos == strtol(cursor, (char **)NULL, 10));
    }
}

TEST_CASE("get_command_from_input") {
    const size_t BUF_SIZE = 1024;
    const char *line = "kubectl get pods -o wide";
    setenv(BASH_LINE_VAR, line, 1);
    int retval = load_completion_input();
    CHECK(retval == 0);

    char cmd[BUF_SIZE];
    bool result = get_command_from_input(cmd, BUF_SIZE);
    CHECK(result == true);
    CHECK(strncmp("kubectl", cmd, BUF_SIZE) == 0);
}

TEST_CASE("current_word") {
    const size_t BUF_SIZE = 1024;
    const char *line = "kubectl get pods -o wide";
    setenv(BASH_LINE_VAR, line, 1);

    SECTION("cursor at 1st word") {
        const char *cursor = "7";
        setup_env(line, cursor);
        int retval = load_completion_input();
        CHECK(retval == 0);

        char word[BUF_SIZE];
        CHECK(get_current_word(word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(word, BUF_SIZE) == false);
        CHECK(strlen(word) == 0);
    }

    SECTION("cursor after 1st word") {
        const char *cursor = "8";
        setup_env(line, cursor);
        int retval = load_completion_input();
        CHECK(retval == 0);

        char word[BUF_SIZE];
        CHECK(get_current_word(word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(word, BUF_SIZE) == false);
        CHECK(strlen(word) == 0);
    }

    SECTION("cursor at 2nd word") {
        const char *cursor = "11";
        setup_env(line, cursor);
        int retval = load_completion_input();
        CHECK(retval == 0);

        char word[BUF_SIZE];
        CHECK(get_current_word(word, BUF_SIZE) == true);
        CHECK(strncmp("get", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);
    }

    SECTION("cursor after 2nd word") {
        const char *cursor = "12";
        setup_env(line, cursor);
        int retval = load_completion_input();
        CHECK(retval == 0);

        char word[BUF_SIZE];
        CHECK(get_current_word(word, BUF_SIZE) == true);
        CHECK(strncmp("get", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);
    }

    SECTION("cursor at middle of 1st word") {
        const char *cursor = "3";
        setup_env(line, cursor);
        int retval = load_completion_input();
        CHECK(retval == 0);

        char word[BUF_SIZE];
        CHECK(get_current_word(word, BUF_SIZE) == true);
        CHECK(strncmp("kub", word, BUF_SIZE) == 0);
    }

}