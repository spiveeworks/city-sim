#pragma once

#include "util.h"

#define NAME_BUF_CAP 65536
char name_buf[NAME_BUF_CAP];
size_t name_buf_len = 0;

char *alloc_name(const char *in, size_t len) {
    char *out = &name_buf[name_buf_len];
    name_buf_len += len + 1;
    if (name_buf_len > NAME_BUF_CAP) {
        printf("Ran out of room in name buffer\n");
        exit(1);
    }
    memcpy(out, in, len);
    out[len] = '\0';
    return out;
}

#define ITEM_TYPE_CAP 256
struct ItemType {
    char *name;
    struct ItemType *turns_into;
    int live_frames;
    uint8_t color[3];
    bool color_initialized;
} item_types[ITEM_TYPE_CAP];
size_t item_type_count = 0;
typedef struct ItemType *ItemType;

#define FIXTURE_TYPE_CAP 256
struct FixtureType {
    char *name;
    num width;
    num height;
    bool passable;
} fixture_types[FIXTURE_TYPE_CAP];
size_t fixture_type_count = 0;
typedef struct FixtureType *FixtureType;

const FixtureType FIXTURE_CLUTTER = &fixture_types[0];

#define RECIPE_CAP 256
#define RECIPE_INPUT_CAP 8
struct Recipe {
    char *name;
    int duration;
    uint8_t input_count;
    uint8_t output_count;
    ItemType inputs[RECIPE_INPUT_CAP];
    ItemType outputs[RECIPE_INPUT_CAP];
} recipes[RECIPE_CAP];
size_t recipe_count = 0;
typedef struct Recipe *Recipe;

enum TokenType {
    TOKEN_NONE,
    TOKEN_INVALID,
    TOKEN_ALPHA,
    TOKEN_NUM,
    TOKEN_SYMBOL, // unused, but no token greater than this has any alphanum
    TOKEN_EQ,
};

struct Token {
    enum TokenType type;
    char *start;
    char *end;
    // only initialized if type is TOKEN_NUM
    bool negate;
    unsigned whole;
    unsigned nume;
    unsigned denom;
};

void token_get(struct Token *out, char *str) {
    out->start = str;
    out->end = str;
    out->type = TOKEN_NONE;
    while (true) {
        char c = *out->end;
        if (c == '\0' || c == '\n') {
            break;
        } else if (c == ' ' || c == '\t') {
            if (out->type != TOKEN_NONE) {
                break;
            }
            out->start += 1;
        } else if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')) {
            if (out->type == TOKEN_NONE) {
                out->type = TOKEN_ALPHA;
            } else if (out->type == TOKEN_NUM) {
                out->type = TOKEN_INVALID;
            } else if (out->type >= TOKEN_SYMBOL) {
                break;
            }
        } else if ('0' <= c && c <= '9') {
            if (out->type == TOKEN_NONE) {
                out->type = TOKEN_NUM;
                out->negate = false;
                out->whole = c - '0';
                out->nume = 0;
                out->denom = 1;
            } else if (out->type == TOKEN_NUM) {
                out->whole = out->whole * 10 + c - '0';
            } else if (out->type >= TOKEN_SYMBOL) {
                break;
            }
        } else if (c == '=') {
            if (out->type == TOKEN_NONE) {
                out->type = TOKEN_EQ;
            } else {
                break;
            }
        }
        out->end += 1;
    }
    size_t tok_len = out->end - out->start;
}

enum TokenType token_get_type(char **str) {
    struct Token t;
    token_get(&t, *str);
    *str = t.end;
    return t.type;
}

#define token_cmp(t, str) (strncmp((t).start, str, (t).end - (t).start) == 0)

ItemType token_get_item_type(char **str) {
    struct Token t;
    token_get(&t, *str);
    *str = t.end;
    if (t.type != TOKEN_ALPHA) {
        printf("Expected item type\n");
        exit(1);
    }
    range(i, item_type_count) {
        if (token_cmp(t, item_types[i].name)) {
            return &item_types[i];
        }
    }
    printf("Unknown item type \"%s\"\n", alloc_name(t.start, t.end - t.start));
    exit(1);
}

long token_get_num(char **str, long unit) {
    struct Token t;
    token_get(&t, *str);
    *str = t.end;
    if (t.type != TOKEN_NUM) {
        printf("Expected number, got \"%s\"\n", alloc_name(t.start, t.end - t.start));
        exit(1);
    }
    return t.whole * unit + t.nume * unit / t.denom;
}

