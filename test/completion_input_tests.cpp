#include "catch.hpp"
#include <string.h>

extern "C" {
#include "../input.h"
#include "../error.h"
};

void setup_env(const char *comp_line, const char *comp_point) {
    setenv(BASH_LINE_VAR, comp_line, 1);
    setenv(BASH_CURSOR_VAR, comp_point, 1);
}

TEST_CASE("completion_input missing ENV vars") {
    SECTION("missing COMP_LINE") {
        setup_env("", "0");
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_MISSING_ENV_COMP_LINE);
        free_completion_input(input);
    }

    SECTION("missing COMP_POINT") {
        setup_env("xyz", "");
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_MISSING_ENV_COMP_POINT);
        free_completion_input(input);
    }

    SECTION("bad COMP_POINT") {
        setup_env("xyz", "abc");
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err != ERR_NONE);
        free_completion_input(input);
    }
}

TEST_CASE("completion_input") {
    const char *line = "kubectl get pods -o wide";
    const char *cursor = "7";
    setup_env(line, cursor);

    SECTION("load completion_input") {
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);
        free_completion_input(input);
    }

    SECTION("line value") {
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(strlen(input->line) == strlen(line));
        free_completion_input(input);
    }

    SECTION("cursor value") {
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(input->cursor_pos == strtol(cursor, (char **) NULL, 10));
        free_completion_input(input);
    }
}

TEST_CASE("get_command_from_input") {
    const size_t BUF_SIZE = 1024;
    const char *line = "kubectl get pods -o wide";
    setenv(BASH_LINE_VAR, line, 1);
    bce_error_t err;
    completion_input_t *input = create_completion_input(&err);
    CHECK(err == ERR_NONE);

    char cmd[BUF_SIZE];
    bool result = get_command_from_input(input, cmd, BUF_SIZE);
    CHECK(result == true);
    CHECK(strncmp("kubectl", cmd, BUF_SIZE) == 0);

    free_completion_input(input);
}

TEST_CASE("current_word") {
    const size_t BUF_SIZE = 1024;
    const char *line = "kubectl get pods -o wide";
    setenv(BASH_LINE_VAR, line, 1);

    SECTION("cursor at 1st word") {
        const char *cursor = "7";
        setup_env(line, cursor);
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);

        char word[BUF_SIZE];
        CHECK(get_current_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(input, word, BUF_SIZE) == false);
        CHECK(strlen(word) == 0);

        free_completion_input(input);
    }

    SECTION("cursor after 1st word") {
        const char *cursor = "8";
        setup_env(line, cursor);
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);

        char word[BUF_SIZE];
        CHECK(get_current_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(input, word, BUF_SIZE) == false);
        CHECK(strlen(word) == 0);

        free_completion_input(input);
    }

    SECTION("cursor at 2nd word") {
        const char *cursor = "11";
        setup_env(line, cursor);
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);

        char word[BUF_SIZE];
        CHECK(get_current_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("get", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        free_completion_input(input);
    }

    SECTION("cursor after 2nd word") {
        const char *cursor = "12";
        setup_env(line, cursor);
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);

        char word[BUF_SIZE];
        CHECK(get_current_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("get", word, BUF_SIZE) == 0);

        CHECK(get_previous_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("kubectl", word, BUF_SIZE) == 0);

        free_completion_input(input);
    }

    SECTION("cursor at middle of 1st word") {
        const char *cursor = "3";
        setup_env(line, cursor);
        bce_error_t err;
        completion_input_t *input = create_completion_input(&err);
        CHECK(err == ERR_NONE);

        char word[BUF_SIZE];
        CHECK(get_current_word(input, word, BUF_SIZE) == true);
        CHECK(strncmp("kub", word, BUF_SIZE) == 0);

        free_completion_input(input);
    }

}