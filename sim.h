#pragma once

#include "util.h"
#include "data.h"
#include "nav.h"

int frame = 0;

// @Robustness why do large IDIM values cause a segfault?
#define IDIM 50
#define DIM_CTIME (UNIT_CTIME * IDIM)
const num DIM = DIM_CTIME;

struct Item {
    int change_frame;
    ItemType type;
};
typedef struct Item *Item;


#define CHAR_CAP (IDIM * IDIM / 4)
//#define CHAR_INITIAL (IDIM * IDIM / 512)
#define CHAR_INITIAL 20

#define REACH (UNIT)
#define MAX_HEALTH 60

size_t char_count = 0;

struct Char {
    // int deadframe;
    Recipe goal;
    num craft_x, craft_y;
    int craft_t;
    uint8_t input_count;
    long inputs[RECIPE_INPUT_CAP];
    struct Item held_item;

    num x, y;
    num velx, vely;
    size_t path_count;
    long next_nav_frame;
    num endx, endy;
} chars[CHAR_CAP];

nav char_paths[CHAR_CAP][NAV_NODE_CAP];

#define FIXTURE_CAP (IDIM * IDIM / 16)
#define ITEM_INITIAL (IDIM * IDIM / 64)

#define STORAGE_CAP 1

struct Fixture {
    num x, y;
    FixtureType type;
    int change_frame;
    int storage_count;
    struct Item storage[STORAGE_CAP];
} fixtures[FIXTURE_CAP];

typedef struct Fixture *Fixture;

size_t fixture_count = 0;
Fixture live_fixtures[FIXTURE_CAP];

#define AWARENESS (UNIT_CTIME * 32)

#define CHUNK_SIZE (UNIT_CTIME * 16)
#define CHUNK_DIM (2 * DIM_CTIME / CHUNK_SIZE + 1)
#define CHUNK_BUFFER_SIZE 1024

typedef uint32_t ref;
#define REF_SHIFT 31
const ref REF_CHAR = 0u<<REF_SHIFT;
const ref REF_FIXTURE = 1u<<REF_SHIFT;
const ref REF_SORT = ~0u<<REF_SHIFT;
const ref REF_IND = ~(~0u<<REF_SHIFT);

struct Chunk {
    long total_num;
    ref refs[CHUNK_BUFFER_SIZE];
} chunks[CHUNK_DIM][CHUNK_DIM];

#define get_chunk(x) (((x)+DIM)/CHUNK_SIZE)

bool chunk_remove(struct Chunk *chunk, ref r) {
    range (i, chunk->total_num) {
        if (chunk->refs[i] == r) {
            chunk->total_num--;
            chunk->refs[i] = chunk->refs[chunk->total_num];
            chunk->refs[chunk->total_num] = 0;
            return true;
        }
    }
    return false;
}

void chunk_remove_char(long i) {
    struct Char c = chars[i];
    size_t ci = get_chunk(c.x);
    size_t cj = get_chunk(c.y);
    if (!chunk_remove(&chunks[ci][cj], i | REF_CHAR)) {
        printf("WARNING: char %ld not removed from chunk %lu, %lu\n", i, ci, cj);
    }
}

void chunk_remove_fixture(long i) {
    struct Fixture c = fixtures[i];
    size_t ci = get_chunk(c.x);
    size_t cj = get_chunk(c.y);
    if (!chunk_remove(&chunks[ci][cj], i | REF_FIXTURE)) {
        printf("WARNING: fixture %ld not removed from chunk %lu, %lu\n", i, ci, cj);
    }
}

int high_water = 0;

void chunk_add_char(long i) {
    struct Char c = chars[i];
    size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
    struct Chunk *chunk = &chunks[ci][cj];
    if (chunk->total_num == CHUNK_BUFFER_SIZE) {
        printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
        exit(1);
    }
    chunk->refs[chunk->total_num++] = i | REF_CHAR;
    high_water = max(high_water, chunk->total_num);
}
void chunk_add_fixture(long i) {
    struct Fixture c = fixtures[i];
    size_t ci = get_chunk(c.x), cj = get_chunk(c.y);
    struct Chunk *chunk = &chunks[ci][cj];
    if (chunk->total_num == CHUNK_BUFFER_SIZE) {
        printf("ERROR: chunk %lu, %lu is full\n", ci, cj);
        exit(1);
    }
    chunk->refs[chunk->total_num++] = i | REF_FIXTURE;
    high_water = max(high_water, chunk->total_num);
}

