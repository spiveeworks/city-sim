#define FRAMERATE 60

#include "text.h"
#include "graphics.h"

#include "data.h"
#include "sim.h"

#include <time.h>

time_t start_time;

int mouse_x_screen;
int mouse_y_screen;
num mouse_x;
num mouse_y;

size_t selected_char = ~0;

void bright(float c, float *col) {
    if (c < 1.0f) {
        col[0] = 1.0f, col[1] = c, col[2] = 0.0f;
    } else if (c < 2.0f) {
        col[0] = 2.0f-c, col[1] = 1.0f, col[2] = 0.0f;
    } else if (c < 3.0f) {
        col[0] = 0.0f, col[1] = 1.0f, col[2] = c-2.0f;
    } else if (c < 4.0f) {
        col[0] = 0.0f, col[1] = 4.0f-c, col[2] = 1.0f;
    } else if (c < 5.0f) {
        col[0] = c-4.0f, col[1] = 0.0f, col[2] = 1.0f;
    } else {
        col[0] = 1.0f, col[1] = 0.0f, col[2] = 6.0f-c;
    }
}

void circle(struct Vertex* vertex_data, size_t *total,
    float x, float y, float dx, float r, float g, float b)
{
    struct Vertex vs[2][2] =
    {
        {
            {{x-dx, y-dx}, {r, g, b}, {-1.0f, -1.0f}},
            {{x+dx, y-dx}, {r, g, b}, { 1.0f, -1.0f}},
        },
        {
            {{x-dx, y+dx}, {r, g, b}, {-1.0f,  1.0f}},
            {{x+dx, y+dx}, {r, g, b}, { 1.0f,  1.0f}},
        }
    };
    vertex_data[(*total)++] = vs[0][0];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[0][1];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[1][1];
    vertex_data[(*total)++] = vs[0][1];
}

void rect(struct Vertex* vertex_data, size_t *total,
    float xl, float yb, float xr, float yt, float r, float g, float b)
{
    struct Vertex vs[2][2] =
    {
        {
            {{xl, yb}, {r, g, b}, {0.0f, 0.0f}},
            {{xr, yb}, {r, g, b}, {0.0f, 0.0f}}
        },
        {
            {{xl, yt}, {r, g, b}, {0.0f, 0.0f}},
            {{xr, yt}, {r, g, b}, {0.0f, 0.0f}}
        }
    };
    vertex_data[(*total)++] = vs[0][0];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[0][1];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[1][1];
    vertex_data[(*total)++] = vs[0][1];
}

void rect_screen(struct Vertex* vertex_data, size_t *total,
    int xl, int yb, int xr, int yt, float r, float g, float b,
    int screen_width, int screen_height)
{
    float cx = 2.0F / screen_width;
    float cy = 2.0F / screen_height;
    rect(
        vertex_data, total,
        cx * xl - 1.0F, cy * yb - 1.0F, cx * xr - 1.0F, cy * yt - 1.0F,
        r, g, b
    );
}

void line_by_num(
    struct Vertex* vertex_data, size_t *total,
    num x0_num, num y0_num, num x1_num, num y1_num,
    float thickness, float r, float g, float b
) {
    num dx = x1_num - x0_num;
    num dy = y1_num - y0_num;
    num qu = (dx*dx + dy*dy)/UNIT;
    num scale = invsqrt_nr(qu);
    num vx_num = dy*scale/UNIT;
    num vy_num = -dx*scale/UNIT;
    float vx = thickness * (float)vx_num / (float)DIM;
    float vy = -thickness * (float)vy_num / (float)DIM;
    float x0 = (float)x0_num / (float)DIM;
    float y0 = -(float)y0_num / (float)DIM;
    float x1 = (float)x1_num / (float)DIM;
    float y1 = -(float)y1_num / (float)DIM;
    struct Vertex vs[2][2] =
    {
        {
            {{x0-vx, y0-vy}, {r, g, b}, {0.0f, 0.0f}},
            {{x0+vx, y0+vy}, {r, g, b}, {0.0f, 0.0f}},
        },
        {
            {{x1-vx, y1-vy}, {r, g, b}, {0.0f, 0.0f}},
            {{x1+vx, y1+vy}, {r, g, b}, {0.0f, 0.0f}},
        },
    };
    vertex_data[(*total)++] = vs[0][0];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[0][1];
    vertex_data[(*total)++] = vs[1][0];
    vertex_data[(*total)++] = vs[1][1];
    vertex_data[(*total)++] = vs[0][1];
}