void parse_data() {
    FILE* f = fopen("data.txt", "r");

    enum {
        PARSE_STATE_NULL,
        PARSE_STATE_ITEM,
        PARSE_STATE_RECIPE,
    } focus = PARSE_STATE_NULL;
    item_type_count = 0;
    recipe_count = 0;

    fixture_type_count = 1;
    if (FIXTURE_CLUTTER - fixture_types >= fixture_type_count) {
        printf("FIXTURE_CLUTTER initialized to bad value\n");
        exit(1);
    }
    FIXTURE_CLUTTER->name = "clutter";
    FIXTURE_CLUTTER->width = 2 * UNIT;
    FIXTURE_CLUTTER->height = 2 * UNIT;
    FIXTURE_CLUTTER->passable = true;

    while (feof(f) == 0) {
        const int STR_CAP = 256;
        char str[STR_CAP];
        fgets(str, STR_CAP, f);
        char *curr = str;
        struct Token t;
        token_get(&t, curr);
        curr = t.end;
        if (t.type == TOKEN_NONE) {
            continue;
        }
        if (t.type != TOKEN_ALPHA) {
            printf("Expected keyword at start of line\n");
            exit(1);
        }
        if (token_cmp(t, "item")) {
            if (item_type_count >= ITEM_TYPE_CAP) {
                printf("Hit item type capacity\n");
                exit(1);
            }
            ItemType out = &item_types[item_type_count];
            item_type_count += 1;
            focus = PARSE_STATE_ITEM;
            token_get(&t, curr);
            curr = t.end;
            if (t.type != TOKEN_ALPHA) {
                printf("Expected name after keyword \"item\"\n");
                exit(1);
            }
            out->name = alloc_name(t.start, t.end - t.start);
            out->turns_into = NULL;
            out->live_frames = -1;
            out->color_initialized = false;
            if (token_get_type(&curr) != TOKEN_NONE) {
                printf("Expected end of line after item declaration\n");
                exit(1);
            }
        } else if (token_cmp(t, "transform")) {
            if (focus != PARSE_STATE_ITEM || item_type_count == 0) {
                printf("Only items can have transformation rules\n");
                exit(1);
            }
            token_get(&t, curr);
            curr = t.end;
            bool into;
            if (token_cmp(t, "from")) {
                into = false;
            } else if (token_cmp(t, "into")) {
                into = true;
            } else {
                printf("Expected \"from\" or \"into\", got \"%s\"\n", alloc_name(t.start, t.end - t.start));
                exit(1);
            }
            ItemType obj = token_get_item_type(&curr);
            token_get(&t, curr);
            curr = t.end;
            if (!token_cmp(t, "after")) {
                printf("Expected \"after\" got \"%s\"\n", alloc_name(t.start, t.end - t.start));
                exit(1);
            }
            int live_frames = (int)token_get_num(&curr, FRAMERATE);
            ItemType from_type;
            ItemType into_type;
            if (into) {
                from_type = &item_types[item_type_count - 1];
                into_type = obj;
            } else {
                into_type = &item_types[item_type_count - 1];
                from_type = obj;
            }
            from_type->turns_into = into_type;
            from_type->live_frames = live_frames;
        } else if (token_cmp(t, "recipe")) {
            if (recipe_count >= RECIPE_CAP) {
                printf("Hit recipe capacity\n");
                exit(1);
            }
            Recipe out = &recipes[recipe_count];
            recipe_count += 1;
            focus = PARSE_STATE_RECIPE;
            token_get(&t, curr);
            curr = t.end;
            if (t.type != TOKEN_ALPHA) {
                printf("Expected name after keyword \"recipe\"\n");
                exit(1);
            }
            out->name = alloc_name(t.start, t.end - t.start);
            out->duration = 0;
            out->input_count = 0;
            out->output_count = 0;
        } else if (token_cmp(t, "duration")) {
            if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
                printf("Only recipes can have durations\n");
                exit(1);
            }
            recipes[recipe_count - 1].duration = token_get_num(&curr, FRAMERATE);
        } else if (token_cmp(t, "input")) {
            if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
                printf("Only recipes can have inputs\n");
                exit(1);
            }
            Recipe out = &recipes[recipe_count - 1];
            if (out->input_count == RECIPE_INPUT_CAP) {
                printf("Hit input capacity (%d)\n", RECIPE_INPUT_CAP);
                exit(1);
            }
            out->inputs[out->input_count] = token_get_item_type(&curr);
            out->input_count += 1;
        } else if (token_cmp(t, "output")) {
            if (focus != PARSE_STATE_RECIPE || recipe_count == 0) {
                printf("Only recipes can have outputs\n");
                exit(1);
            }
            Recipe out = &recipes[recipe_count - 1];
            if (out->output_count == RECIPE_INPUT_CAP) {
                printf("Hit output capacity (%d)\n", RECIPE_INPUT_CAP);
                exit(1);
            }
            out->outputs[out->output_count] = token_get_item_type(&curr);
            out->output_count += 1;
        } else {
            printf("Unknown keyword %s\n", alloc_name(t.start, t.end - t.start));
            exit(1);
        }
        token_get(&t, curr);
        if (t.type != TOKEN_NONE) {
            size_t len = strlen(t.start);
            while (t.start[len-1] == '\r' || t.start[len-1] == '\n') {
                len -= 1;
            }
            printf("Expected end of line, got \"%s\"\n", alloc_name(t.start, strlen(t.start)));
            exit(1);
        }
    }

    fclose(f);
}

