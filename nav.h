#pragma once

#include "util.h"

#define OBSTACLE_CAP 256
struct Obstacle {
	num l,r,t,b; // l < r && b < t
} obstacles[OBSTACLE_CAP];
size_t obstacle_count = 0;
typedef struct Obstacle *Obstacle;

typedef struct nav { size_t i; } nav;

#define NAV_NODE_CAP (OBSTACLE_CAP * 4)
struct NavNode {
	num x, y;
} nav_nodes[NAV_NODE_CAP];
size_t nav_node_count = 0;

#define NAV_PATH_CAP (NAV_NODE_CAP * (NAV_NODE_CAP - 1) / 2)
struct NavPath {
	nav prev; // fastest node to get from i to j (i < j)
	nav next; // fastest node to get from j to i (i < j)
	num distance; // distance of either of these paths - assumed symmetry
} nav_paths[NAV_PATH_CAP];
// nav_path_count == (nav_node_count * nav_node_count - nav_node_count) / 2
// when i < j: nav_paths[i, j] = nav_paths[(j*j-j)/2 + i]

bool rect_intersects_interval(
	num l, num r, num b, num t,
	num x0, num y0, num x1, num y1
) {
	// is the interval incident with the _extension_ of the edges
	bool lline = (x0 < l && l < x1) || (x1 < l && l < x0);
	bool rline = (x0 < r && r < x1) || (x1 < r && r < x0);
	bool bline = (y0 < b && b < y1) || (y1 < b && b < y0);
	bool tline = (y0 < t && t < y1) || (y1 < t && t < y0);

	num dx = x1 - x0;
	num dy = y1 - y0;

	num bx, tx, ly, ry;

	// recall points on line will satisfy
	// (x - x0)(y1 - y0) = (y - y0)(x1 - x0)
	// avoid expanding brackets, to prevent overflow
	// x = x0 + (y - y0)(x1 - x0)/(y1 - y0)
	// y = y0 + (x - x0)(y1 - y0)/(x1 - x0)
	if (dy != 0) {
		bx = x0 + (b - y0)*dx/dy;
		tx = x0 + (t - y0)*dx/dy;
		if (bline && l < bx && bx < r) { return true; }
		if (tline && l < tx && tx < r) { return true; }
	}
	if (dx != 0) {
		ly = y0 + (l - x0)*dy/dx;
		ry = y0 + (r - x0)*dy/dx;
		if (lline && b < ly && ly < t) { return true; }
		if (rline && b < ry && ry < t) { return true; }
	}

	// edge case for passing through the corner of a rectangle, since this will
	// appear to 'glance' both edges
	// this test also makes us much more robust to numerical error since this
	// test doesn't care at all where the intersection is relative to the
	// corner being glanced, only where it is relative to the opposite sides of
	// the rectangle
	if (dx != 0 && dy != 0) {
		if (lline && bline && dx * dy > 0 && ly < t && bx < r) { return true; }
		if (rline && bline && dx * dy < 0 && ry < t && bx > l) { return true; }
		if (lline && tline && dx * dy < 0 && ly > b && tx < r) { return true; }
		if (rline && tline && dx * dy > 0 && ry > b && tx > l) { return true; }
	}

	return false;
}

void initialize_nav_edges() {
	// just hardcode the rectangles while we get the nav system working
	obstacles[obstacle_count].l = -100*UNIT;
	obstacles[obstacle_count].r = -50*UNIT;
	obstacles[obstacle_count].b = -50*UNIT;
	obstacles[obstacle_count].t = 0*UNIT;
	obstacle_count += 1;
	obstacles[obstacle_count].l = 50*UNIT;
	obstacles[obstacle_count].r = 100*UNIT;
	obstacles[obstacle_count].b = 10*UNIT;
	obstacles[obstacle_count].t = 60*UNIT;
	obstacle_count += 1;
	obstacles[obstacle_count].l = 0*UNIT;
	obstacles[obstacle_count].r = 10*UNIT;
	obstacles[obstacle_count].b = 0*UNIT;
	obstacles[obstacle_count].t = 10*UNIT;
	obstacle_count += 1;


	nav_node_count = obstacle_count * 4;
	range(i, obstacle_count) {
		nav_nodes[4*i+0].x = obstacles[i].r;
		nav_nodes[4*i+0].y = obstacles[i].t;
		nav_nodes[4*i+1].x = obstacles[i].l;
		nav_nodes[4*i+1].y = obstacles[i].t;
		nav_nodes[4*i+2].x = obstacles[i].l;
		nav_nodes[4*i+2].y = obstacles[i].b;
		nav_nodes[4*i+3].x = obstacles[i].r;
		nav_nodes[4*i+3].y = obstacles[i].b;
	}
	size_t path_count = nav_node_count * (nav_node_count - 1) / 2;
	range(ind, path_count) {
		nav_paths[ind].next.i = ~0U;
		nav_paths[ind].distance = NUM_GREATEST;
	}

	range(j, nav_node_count) {
		range(i, j) {
			size_t ind = (j*j-j)/2 + i;
			bool clear = true;
			range(o, obstacle_count) {
				if (rect_intersects_interval(
					obstacles[o].l, obstacles[o].r, obstacles[o].b, obstacles[o].t,
					nav_nodes[i].x, nav_nodes[i].y, nav_nodes[j].x, nav_nodes[j].y
				)) {
					clear = false;
				}
			}
			if (clear) {
				nav_paths[ind].next.i = j;
				nav_paths[ind].prev.i = i;
				num dx = nav_nodes[j].x - nav_nodes[i].x;
				num dy = nav_nodes[j].y - nav_nodes[i].y;
				nav_paths[ind].distance = num_hypot(dx, dy);
			}
		}
	}
}