void text_line(
    VkImageCopy *sprite_regions, size_t *total,
    size_t str_len, u8 *data,
    int x0, int y0
) {
    VkImageSubresourceLayers subresource = {};
    subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource.mipLevel = 0;
    subresource.baseArrayLayer = 0;
    subresource.layerCount = 1;

    int x = x0;
    range (i, str_len) {
        u8 c = data[i];
        sprite_regions[*total].srcSubresource = subresource;
        sprite_regions[*total].dstSubresource = subresource;
        sprite_regions[*total].srcOffset = (VkOffset3D){0, 0, c};
        sprite_regions[*total].dstOffset =
            (VkOffset3D){x + glyphs[c].bearingx, y0 - glyphs[c].bearingy, 0};
        sprite_regions[*total].extent =
            (VkExtent3D){glyphs[c].width, glyphs[c].height, 1};
        (*total)++;
        x += glyphs[c].advance;
    }
}

#define TEXT_BOX_CAP 128

typedef struct str {
    u8 *data;
    size_t len;
} str;

struct TextBoxInfo {
    int width;
    int height;
    int rows;
    str lines[TEXT_BOX_CAP];
};

void text_box_info(
    str text,
    int width,
    struct TextBoxInfo *info
) {
    int line = 0;
    info->lines[0].data = text.data;
    info->lines[0].len = 0;

    int cursor = 0;
    int max_cursor = 0;
    range (i, text.len) {
        u8 c = text.data[i];
        int advance = glyphs[c].advance;
        if (advance > width) {
            printf("character '%c' is wider than text box (width = %d)\n", c, width);
            exit(1);
        }
        if (c == '\n' || cursor + advance > width) {
            cursor = 0;
            line += 1;
            info->lines[line].data = text.data + i;
            info->lines[line].len = 0;
            // don't wrap whitespace, just skip it
            // @Polish handle " \n" and "\n " and "  " properly
            if (c == '\n' || c == ' ') {
                info->lines[line].data += 1;
                continue;
            }
        }
        info->lines[line].len += 1;
        cursor += advance;
        if (cursor > max_cursor) {
            max_cursor = cursor;
        }
    }

    if (info->lines[line].len == 0) {
        line -= 1;
    }

    info->width = max_cursor;
    info->rows = line + 1;
    info->height = font_texture_range * info->rows;
}

void text_box(
    struct Vertex* vertex_data, size_t *vertex_count,
    VkImageCopy *sprite_regions, size_t *sprite_count,
    str text,
    int max_width,
    int x, int y,
    int frame_thickness,
    int screen_width, int screen_height
) {
    static struct TextBoxInfo info;
    text_box_info(text, max_width, &info);

    y -= info.height;
    x += frame_thickness;
    y -= frame_thickness;

    if (x < 0) x = 0;
    if (y - frame_thickness < 0) y = frame_thickness;
    if (x + info.width  > screen_width ) x = screen_width  - info.width;
    if (y + info.height > screen_height) y = screen_height - info.height;

    range (i, info.rows) {
        text_line(
            sprite_regions, sprite_count,
            info.lines[i].len, info.lines[i].data,
            x, y + i * font_texture_range + font_texture_offset
        );
    }
    rect_screen(
        vertex_data, vertex_count,
        x - frame_thickness,
        y - frame_thickness,
        x + info.width + frame_thickness,
        y + info.height + frame_thickness,
        (float)font_texture_bg.r/255,
        (float)font_texture_bg.g/255,
        (float)font_texture_bg.b/255,
        screen_width, screen_height
    );
}

