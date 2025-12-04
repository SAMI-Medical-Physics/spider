;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025 South Australia Medical Imaging

(use-modules ((guix licenses) #:prefix license:)
             (guix)
             (guix build-system cmake)
             (guix git-download)
             (gnu packages check)
             (gnu packages image-processing)
             (gnu packages maths)
             (guix-science packages neuroscience)) ;dcm2niix, elastix

(define spider
  (package
   (name "spider")
   (version "0.0.0-git")
   (source (local-file "." "spider-checkout"
                       #:recursive? #t
                       ;; Only include files under version control.
                       #:select? (git-predicate (current-source-directory))))
   (build-system cmake-build-system)
   (arguments
    (list
     #:configure-flags
     ;; Development flags.
     #~(list "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror")))
   (inputs (list insight-toolkit
                 ;; Development inputs.
                 dcm2niix
                 elastix
                 gnuplot
                 itk-snap))
   (native-inputs (list googletest))
   (synopsis "Radionuclide therapy patient dosimetry")
   (description
    "Spider is a software pipeline for performing image-based radiation
dosimetry for radionuclide therapy patients.")
   (home-page "https://github.com/SAMI-Medical-Physics/spider")
   (license license:agpl3+)))

spider
