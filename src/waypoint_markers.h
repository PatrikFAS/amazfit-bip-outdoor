#ifndef BIP_OUTDOOR_WAYPOINT_MARKERS_H
#define BIP_OUTDOOR_WAYPOINT_MARKERS_H

#include <libbip.h>
#include <stdint.h>

/* Project 32 clockwise directions onto the square perimeter. North starts
 * at slot zero. All markers use the same inset from the edge. */
static inline int edge_slot(int bearing, int heading)
{
    return (((bearing - heading + 360) * 32 + 180) / 360) & 31;
}

static inline void edge_vector(int slot, int span, int *x, int *y)
{
    static const uint8_t east[] = {0, 13, 27, 43, 64, 64, 64, 64, 64, 64, 64, 64, 64, 43, 27, 13};
    int dx = (east[slot & 15] * span + 32) >> 6;
    int dy = (east[(slot + 8) & 15] * span + 32) >> 6;
    *x = slot < 16 ? dx : -dx;
    *y = slot >= 8 && slot < 24 ? dy : -dy;
}

static inline void draw_edge_line(int x0, int y0, int x1, int y1)
{
    /* Use 16 samples for edge glyphs and 64 for longer compass strokes. */
    int dx = x1 - x0, dy = y1 - y0;
    int stride = (unsigned)(dx + 16) <= 32 && (unsigned)(dy + 16) <= 32 ? 4 : 1;
    int x = x0 * 64 + 32, y = y0 * 64 + 32;
    dx *= stride;
    dy *= stride;
    for (int step = 0; step <= 64; step += stride) {
        int px = x >> 6, py = y >> 6;
        draw_filled_rect(px, py, px + 1, py + 1);
        x += dx;
        y += dy;
    }
}

/* The shaft and arrowhead fit within the same square for either size. */
static inline void draw_direction_arrow(int slot, int x, int y, int radius, int color)
{
    int dx, dy;
    edge_vector(slot, radius, &dx, &dy);
    int tip_x = x + dx, tip_y = y + dy;
    int half_x = dx / 2, half_y = dy / 2;
    set_fg_color(color);
    draw_edge_line(x - dx, y - dy, tip_x, tip_y);
    draw_edge_line(tip_x, tip_y, x + half_x - half_y, y + half_y + half_x);
    draw_edge_line(tip_x, tip_y, x + half_x + half_y, y + half_y - half_x);
}

static inline void draw_north_badge(int x, int y)
{
    set_fg_color(COLOR_RED);
    draw_filled_rect(x - 7, y - 3, x + 7, y + 3);
    draw_filled_rect(x - 5, y - 5, x + 5, y + 5);
    draw_filled_rect(x - 3, y - 7, x + 3, y + 7);
    set_fg_color(COLOR_WHITE);
    draw_filled_rect(x - 4, y - 4, x - 3, y + 4);
    draw_filled_rect(x + 3, y - 4, x + 4, y + 4);
    draw_edge_line(x - 3, y - 4, x + 3, y + 4);
}

static inline void draw_edge_arrow(int direction_slot, int position_slot, int color)
{
    int x, y;
    edge_vector(position_slot, 79, &x, &y);
    draw_direction_arrow(direction_slot, x + 87, y + 87, 7, color);
}

static inline int edge_free_slot(int slot, int first_used, int second_used)
{
    /* At most two slots are occupied. Preserve the arrow's original tilt. */
    while (slot == first_used || slot == second_used) slot = (slot + 1) & 31;
    return slot;
}

static inline void draw_edge_markers(int bearing, int course, int heading)
{
    int x, y;
    int north = edge_slot(0, heading);
    edge_vector(north, 79, &x, &y);
    x += 87;
    y += 87;
    draw_north_badge(x, y);

    int target = -1;
    if (bearing >= 0) {
        int direction = edge_slot(bearing, heading);
        target = edge_free_slot(direction, north, -1);
        draw_edge_arrow(direction, target, COLOR_GREEN);
    }
    if (course >= 0) {
        int travel = edge_slot(course, heading);
        int position = edge_free_slot(travel, north, target);
        draw_edge_arrow(travel, position, COLOR_YELLOW);
    }
}

#endif