size_t create_fixture(num x, num y, struct Item it) {
    if (fixture_count == FIXTURE_CAP) {
        printf("Reached fixture capacity\n");
        exit(1);
    }
    size_t i = 0;
    {
        while (i < fixture_count && live_fixtures[i] == &fixtures[i]) {
            i += 1;
        }
        size_t j = i;
        Fixture insert = &fixtures[i];
        fixture_count += 1;
        while (j < fixture_count) {
            Fixture tmp = live_fixtures[j];
            live_fixtures[j] = insert;
            insert = tmp;
            j += 1;
        }
    }
    Fixture fx = &fixtures[i];
    fx->x = x;
    fx->y = y;
    fx->type = FIXTURE_CLUTTER;
    fx->storage_count = 1;
    fx->storage[0] = it;
    chunk_add_fixture(i);
    return i;
}

void destroy_fixture(long fx_i) {
    Fixture fx = &fixtures[fx_i];
    chunk_remove_fixture(fx_i);
    fx->type = NULL;
    size_t i = 0;
    while (i < fixture_count && live_fixtures[i] < fx) {
        i++;
    }
    if (live_fixtures[i] != fx) {
        printf("WARNING: fixture %ld destroyed while not live\n", fx_i);
        return;
    }
    fixture_count -= 1;
    while (i < fixture_count) {
        live_fixtures[i] = live_fixtures[i+1];
        i++;
    }
}

void rand_pos_in_space(num *out_x, num *out_y) {
    bool done = false;
    while (!done) {
        const int g = 1000; // granularity of randomness
        const num RANGE = DIM - 1;
        num x = rand_int(g) * RANGE / g;
        num y = rand_int(g) * RANGE / g;
        *out_x = x;
        *out_y = y;
        done = true;
        range(o, obstacle_count) {
            if (obstacles[o].l < x && x < obstacles[o].r &&
                    obstacles[o].b < y && y < obstacles[o].t
            ) {
                done = false;
            }
        }
    }
}

#define OBSTACLE_INITIAL 100

void init() {
    fixture_count = 0;
    char_count = 0;
    range (i, CHUNK_DIM) {
        range (j, CHUNK_DIM) {
            chunks[i][j].total_num = 0;
            range(k, CHUNK_BUFFER_SIZE) {
                chunks[i][j].refs[k] = 0;
            }
        }
    }

    obstacle_count = 0;

    range (i, OBSTACLE_INITIAL) {
        const int g = 1000; // granularity of randomness
        const num SPACING = DIM/10;
        const num RANGE = DIM/20;
        num x = -DIM + i%10*2*SPACING + rand_int(g)*RANGE/g;
        num y = -DIM + i*2*DIM/OBSTACLE_INITIAL + rand_int(g)*RANGE/g;
        num dx = DIM / 100;
        num dy = DIM / 5;
        if (rand() % 2) {
            dx = DIM / 5;
            dy = DIM / 100;
        }
        obstacles[i].l = x - dx;
        obstacles[i].r = x + dx;
        obstacles[i].b = y - dy;
        obstacles[i].t = y + dy;
        obstacle_count += 1;
    }

    initialize_nav_edges(-DIM, DIM, -DIM, DIM);

    range (i, ITEM_INITIAL) {
        const int g = 1000; // granularity of randomness
        const num RANGE = DIM - 1;
        struct Item it;
        it.type = &item_types[i % item_type_count];
        it.change_frame = it.type->live_frames;
        num x, y;
        rand_pos_in_space(&x, &y);
        create_fixture(x, y, it);
    }
    range (i, CHAR_INITIAL) {
        rand_pos_in_space(&chars[i].x, &chars[i].y);

        chars[i].velx = 0;
        chars[i].vely = 0;
        chars[i].path_count = 0;
        chars[i].next_nav_frame = -1;
        chars[i].endx = chars[i].x;
        chars[i].endy = chars[i].y;

        chars[i].held_item.type = NULL;
        chars[i].input_count = 0;
        chars[i].goal = &recipes[i % recipe_count];
        char_count++;
        chunk_add_char(i);
    }
    printf("Spread %d characters, %d fixtures across %d chunks, highest was %d in one chunk\n", char_count, fixture_count, CHUNK_DIM*CHUNK_DIM, high_water);
}

