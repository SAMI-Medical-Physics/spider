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
We calculated the per-pixel ratio of TIA values between the Spider and
benchmark images.
The distribution of the base-10 logarithm of this ratio is shown in
the histogram below.
Note that a value of 0 corresponds to equal voxel TIA values.

.. image:: tia/hist-.svg
   :align: center

If we exclude voxels that have TIA less than 10\ :sup:`10`
disintegrations mL\ :sup:`-1` (≈ 2.8 MBq h mL\ :sup:`-1`) in the
benchmark image, the distribution becomes:

.. image:: tia/hist-1e10.svg
   :align: center

.. Placeholder for provenance information.

License
-------

Copyright © 2026 South Australia Medical Imaging.
This document is published under the terms of the `CC BY 4.0
<https://creativecommons.org/licenses/by/4.0/>`_ license.
