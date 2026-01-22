# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2025 South Australia Medical Imaging

# Plot a histogram by reading lines from stdin in the format:
# bin_edge_left bin_edge_right counts.
# Usage: 'PROGRAM | gnuplot hist.gp'.
# Warning: The horizontal axis tick interval is hard-coded to 1.

# Write to a file.
set terminal postscript eps
set output 'hist.eps'

set style fill solid
# Remove legend.
unset key
set yrange [0:*]
set ylabel "Frequency"
set xtics 1
set grid xtics
plot '<cat' using (($1+$2)/2.):3:($2-$1) with boxes
