.. SPDX-License-Identifier: CC-BY-4.0
.. Copyright (C) 2026 South Australia Medical Imaging

.. _spider-repository: https://github.com/SAMI-Medical-Physics/spider

Spider benchmark results
========================

.. contents::
   :depth: 2

Introduction
------------

We evaluate the performance of the `Spider dosimetry pipeline
<spider-repository_>`_ by applying it to publicly available datasets.

Time-integrated activity image
------------------------------

Spider was used to compute a time-integrated activity (TIA) image from
the SPECT/CT scans of patient 4 in the `SNMMI Lu-177 Dosimetry
Challenge 2021 dataset
<https://deepblue.lib.umich.edu/data/collections/hm50ts030?locale=en>`_.
This benchmark dataset also includes a TIA image that, according to
the dataset's documentation, was computed using the proprietary MIM
MRT Dosimetry package.

The TIA image from Spider was compared with the TIA image from the
benchmark dataset.
Below are two axial slices from the benchmark TIA image (left) and
Spider's TIA image (right) over registered CT.
The cyan contour is at a TIA of 10\ :sup:`11` disintegrations mL\
:sup:`-1` (≈ 28 MBq h mL\ :sup:`-1`).

.. list-table::
   :class: borderless
   :align: center

   * - .. image:: tia/image1_145.png
     - .. image:: tia/image2_145.png
   * - .. image:: tia/image1_133.png
     - .. image:: tia/image2_133.png
   * - *Benchmark TIA*
     - *Spider TIA*

Below is the joint histogram for the benchmark and Spider TIA images.

.. image:: tia/tia_joint_hist.svg
   :align: center

.. Placeholder for provenance information.

License
-------

Copyright © 2026 South Australia Medical Imaging.
This document is published under the terms of the `CC BY 4.0
<https://creativecommons.org/licenses/by/4.0/>`_ license.
