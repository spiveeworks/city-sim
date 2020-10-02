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

bool interval_obstructed(
	num x0, num y0, num x1, num y1
) {
	range(o, obstacle_count) {
		num l = obstacles[o].l;
		num r = obstacles[o].r;
		num b = obstacles[o].b;
		num t = obstacles[o].t;

		// If line doesnt touch polygon, then interval doesnt touch polygon on
		// the other hand if line touches polygon then its intersection with
		// the polygon will be another interval, specifically the intersection
		// of two half planes with the line
		// if the actual interval doesn't touch this intersection area then it
		// must be fully outside one of the half planes
		//
		// this lets us separate the test into two completely separate parts,
		// one based on the interval sitting outside any half plane, and the
		// other based on whether its extension to a line passes through the
		// polygon
		if (x0 <= l && x1 <= l) { continue; }
		if (x0 >= r && x1 >= r) { continue; }
		if (y0 <= b && y1 <= b) { continue; }
		if (y0 >= t && y1 >= t) { continue; }

		num dx = x1 - x0;
		num dy = y1 - y0;

		if (dx == 0 || dy == 0) {
			// the previous test is sufficient for axis aligned intervals
			return true;
		}

		// recall points on line will satisfy
		// (x - x0)(y1 - y0) = (y - y0)(x1 - x0)
		// avoid expanding brackets, to prevent overflow
		// x = x0 + (y - y0)(x1 - x0)/(y1 - y0)
		// y = y0 + (x - x0)(y1 - y0)/(x1 - x0)
		num bx = x0 + (b - y0)*dx/dy;
		num tx = x0 + (t - y0)*dx/dy;
		num ly = y0 + (l - x0)*dy/dx;
		num ry = y0 + (r - x0)*dy/dx;

		// a positive gradient line will miss the rectangle if and only if it
		// passes through the rays extending from either the top left or bottom
		// right corner
		// we test for these miss regions because that way we leave no place to
		// squeak through, whereas if we test the edges of the rectangle
		// directly then the corner of the rectangle becomes difficult to
		// detect
		if (dx * dy > 0) {
			if ((ly >= t && tx <= l) || (ry <= b && bx >= r)) {
				continue;
			} else {
				return true;
			}
		} else {
			if ((ry >= t && tx >= r) || (ly <= b && bx <= l)) {
				continue;
			} else {
				return true;
			}
		}
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
	obstacles[obstacle_count].l = -50*UNIT;
	obstacles[obstacle_count].r = 0*UNIT;
	obstacles[obstacle_count].b = 100*UNIT;
	obstacles[obstacle_count].t = 150*UNIT;
	obstacle_count += 1;


	nav_node_count = obstacle_count * 4;
	if (nav_node_count > NAV_NODE_CAP) {
		printf("ERROR: Nav node capacity is too small\n");
		exit(1);
	}
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
			if (!interval_obstructed(
				nav_nodes[i].x, nav_nodes[i].y, nav_nodes[j].x, nav_nodes[j].y
			)) {
				size_t ind = (j*j-j)/2 + i;
				nav_paths[ind].next.i = j;
				nav_paths[ind].prev.i = i;
				num dx = nav_nodes[j].x - nav_nodes[i].x;
				num dy = nav_nodes[j].y - nav_nodes[i].y;
				nav_paths[ind].distance = num_hypot(dx, dy);
			}
		}
	}

	// all-pairs shortest paths, using Floyd-Warshall
	range(k, nav_node_count) {
		range(j, nav_node_count) {
			range(i, nav_node_count) {
				if (i == j || j == k || k == i) {
					continue;
				}
				size_t indij = i < j ? (j*j-j)/2 + i : (i*i-i)/2 + j;
				size_t indik = i < k ? (k*k-k)/2 + i : (i*i-i)/2 + k;
				size_t indjk = j < k ? (k*k-k)/2 + j : (j*j-j)/2 + k;
				num dij = nav_paths[indij].distance;
				num dik = nav_paths[indik].distance;
				num djk = nav_paths[indjk].distance;
				if (dik != NUM_GREATEST && djk != NUM_GREATEST && dij > dik + djk) {
					nav nextik = i < k ? nav_paths[indik].next : nav_paths[indik].prev;
					nav nextjk = j < k ? nav_paths[indjk].next : nav_paths[indjk].prev;
					nav_paths[indij].next = i < j ? nextik : nextjk;
					nav_paths[indij].prev = i < j ? nextjk : nextik;
					nav_paths[indij].distance = dik + djk;
				}
			}
		}
	}
}

// @Performance scanning line thing
void pick_route(
	num startx, num starty,
	num endx, num endy,
	nav *startnode, nav *endnode
) {
	bool start_clear[nav_node_count];
	bool end_clear[nav_node_count];
	size_t start_clear_count = 0;
	size_t end_clear_count = 0;
	range (i, nav_node_count) {
		if (interval_obstructed(startx, starty, nav_nodes[i].x, nav_nodes[i].y)) {
			start_clear[i] = false;
		} else {
			start_clear[i] = true;
			start_clear_count += 1;
		}
		if (interval_obstructed(nav_nodes[i].x, nav_nodes[i].y, endx, endy)) {
			end_clear[i] = false;
		} else {
			end_clear[i] = true;
			end_clear_count += 1;
		}
	}
	size_t start_i[start_clear_count];
	num start_dist[start_clear_count];
	size_t start_dist_init_count = 0;
	size_t end_i[end_clear_count];
	num end_dist[end_clear_count];
	size_t end_dist_init_count = 0;
	range (i, nav_node_count) {
		if (start_clear[i]) {
			start_i[start_dist_init_count] = i;
			start_dist[start_dist_init_count] =
				num_hypot(nav_nodes[i].x - startx, nav_nodes[i].y - starty);
			start_dist_init_count += 1;
		}
		if (end_clear[i]) {
			end_i[end_dist_init_count] = i;
			end_dist[end_dist_init_count] =
				num_hypot(endy - nav_nodes[i].x, endy - nav_nodes[i].y);
			end_dist_init_count += 1;
		}
	}
	startnode->i = ~0U;
	endnode->i = ~0U;
	num min_dist = NUM_GREATEST;
	range (i0, start_clear_count) {
		size_t i = start_i[i0];
		range (j0, end_clear_count) {
			size_t j = end_i[j0];
			num path_dist = 0;
			if (i != j) {
				size_t ind = i < j ? (j*j-j)/2+i : (i*i-i)/2+j;
				path_dist = nav_paths[ind].distance;
			}
			num dist = NUM_GREATEST;
			if (
				start_dist[i0] < NUM_GREATEST &&
				path_dist < NUM_GREATEST &&
				end_dist[j0] < NUM_GREATEST
			) {
				dist = start_dist[i0] + path_dist + end_dist[j0];
			}
			if (dist < min_dist) {
				startnode->i = i;
				endnode->i = j;
				min_dist = dist;
			}
		}
	}
}


