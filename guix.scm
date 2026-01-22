;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025, 2026 South Australia Medical Imaging

(use-modules ((guix licenses) #:prefix license:)
             (guix)
             (guix build-system cmake)
             (guix git-download)
             (gnu packages check)
             (gnu packages compression)
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
     #~(list "-DSPIDER_DOWNLOAD_TEST_DATA=OFF"
             (string-append "-DSPIDER_TEST_DATA_DIR=" (canonicalize-path ".")
                            "/source/pet-images-01")
             ;; Development flags.
             "-DCMAKE_CXX_FLAGS=-Wall -Wextra -Wpedantic -Werror")
    #:phases
    #~(modify-phases %standard-phases
        (add-after 'unpack 'prepare-test-data
          (lambda* (#:key inputs native-inputs tests? #:allow-other-keys)
            (when tests?
              (invoke (search-input-file (or native-inputs inputs) "bin/unzip")
                      #$(this-package-native-input
                         "zenodo-4751233-pet-images-01.zip")))))
        (add-after 'unpack 'patch-commands
          (lambda* (#:key inputs #:allow-other-keys)
            (for-each
             (lambda (command)
               (substitute* "bin/spider.sh.in"
                 (((string-append command "_cmd=" command))
                  (string-append command "_cmd="
                                 (search-input-file inputs
                                                    (string-append "bin/"
                                                                   command))))))
             '("awk" "dcm2niix" "elastix" "mkdir" "mktemp" "rm" "sed"
               "tail")))))))
   (inputs (list dcm2niix
                 elastix
                 insight-toolkit
                 ;; Development inputs.
                 gnuplot
                 itk-snap))
   (native-inputs
    (list (origin
            (method url-fetch)
            (uri (string-append "https://zenodo.org/records/4751233"
                                "/files/pet-images-01.zip?download=1"))
            (sha256
             (base32
              "1nffcmql6g3c1l5s8fkpqk66gmingfmgiiyx37jvmz80zznn99wn"))
            (file-name "zenodo-4751233-pet-images-01.zip"))
          googletest
          unzip))
   (synopsis "Radionuclide therapy patient dosimetry")
   (description
    "Spider is a software pipeline for performing image-based radiation
dosimetry for radionuclide therapy patients.")
   (home-page "https://github.com/SAMI-Medical-Physics/spider")
   (license license:agpl3+)))

spider
