# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 South Australia Medical Imaging

# Plot a 2D/joint histogram by reading lines from stdin in the format:
# 'xcentre ycentre xlow xhigh ylow yhigh counts'.
# Usage: 'PROGRAM | gnuplot -c joint_hist.gp XLABEL YLABEL OUT_FILENAME'.

set terminal svg
set output ARG3
unset key
unset colorbox
set isotropic
set title "Joint Histogram" font "Arial,16"
set xlabel ARG1 font "Arial,14"
set ylabel ARG2 offset 1,0 font "Arial,14"
set palette defined (0 "white", 1 "dark-red")
set cbrange [0:*]               # data does not include empty bins
set style fill solid
set grid front
plot '<cat' using 1:2:3:4:5:6:(log10(1+$7)) with boxxyerror lc palette z
