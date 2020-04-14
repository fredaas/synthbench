#!/usr/bin/env python3
#
# USAGE
#
#     compute_metrics.py <filename> <sockets> <cores> <affinity>
#
# OPTIONS
#
#     filename
#
#         File containing the performance matrix from the ping-pong benchmark.
#
#     sockets
#
#         Number of sockets per node. Must be one of 1 or 2.
#
#     cores
#
#         Number of cores per node.
#
#     affinity
#
#         Must be one of 'scattered' or 'compact'. Ignored when 'sockets' is set
#         to 1.
#

import os
import sys

filename = None
num_sockets_per_node = None
num_cores_per_node = None
affinity = None

def parse_args(argc, argv):
    global filename
    global num_sockets_per_node
    global num_cores_per_node
    global affinity

    filename             = argv[0]
    num_sockets_per_node = int(argv[1])
    num_cores_per_node   = int(argv[2])

    # Don't care about affinity on one socket
    if num_sockets_per_node == 1:
        return

    affinity             = argv[3]

def on_same_socket(i, j):
    if affinity == "scattered":
        return (i % 2 == 0) and (j % 2 == 0)
    if affinity == "compact":
        k = num_cores_per_node / 2
        return (i < k and j < k) or (i >= k and j >= k)

argv = sys.argv[1:]
argc = len(argv)

parse_args(argc, argv)

mat = open(filename, "r").read().split("\n")
mat = [ x for x in mat if len(x) > 0 ]

num_rows = len(mat)
num_cols = len(mat[0].strip().split(" "))

print("Matrix dimensions: {} rows x {} columns".format(num_rows, num_cols))

# Performance counters
perf_min_intra_socket = float("inf")
perf_max_intra_socket = 0.0
perf_min_inter_socket = float("inf")
perf_max_inter_socket = 0.0
perf_min_inter_node   = float("inf")
perf_max_inter_node   = 0.0
perf_avg_inter_node   = 0.0
perf_avg_intra_socket = 0.0
perf_avg_inter_socket = 0.0
count_intra_socket    = 0
count_inter_socket    = 0
count_inter_node      = 0

for rank_i, row in enumerate(mat):
    cols = row.strip().split(" ")
    start = rank_i + 1
    node_i = int(rank_i / num_cores_per_node)
    for rank_j, col in enumerate(cols[start:], start=start):
        node_j = int(rank_j / num_cores_per_node)
        col = float(col)
        # 'rank_i' and 'rank_j' are on separate nodes
        if node_j > node_i:
            if col < perf_min_inter_node:
                perf_min_inter_node = col
            if col > perf_max_inter_node:
                perf_max_inter_node = col
            perf_avg_inter_node += col
            count_inter_node += 1
        else:
            if num_sockets_per_node == 1:
                if col < perf_min_intra_socket:
                    perf_min_intra_socket = col
                if col > perf_max_intra_socket:
                    perf_max_intra_socket = col
                perf_avg_intra_socket += col
                count_intra_socket += 1
            else:
                # 'rank_ i' and 'rank_j' are on same socket
                if on_same_socket(rank_i, rank_j):
                    if col < perf_min_intra_socket:
                        perf_min_intra_socket = col
                    if col > perf_max_intra_socket:
                        perf_max_intra_socket = col
                    perf_avg_intra_socket += col
                    count_intra_socket += 1
                else:
                    if col < perf_min_inter_socket:
                        perf_min_inter_socket = col
                    if col > perf_max_inter_socket:
                        perf_max_inter_socket = col
                    perf_avg_inter_socket += col
                    count_inter_socket += 1

if count_intra_socket > 0:
    perf_avg_intra_socket /= count_intra_socket
if count_inter_socket > 0:
    perf_avg_inter_socket /= count_inter_socket
if count_inter_node > 0:
    perf_avg_inter_node /= count_inter_node

print("Intra-socket latency (min/max/avg): {:.3}/{:.3}/{:.3}".format(
    perf_min_intra_socket,
    perf_max_intra_socket,
    perf_avg_intra_socket))

print("Inter-socket latency (min/max/avg): {:.3}/{:.3}/{:.3}".format(
    perf_min_inter_socket,
    perf_max_inter_socket,
    perf_avg_inter_socket))

print("Inter-node latency (min/max/avg): {:.3}/{:.3}/{:.3}".format(
    perf_min_inter_node,
    perf_max_inter_node,
    perf_avg_inter_node))