ref find_nearest(num x, num y, num r, bool (*cond)(ref x)) {
    size_t cl = get_chunk(max(x - r, 1-DIM));
    size_t cr = get_chunk(min(x + r, DIM-1));
    size_t cu = get_chunk(max(y - r, 1-DIM));
    size_t cd = get_chunk(min(y + r, DIM-1));
    ref nearest = -1;
    long nearestqu = r * r;
    for (int di = cl; di <= cr; di++) {
        for (int dj = cu; dj <= cd; dj++) {
            struct Chunk *chunk = &chunks[di][dj];
            range(k, chunk->total_num) {
                ref r = chunk->refs[k];
                if (!cond(r)) continue;
                num itx, ity;
                if ((r & REF_SORT) == REF_CHAR) {
                    itx = chars[r & REF_IND].x;
                    ity = chars[r & REF_IND].y;
                } else if ((r & REF_SORT) == REF_FIXTURE) {
                    itx = fixtures[r & REF_IND].x;
                    ity = fixtures[r & REF_IND].y;
                } else {
                    printf("Chunk contained unknown ref %x\n", r);
                    exit(1);
                }
                num dx = itx - x;
                num dy = ity - y;
                num qu = dx*dx + dy*dy;
                if (qu < nearestqu) {
                    nearest = r;
                    nearestqu = qu;
                }
            }
        }
    }
    return nearest;
}

bool is_char(ref x) { return (x & REF_SORT) == REF_CHAR; }
bool is_fixture(ref x) { return (x & REF_SORT) == REF_FIXTURE; }

size_t is_valid_input_current_char;
bool is_valid_input(ref x) {
    size_t i = is_valid_input_current_char;
    if ((x & REF_SORT) != REF_FIXTURE) {
        return false;
    }
    x &= REF_IND;
    Fixture it = &fixtures[x];
    size_t input_count = chars[i].input_count;
    range (inp, input_count) {
        if (chars[i].inputs[inp] == x) {
            return false;
        }
    }
    ItemType goal = chars[i].goal->inputs[input_count];
    range(i, it->storage_count) {
        if (it->storage[i].type == goal) {
            return true;
        }
    }
    return false;
}