void build_frame_data(
    size_t *vertex_count,
    struct Vertex* vertex_data,
    size_t *sprite_count,
    VkImageCopy *sprite_regions,
    int screen_width, int screen_height
) {
    size_t total = 0;
    range (i, fixture_count) {
        Fixture fx = live_fixtures[i];
        if (fx->storage_count == 0) { continue; }
        float x = (float)fx->x / (float)DIM;
        float y = -(float)fx->y / (float)DIM;
        float dx = (float)fx->type->width/2 / (float)DIM;
        float dy = (float)fx->type->height/2 / (float)DIM;

        float col[3] = {0.5f, 0.5f, 0.5f};
        ItemType type = fx->storage[0].type;
        if (type->color_initialized) {
            col[0] = (float)type->color[0]/255.0f;
            col[1] = (float)type->color[1]/255.0f;
            col[2] = (float)type->color[2]/255.0f;
        } else {
            bright((float)((type - item_types) % 12)*0.5f, col);
            if ((type - item_types) / 12 % 2) {
                col[0] *= 0.5f;
                col[1] *= 0.5f;
                col[2] *= 0.5f;
            }
        }
        rect(vertex_data, &total, x-dx, y-dy, x+dx, y+dy, col[0], col[1], col[2]);
    }
    float dx = 0.95f/100.0f;
    range (i, char_count) {
        float x = (float)chars[i].x / (float)DIM;
        float y = -(float)chars[i].y / (float)DIM;

        float col[3] = {0.5f, 0.5f, 0.5f};
        bright((float)(i % 12)*0.5f, col);
        if (i / 12 % 2) {
            col[0] *= 0.5f;
            col[1] *= 0.5f;
            col[2] *= 0.5f;
        }
        if (i == selected_char) {
            col[0] = 1.0f;
            col[1] = 1.0f;
            col[2] = 1.0f;
        }

        circle(vertex_data, &total, x, y, dx, col[0], col[1], col[2]);
    }
    range (i, obstacle_count) {
        float xl = (float)obstacles[i].l / (float)DIM;
        float xr = (float)obstacles[i].r / (float)DIM;
        float yb = -(float)obstacles[i].b / (float)DIM;
        float yt = -(float)obstacles[i].t / (float)DIM;
        rect(vertex_data, &total, xl, yb, xr, yt, 1.0F, 1.0F, 1.0F);
    }
    /*
    range (i, nav_node_count) {
        range (j0, nav_adj_counts[i]) {
            size_t j = nav_adj[i][j0].it.i;
            if (i < j) {
                line_by_num(
                    vertex_data, &total,
                    nav_nodes[i].x, nav_nodes[i].y,
                    nav_nodes[j].x, nav_nodes[j].y,
                    0.2F,
                    0.0F, 1.0F, 0.2F
                );
            }
        }
    }
    */

    *sprite_count = 0;

    str text;
    text.data = "Hello world!\nRaspberry Sorbet and Chocolate Fudge Brownies\npgqJj__\n\xC0@@@Jj";
    text.len = strlen(text.data);
    text_box(
        vertex_data, &total,
        sprite_regions, sprite_count,
        text,
        150,
        mouse_x_screen, mouse_y_screen,
        5,
        screen_width, screen_height
    );

    float x = (float)mouse_x/DIM;
    float y = (float)mouse_y/DIM;

    *vertex_count = total;
}

void recordResize(GLFWwindow *window, int width, int height) {
    bool *recreateGraphics = glfwGetWindowUserPointer(window);
    *recreateGraphics = true;
}

const num MOUSE_RANGE = DIM_CTIME / 32;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    double glfw_x;
    double glfw_y;
    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
        ref nearest = find_nearest(mouse_x, mouse_y, MOUSE_RANGE, is_char);
        if (nearest != -1) {
            selected_char = nearest & REF_IND;
        } else {
            selected_char = -1;
        }
    } else if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT) {
    }
}

void mouse_move_callback(GLFWwindow* window, double x, double y) {
    mouse_x_screen = x;
    mouse_y_screen = y;
    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    mouse_x = 2 * DIM * (num)mouse_x_screen / width - DIM;
    mouse_y = 2 * DIM * (num)mouse_y_screen / height - DIM;
}

int main() {
    srand(time(&start_time));

    parse_data();
    printf("Total of %lu item types\n", item_type_count);

    col font_fg = {.r = 20,.g = 20, .b = 255, .a = 255};
    col font_bg = {.r = 20, .g = 20, .b = 20, .a = 255};
    init_text(font_fg, font_bg);

    struct GraphicsInstance gi = createGraphicsInstance();
    struct Graphics g = createGraphics(
        &gi,
        font_texture_width,
        font_texture_height,
        GLYPH_COUNT,
        (u8*)font_texture
    );

    bool recreateGraphics = false;
    glfwSetWindowUserPointer(gi.window, &recreateGraphics);
    glfwSetFramebufferSizeCallback(gi.window, recordResize);
    glfwSetMouseButtonCallback(gi.window, mouse_button_callback);
    glfwSetCursorPosCallback(gi.window, mouse_move_callback);

    init();

    while(!glfwWindowShouldClose(gi.window)) {
        glfwPollEvents();

        frame++;
        if (frame % 600 == 0) {
            printf("reached frame %d (%d seconds)\n", frame, time(NULL)-start_time);
        }

        simulate();

        if (recreateGraphics || !drawFrame(&gi, &g)) {
            int width;
            int height;
            glfwGetFramebufferSize(gi.window, &width, &height);
            while (width == 0 && height == 0) {
                glfwGetFramebufferSize(gi.window, &width, &height);
                glfwWaitEvents();
            }

            destroyGraphics(&gi, &g);
            g = createGraphics(
                &gi,
                font_texture_width,
                font_texture_height,
                GLYPH_COUNT,
                (u8*)font_texture
            );
            recreateGraphics = false;
        }
    }

    destroyGraphics(&gi, &g);
    destroyGraphicsInstance(&gi);

    return 0;
}

