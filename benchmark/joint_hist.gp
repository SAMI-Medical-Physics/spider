# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 South Australia Medical Imaging

# Plot a 2D/joint histogram by reading lines from stdin in the format:
# 'xcentre ycentre xlow xhigh ylow yhigh counts'.  The values in the
# first 6 columns are divided by DIVISOR.  Usage: 'PROGRAM | gnuplot
# -c joint_hist.gp XLABEL YLABEL DIVISOR OUT_FILENAME'.

set encoding utf8               # used by benchmark/tia/run.sh.in
set terminal svg
set output ARG4
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
div = ARG3
plot '<cat' using ($1/div):($2/div):($3/div):($4/div):($5/div):($6/div)\
     :(log10(1+$7)) with boxxyerror lc palette z
