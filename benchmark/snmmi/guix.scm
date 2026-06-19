;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025, 2026 South Australia Medical Imaging

(use-modules (guix gexp)
             (guix packages)
             (gnu packages)
             (gnu packages base)
             (gnu packages compression))

;;; Commentary:
;;;
;;; Pass the dataset from patient 4 in the SNMMI Lu-177 Dosimetry
;;; Challenge 2021
;;; (<https://deepblue.lib.umich.edu/data/collections/hm50ts030>)
;;; through the Spider pipeline in an isolated, self-contained, and
;;; reproducible build environment with bullet-proof caching (work
;;; re-use) using GNU Guix.
;;;
;;; Code:

(define spect/cts-snmmi.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17271433"
                        "/files/patient_4.zip?download=1"))
    (file-name "zenodo-17271433-patient_4.zip")
    (sha256
     (base32 "0f1al7v45w0zksv6hh1gwjmr8v97ad9cb43ibhpwg4mxh6dfiy77"))))

(define unzip
  (specification->package "unzip"))

(define spect/cts-snmmi
  (computed-file "spect-cts-snmmi"
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$spect/cts-snmmi.zip "-d" #$output)))))

(define (spect-dicom-dir-snmmi n)
  #~(string-append #$spect/cts-snmmi "/patient_4/SPECT_Cts/scan"
                   #$(number->string n) "/spect"))

;; For run-spider and spider-benchmark.
(include "../utils.scm")

(define spider-tia-snmmi
  ;; The documentation for the benchmark TIA image describes
  ;; registering SPECTs to the first SPECT.
  (run-spider (list (spect-dicom-dir-snmmi 1)
                    (spect-dicom-dir-snmmi 2)
                    (spect-dicom-dir-snmmi 3)
                    (spect-dicom-dir-snmmi 4))
              #:verbose? #t
              #:time-zone (list "America/Detroit")))


;;; TIA image included in the benchmark dataset.

(define snmmi-tia.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17680076"
                        "/files/patient_4.zip?download=1"))
    (file-name "zenodo-17680076-patient_4.zip")
    (sha256
     (base32 "01b4z28w41ws13dvlffw41zz5b23i0q8an38zxqarszx10fqg49s"))))

(define dcm2niix
  (specification->package "dcm2niix"))

(define snmmi-tia
  (computed-file "snmmi-tia"
                 (with-imported-modules '((guix build utils))
                   #~(begin
                       (use-modules (guix build utils))
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$snmmi-tia.zip)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" "."
                               "-f" "tia"
                               "patient_4/TIA")
                       (install-file "tia.nii" #$output)))))


;;; Compare TIA images from Spider and the benchmark dataset.

(define (ct-dicom-dir-snmmi n)
  #~(string-append #$spect/cts-snmmi "/patient_4/SPECT_Cts/scan"
                   #$(number->string n) "/ct"))

(define (ct-snmmi n)
  ;; 3D NIfTI image and BIDS sidecar of CT scan number N.
  (computed-file (string-append "ct" (number->string n) "-snmmi")
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" #$output
                               "-f" "ct"
                               #$(ct-dicom-dir-snmmi n))))))

(define gnuplot
  (specification->package "gnuplot"))

(define coreutils
  (specification->package "coreutils"))

(define tia-comparison-snmmi
  (computed-file
   "tia-comparison-snmmi"
   (with-imported-modules '((guix build utils)) ;for invoke, install-file
     #~(begin
         (use-modules (guix build utils)
                      (ice-9 popen)
                      (ice-9 receive))
         ;; Generate PNG images of axial slices over registered CT.
         (map
          (lambda (z)
            (invoke (string-append #$spider-benchmark "/slice_compare")
                    (string-append #$snmmi-tia "/tia.nii")
                    (string-append #$spider-tia-snmmi "/tia.nii")
                    (string-append #$(ct-snmmi 1) "/ct.nii")
                    ;; Display contours at a TIA of 10^11 disintegrations/mL
                    ;; (approx. 28 MBq.h/mL).  Display the CT background using a
                    ;; window width of 400 and window level of 50.
                    z "1e11" "0.5" "400" "50")
            (install-file (string-append "image1_" z ".png") #$output)
            (install-file (string-append "image2_" z ".png") #$output))
          '("145"         ;slice 145 (0-indexed) contains Lesion 1 (liver large)
            "133"))       ;slice 133 contains Lesion 2 (liver small)

         ;; Joint histogram.
         (let ((outfile "tia_joint_hist.svg"))
           ;; joint_hist.gp uses 'cat'.
           (setenv "PATH" (string-append #$coreutils "/bin"))
           (setenv "XDG_CACHE_HOME" ".")    ;placate Fontconfig
           (receive (from to pids)
               (pipeline
                (list (append (list (string-append #$spider-benchmark
                                                   "/joint_hist")
                                    (string-append #$snmmi-tia "/tia.nii")
                                    (string-append #$spider-tia-snmmi
                                                   "/tia.nii")))
                      (list (string-append #$gnuplot "/bin/gnuplot")
                            "-c" #$(local-file "../joint_hist.gp")
                            "Benchmark TIA (MBq h mL^{-1})"
                            "Spider TIA (MBq h mL^{-1})"
                            ;; TIA images have units of disintegrations/mL;
                            ;; display MBq.h/mL in joint histogram.
                            "3.6e9" outfile)))
             (close to)
             (close from)
             (for-each waitpid pids))
           (install-file outfile #$output))))))

tia-comparison-snmmi
