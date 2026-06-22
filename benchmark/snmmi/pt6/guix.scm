;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025, 2026 South Australia Medical Imaging

;;; Commentary:
;;;
;;; Pass the dataset from patient 6 in the SNMMI Lu-177 Dosimetry
;;; Challenge 2021
;;; (<https://deepblue.lib.umich.edu/data/collections/hm50ts030>)
;;; through the Spider pipeline in an isolated, self-contained, and
;;; reproducible build environment with bullet-proof caching (work
;;; re-use) using GNU Guix.
;;;
;;; Code:

(use-modules (guix gexp)
             (guix packages)
             (gnu packages)
             (gnu packages base)
             (gnu packages compression))

(define spect/cts-snmmi-pt6.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17271433"
                        "/files/patient_6.zip?download=1"))
    (file-name "zenodo-17271433-patient_6.zip")
    (sha256
     (base32 "1ahw37hg9hnkrw9j0048galyc15m5nw7nqgannspr5dj9zsvw7pq"))))

(define unzip
  (specification->package "unzip"))

;; For run-spider and spider-benchmark.
(include "../../utils.scm")

(define spect/cts-snmmi-pt6
  (computed-file "spect-cts-snmmi-pt6"
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$spect/cts-snmmi-pt6.zip)
                       ;; One slice of the 2nd SPECT has modality 'NM' while the
                       ;; rest have 'PT', which causes dcm2niix to write 2
                       ;; images.
                       (invoke (string-append #$spider-benchmark
                                              "/set_modality_pt")
                               (string-append "patient_6/SPECT_Cts/scan2/spect/"
                                              "2.16.840.1.114362.1.11987842."
                                              "22403444876.565511897.618.970."
                                              "dcm"))
                       (mkdir-p #$output)
                       (copy-recursively "." #$output)))))

(define (spect-dicom-dir-snmmi-pt6 n)
  (file-append spect/cts-snmmi-pt6 "/patient_6/SPECT_Cts/scan"
               (number->string n) "/spect"))

(define spider-tia-snmmi-pt6
  ;; The documentation for the benchmark TIA image describes
  ;; registering SPECTs to the first SPECT.
  (run-spider (list (spect-dicom-dir-snmmi-pt6 1)
                    (spect-dicom-dir-snmmi-pt6 2)
                    (spect-dicom-dir-snmmi-pt6 3)
                    (spect-dicom-dir-snmmi-pt6 4))
              #:verbose? #t
              #:time-zone (list "America/Detroit")))


;;; TIA image included in the benchmark dataset.

(define snmmi-tia-pt6.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17680076"
                        "/files/patient_6.zip?download=1"))
    (file-name "zenodo-17680076-patient_6.zip")
    (sha256
     (base32 "1gqwimbwf2d43qk14jjijp5mdmdq03jc6avsblqm9in40inma87r"))))

(define dcm2niix
  (specification->package "dcm2niix"))

(define snmmi-tia-pt6
  (computed-file "snmmi-tia-pt6"
                 (with-imported-modules '((guix build utils))
                   #~(begin
                       (use-modules (guix build utils))
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$snmmi-tia-pt6.zip)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" "."
                               "-f" "tia"
                               "patient_6/TIA")
                       (install-file "tia.nii" #$output)))))


;;; Compare TIA images from Spider and the benchmark dataset.

(define (ct-dicom-dir-snmmi-pt6 n)
  (file-append spect/cts-snmmi-pt6 "/patient_6/SPECT_Cts/scan"
               (number->string n) "/ct"))

(define (ct-snmmi-pt6 n)
  ;; 3D NIfTI image and BIDS sidecar of CT scan number N.
  (computed-file (string-append "ct" (number->string n) "-snmmi-pt6")
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" #$output
                               "-f" "ct"
                               #$(ct-dicom-dir-snmmi-pt6 n))))))

(define gnuplot
  (specification->package "gnuplot"))

(define coreutils
  (specification->package "coreutils"))

(define tia-comparison-snmmi-pt6
  (computed-file
   "tia-comparison-snmmi-pt6"
   (with-imported-modules '((guix build utils)) ;for invoke, install-file
     #~(begin
         (use-modules (guix build utils)
                      (ice-9 popen)
                      (ice-9 receive))
         ;; Generate PNG images of axial slices over registered CT.
         (map
          (lambda (z)
            (invoke (string-append #$spider-benchmark "/slice_compare")
                    (string-append #$snmmi-tia-pt6 "/tia.nii")
                    (string-append #$spider-tia-snmmi-pt6 "/tia.nii")
                    (string-append #$(ct-snmmi-pt6 1) "/ct.nii")
                    ;; Display contours at a TIA of 10^11 disintegrations/mL
                    ;; (approx. 28 MBq.h/mL).  Display the CT background using a
                    ;; window width of 400 and window level of 50.
                    z "1e11" "0.5" "400" "50")
            (install-file (string-append "image1_" z ".png") #$output)
            (install-file (string-append "image2_" z ".png") #$output))
          '("170"   ;slice 170 (0-indexed) contains Lesion 1 (right lobe)
            "154"   ;Lesion 2 (left lobe)
            "132"   ;Lesion 3 (lymph node)
            "114")) ;Lesion 4 (chest wall)

         ;; Joint histogram.
         (let ((outfile "tia_joint_hist.svg"))
           ;; joint_hist.gp uses 'cat'.
           (setenv "PATH" (string-append #$coreutils "/bin"))
           (setenv "XDG_CACHE_HOME" ".")    ;placate Fontconfig
           (receive (from to pids)
               (pipeline
                (list (append (list (string-append #$spider-benchmark
                                                   "/joint_hist")
                                    (string-append #$snmmi-tia-pt6 "/tia.nii")
                                    (string-append #$spider-tia-snmmi-pt6
                                                   "/tia.nii")))
                      (list (string-append #$gnuplot "/bin/gnuplot")
                            "-c" #$(local-file "../../joint_hist.gp")
                            "Reference TIA (MBq h mL^{-1})"
                            "Spider TIA (MBq h mL^{-1})"
                            ;; TIA images have units of disintegrations/mL;
                            ;; display MBq.h/mL in joint histogram.
                            "3.6e9" outfile)))
             (close to)
             (close from)
             (for-each waitpid pids))
           (install-file outfile #$output))))))

tia-comparison-snmmi-pt6
