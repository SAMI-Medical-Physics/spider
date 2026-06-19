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

SNMMI Lu-177 Dosimetry Challenge 2021
-------------------------------------

Spider was used to compute time-integrated activity (TIA) images from
the SPECT/CT scans of patients 4 and 6 in the `SNMMI Lu-177 Dosimetry
Challenge 2021 dataset
<https://deepblue.lib.umich.edu/data/collections/hm50ts030?locale=en>`_.
This benchmark dataset also includes reference TIA images that,
according to the dataset's documentation, were computed using the
proprietary MIM MRT Dosimetry package.

For each patient, the TIA image computed by Spider was compared with
the reference TIA image from the benchmark dataset.
Below, corresponding axial slices of the reference TIA image (left)
and Spider's TIA image (right) are shown over registered CT.
Cyan contours are drawn at a TIA of 10\ :sup:`11` disintegrations mL\
:sup:`-1` (≈ 28 MBq h mL\ :sup:`-1`).
Joint histograms of the reference and Spider TIA images are also
shown.

Patient 4
^^^^^^^^^

.. list-table::
   :class: borderless
   :align: center

   * - .. image:: snmmi/pt4/image1_145.png
     - .. image:: snmmi/pt4/image2_145.png
   * - .. image:: snmmi/pt4/image1_133.png
     - .. image:: snmmi/pt4/image2_133.png
   * - *Reference TIA*
     - *Spider TIA*

.. image:: snmmi/pt4/tia_joint_hist.svg
   :align: center

Patient 6
^^^^^^^^^

.. list-table::
   :class: borderless
   :align: center

   * - .. image:: snmmi/pt6/image1_170.png
     - .. image:: snmmi/pt6/image2_170.png
   * - .. image:: snmmi/pt6/image1_154.png
     - .. image:: snmmi/pt6/image2_154.png
   * - .. image:: snmmi/pt6/image1_132.png
     - .. image:: snmmi/pt6/image2_132.png
   * - .. image:: snmmi/pt6/image1_114.png
     - .. image:: snmmi/pt6/image2_114.png
   * - *Reference TIA*
     - *Spider TIA*

.. image:: snmmi/pt6/tia_joint_hist.svg
   :align: center

.. Placeholder for provenance information.

License
-------

Copyright © 2026 South Australia Medical Imaging.
This document is published under the terms of the `CC BY 4.0
<https://creativecommons.org/licenses/by/4.0/>`_ license.