void simulate() {
    // item evolution
    range (i, char_count) {
        Item it = &chars[i].held_item;
        if (it->type != NULL && 0 <= it->change_frame && it->change_frame <= frame)
        {
            ItemType into = it->type->turns_into;
            if (into == NULL || into->live_frames == -1) {
                it->change_frame = -1;
            } else {
                it->change_frame += it->type->live_frames;
            }
            it->type = into;
        }
    }
    range (i, fixture_count) {
        Fixture fx = live_fixtures[i];
        range (j, fx->storage_count) {
            Item it = &fx->storage[j];
            if (it->type != NULL && 0 <= it->change_frame && it->change_frame <= frame)
            {
                ItemType into = it->type->turns_into;
                if (into == NULL) {
                    fx->storage[j] = fx->storage[fx->storage_count - 1];
                    fx->storage_count -= 1;
                    j -= 1;
                } else if (into->live_frames = -1) {
                    it->change_frame = -1;
                } else {
                    it->change_frame += it->type->live_frames;
                }
                it->type = into;
            }
        }
        if (fx->type == FIXTURE_CLUTTER && fx->storage_count == 0) {
            destroy_fixture(fx - fixtures);
            i -= 1;
        }
    }

    // decision making
    range (i, char_count) {
        // only make decisions when not currently walking somewhere
        // @Polish keep track of target item to see if goal has been
        // undermined? eventually there will be explicit rules for tracking and
        // locating though, maybe just keep it as is until then
        if (chars[i].next_nav_frame >= 0) {
            continue;
        }
        Recipe goal = chars[i].goal;
        if (goal == NULL) {
            continue;
        }
        if (goal->input_count == 0) {
            printf("Recipes without inputs currently not supported\n");
            exit(1);
        } else if (chars[i].held_item.type != NULL) {
            if (chars[i].input_count == 0
                || chars[i].input_count >= goal->input_count
                || goal->inputs[chars[i].input_count] != chars[i].held_item.type
            ) {
                create_fixture(chars[i].x, chars[i].y, chars[i].held_item);
                chars[i].held_item.type = NULL;
                chars[i].held_item.change_frame = -1;
            } else {
                num dx = chars[i].craft_x - chars[i].x;
                num dy = chars[i].craft_y - chars[i].y;
                num qu = dx * dx + dy * dy;
                if (qu < REACH * REACH) {
                    long it = create_fixture(chars[i].x, chars[i].y, chars[i].held_item);
                    chars[i].held_item.type = NULL;
                    chars[i].held_item.change_frame = -1;
                    bool stolen = false;
                    range (j, chars[i].input_count - 1) {
                        if (chars[i].inputs[j] == it) {
                            chars[i].input_count = j;
                            stolen = true;
                        }
                    }
                    if (!stolen) {
                        chars[i].inputs[chars[i].input_count] = it;
                        chars[i].input_count += 1;
                        chars[i].craft_t = frame + goal->duration;
                    }
                } else {
                    chars[i].endx = chars[i].craft_x;
                    chars[i].endy = chars[i].craft_y;
                    chars[i].next_nav_frame = frame;
                }
            }
        } else if (chars[i].input_count < goal->input_count) {
            is_valid_input_current_char = i;
            ref target = find_nearest(chars[i].x, chars[i].y, AWARENESS, is_valid_input);
            if (target == -1) {
                continue;
            }
            target &= REF_IND;
            num dx = fixtures[target].x - chars[i].x;
            num dy = fixtures[target].y - chars[i].y;
            num qu = dx * dx + dy * dy;
            Fixture fx = &fixtures[target];
            if (qu < REACH * REACH || (chars[i].input_count == 0 && goal->input_count > 1)) {
                if (chars[i].input_count == 0) {
                    chars[i].inputs[chars[i].input_count] = target;
                    chars[i].input_count += 1;
                    chars[i].craft_x = fx->x;
                    chars[i].craft_y = fx->y;
                    chars[i].craft_t = frame + goal->duration;
                } else {
                    bool worked = false;
                    range(j, fx->storage_count) {
                        if (fx->storage[j].type != goal->inputs[chars[i].input_count]) {
                            continue;
                        }
                        chars[i].held_item = fx->storage[j];
                        fx->storage[j] = fx->storage[fx->storage_count - 1];
                        fx->storage_count -= 1;
                        if (fx->type == FIXTURE_CLUTTER && fx->storage_count == 0) {
                            destroy_fixture(target);
                        }
                        worked = true;
                        break;
                    }
                    if (!worked) {
                        printf("Chosen fixture did not contain desired item?\n");
                        exit(1);
                    }
                }
            } else {
                chars[i].endx = fixtures[target].x;
                chars[i].endy = fixtures[target].y;
                chars[i].next_nav_frame = frame;
            }
        } else {
            bool stolen = false;
            range(inp, chars[i].input_count) {
                Fixture fx = &fixtures[chars[i].inputs[inp]];
                num dx = fx->x - chars[i].craft_x;
                num dy = fx->y - chars[i].craft_y;
                num qu = dx * dx + dy * dy;
                if (
                    fx->type != FIXTURE_CLUTTER
                    || fx->storage_count == 0
                    || fx->storage[0].type != goal->inputs[inp]
                    || qu > REACH * REACH
                ) {
                    stolen = true;
                    chars[i].input_count = inp;
                    break;
                }
            }
            if (!stolen && frame >= chars[i].craft_t) {
                range(inp, chars[i].input_count) {
                    destroy_fixture(chars[i].inputs[inp]);
                }
                chars[i].input_count = 0;
                range(out, goal->output_count) {
                    long j = 0;
                    struct Item it;
                    it.type = goal->outputs[out];
                    int live_frames = goal->outputs[out]->live_frames;
                    if (live_frames == -1) {
                        it.change_frame = -1;
                    } else {
                        it.change_frame = frame + live_frames;
                    }
                    create_fixture(chars[i].craft_x, chars[i].craft_y, it);
                }
            }
        }
    }

    // navigation
    range (i, char_count) {
        if (chars[i].next_nav_frame < 0 || frame < chars[i].next_nav_frame) {
            continue;
        }
        bool nextpos_chosen = false;
        num nextx;
        num nexty;
        if (chars[i].path_count == 0) {
            if (chars[i].velx == 0 && chars[i].vely == 0) {
                if (chars[i].x == chars[i].endx && chars[i].y == chars[i].endy) {
                    chars[i].next_nav_frame = -1;
                } else if (interval_obstructed(
                    chars[i].x, chars[i].y,
                    chars[i].endx, chars[i].endy
                )) {
                    pick_route(
                        chars[i].x, chars[i].y, chars[i].endx, chars[i].endy,
                        &chars[i].path_count, char_paths[i]
                    );
                    if (chars[i].path_count == 0) {
                        chars[i].next_nav_frame = -1;
                    } else {
                        nav next = char_paths[i][chars[i].path_count - 1];
                        nextx = nav_nodes[next.i].x;
                        nexty = nav_nodes[next.i].y;
                        nextpos_chosen = true;
                    }
                } else {
                    nextx = chars[i].endx;
                    nexty = chars[i].endy;
                    nextpos_chosen = true;
                }
            } else {
                chars[i].endx = chars[i].x;
                chars[i].endy = chars[i].y;
                chars[i].velx = 0;
                chars[i].vely = 0;
                chars[i].next_nav_frame = -1;
            }
        } else {
            nav curr = char_paths[i][chars[i].path_count - 1];
            chars[i].path_count -= 1;
            chunk_remove_char(i);
            chars[i].x = nav_nodes[curr.i].x;
            chars[i].y = nav_nodes[curr.i].y;
            chunk_add_char(i);
            if (chars[i].path_count == 0) {
                nextx = chars[i].endx;
                nexty = chars[i].endy;
            } else {
                nav next = char_paths[i][chars[i].path_count - 1];
                nextx = nav_nodes[next.i].x;
                nexty = nav_nodes[next.i].y;
            }
            nextpos_chosen = true;
        }
        if (nextpos_chosen) {
            num dx = nextx - chars[i].x;
            num dy = nexty - chars[i].y;
            num qu = (dx*dx+dy*dy)/UNIT;
            num scale = invsqrt_nr(qu);
            num dist = qu*scale/UNIT;
            const num SPEED = UNIT/4;
            chars[i].velx = dx*scale/UNIT*SPEED/UNIT;
            chars[i].vely = dy*scale/UNIT*SPEED/UNIT;
            chars[i].next_nav_frame = frame + dist / SPEED;
        }
    }

    // movement
    range (i, char_count) {
        num x = chars[i].x;
        num y = chars[i].y;
        num newx = x + chars[i].velx;
        num newy = y + chars[i].vely;
        bool same_chunk = get_chunk(x) == get_chunk(newx)
            && get_chunk(y) == get_chunk(newy);
        if (!same_chunk) {
            chunk_remove_char(i);
            if (newx >= DIM) {
                newx = DIM-1;
            } else if (newx <= -DIM) {
                newx = -DIM+1;
            }
            if (newy >= DIM) {
                newy = DIM-1;
            } else if (newy <= -DIM) {
                newy = -DIM+1;
            }
        }
        chars[i].x = newx;
        chars[i].y = newy;
        if (!same_chunk) {
            chunk_add_char(i);
        }
    }
}

