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

size_t nav_adj_counts[NAV_NODE_CAP];
struct NavAdj {
	nav it;
	num dist;
} nav_adj[NAV_NODE_CAP][NAV_NODE_CAP];

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

void initialize_nav_edges(num world_l, num world_r, num world_b, num world_t) {
	if (obstacle_count * 4 > NAV_NODE_CAP) {
		printf("ERROR: Nav node capacity is too small\n");
		exit(1);
	}
	nav_node_count = 0;
	range(i, obstacle_count) {
		num l = obstacles[i].l;
		num r = obstacles[i].r;
		num b = obstacles[i].b;
		num t = obstacles[i].t;
		num vs[4][2] = {{r, t}, {l, t}, {l, b}, {r, b}};
		range(j, 4) {
			num x = vs[j][0];
			num y = vs[j][1];
			if (world_l < x && x < world_r && world_b < y && y < world_t) {
				nav_nodes[nav_node_count].x = x;
				nav_nodes[nav_node_count].y = y;
				nav_adj_counts[nav_node_count] = 0;
				nav_node_count += 1;
			}
		}
	}

	range(j, nav_node_count) {
		range(i, j) {
			if (!interval_obstructed(
				nav_nodes[i].x, nav_nodes[i].y, nav_nodes[j].x, nav_nodes[j].y
			)) {
				struct NavAdj *adj_ij = &nav_adj[i][nav_adj_counts[i]];
				nav_adj_counts[i] += 1;
				struct NavAdj *adj_ji = &nav_adj[j][nav_adj_counts[j]];
				nav_adj_counts[j] += 1;
				adj_ij->it.i = j;
				adj_ji->it.i = i;
				num distance = num_hypot(
					nav_nodes[j].x - nav_nodes[i].x,
					nav_nodes[j].y - nav_nodes[i].y
				);
				adj_ij->dist = distance;
				adj_ji->dist = distance;
			}
		}
	}
}

struct PathQueueElem {
	num dist_so_far;
	num dist_heuristic;
	nav curr;
	nav pred;
} path_queue[NAV_NODE_CAP];
size_t path_queue_count;

void path_queue_push(num dist, num heuristic, nav curr, nav pred) {
	struct PathQueueElem new = { dist, heuristic, curr, pred};
	size_t i;
	// @Performance binary search or priority queue
	for (i = 0; i < path_queue_count; i++) {
		if (new.dist_heuristic < path_queue[i].dist_heuristic) {
			break;
		}
		if (curr.i == path_queue[i].curr.i) {
			return;
		}
	}
	for (; i < path_queue_count + 1; i++) {
		struct PathQueueElem tmp = path_queue[i];
		path_queue[i] = new;
		if (i < path_queue_count && tmp.curr.i == curr.i) {
			return;
		}
		new = tmp;
	}
	path_queue_count += 1;
}

void path_queue_pop() {
	path_queue_count -= 1;
	range (i, path_queue_count) {
		path_queue[i] = path_queue[i + 1];
	}
}

// @Performance scanning line thing
void pick_route(
	num startx, num starty,
	num endx, num endy,
	size_t *path_count, nav *path_out
) {
	static bool covered[NAV_NODE_CAP];
	static num end_dist[NAV_NODE_CAP];
	static bool end_clear[NAV_NODE_CAP];
	static nav pred[NAV_NODE_CAP];
	path_queue_count = 0;
	range (i, nav_node_count) {
		// @Robustness is ~0U even right?? surely ~0UL or UINT64_MAX... ugh
		pred[i].i = ~0U;
		covered[i] = false;
		num heuristic =
			num_hypot(endx - nav_nodes[i].x, endy - nav_nodes[i].y);
		if (!interval_obstructed(startx, starty, nav_nodes[i].x, nav_nodes[i].y)) {
			num dist =
				num_hypot(nav_nodes[i].x - startx, nav_nodes[i].y - starty);
			path_queue_push(dist, dist + heuristic, (nav){i}, (nav){~0U});
		}
		end_dist[i] = heuristic;
		end_clear[i] =
			!interval_obstructed(nav_nodes[i].x, nav_nodes[i].y, endx, endy);
	}
	*path_count = 0;
	nav end = (nav){~0U};
	while (path_queue_count > 0) {
		num dist_so_far = path_queue[0].dist_so_far;
		nav curr = path_queue[0].curr;
		covered[curr.i] = true;
		pred[curr.i] = path_queue[0].pred;
		path_queue_pop();
		if (end_clear[curr.i]) {
			end = curr;
			break;
		}
		range (j, nav_adj_counts[curr.i]) {
			nav next = nav_adj[curr.i][j].it;
			// @Correctness just because curr is optimal does not mean next
			// will be... need to change this so that covered[i] and pred[i]
			// are set only when expanding i, not just queueing i
			if (!covered[next.i]) {
				num step_dist = nav_adj[curr.i][j].dist;
				num dist = dist_so_far + step_dist;
				path_queue_push(dist, dist + end_dist[next.i], next, curr);
			}
		}
	}
	while (end.i != ~0U) {
		path_out[*path_count] = end;
		*path_count += 1;
		end = pred[end.i];
	}
}


