;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025, 2026 South Australia Medical Imaging

(use-modules (guix gexp)
             (guix packages)
             (gnu packages)
             (gnu packages base)
             (gnu packages compression))

;;; Commentary:

;; Pass the dataset from patient 4 in the SNMMI Lu-177 Dosimetry
;; Challenge 2021
;; (<https://deepblue.lib.umich.edu/data/collections/hm50ts030>)
;; through Spider's time-integrated activity (TIA) pipeline in an
;; isolated, self-contained, and reproducible build environment with
;; bullet-proof caching (work re-use) using GNU Guix.

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

(define dcm2niix
  (specification->package "dcm2niix"))

(define (spect-snmmi n)
  ;; 3D NIfTI image and BIDS sidecar of SPECT scan number N.
  (computed-file (string-append "spect" (number->string n) "-snmmi")
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" #$output
                               "-f" "spect"
                               #$(spect-dicom-dir-snmmi n))))))

(define elastix
  (specification->package "elastix"))

(define (registered-spect-snmmi n)
  ;; 'result.0.nii' is the Nth SPECT registered to the first SPECT.
  ;; Note the documentation for the benchmark TIA image also describes
  ;; registering SPECTs to the first SPECT.
  (computed-file (string-append "registered-spect" (number->string n) "-snmmi")
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$elastix "/bin/elastix")
                               "-f" (string-append #$(spect-snmmi 1)
                                                   "/spect.nii")
                               "-m" (string-append #$(spect-snmmi n)
                                                   "/spect.nii")
                               "-p"
                               #$(local-file "../../etc/Parameters_Rigid.txt")
                               "-out" #$output)))))

;; Include the top-level file, which provides spider.
(include "../../guix.scm")

;; Variant of spider that builds, potentially tests, and installs
;; benchmark targets.
(define spider-benchmark
  (package/inherit spider
    (name "spider-benchmark")
    (arguments
     (substitute-keyword-arguments arguments
       ((#:configure-flags cf)
        #~(append #$cf (list "-DSPIDER_BUILD_BENCHMARKS=ON"
                             "-DSPIDER_DOWNLOAD_BENCHMARK_DATA=OFF")))
       ((#:phases phases #~%standard-phases)
        #~(modify-phases #$phases
            (replace 'install
              (lambda _
                (install-file "benchmark/slice_compare" #$output)
                (install-file "benchmark/joint_hist" #$output)))))))))

(define spider-tia-snmmi
  (computed-file "spider-tia-snmmi"
                 ;; For invoke and install-file.
                 (with-imported-modules '((guix build utils))
                   #~(begin (use-modules (guix build utils))
                            (invoke (string-append #$spider "/bin/spider_tia")
                                    "-v" "-z" "America/Detroit"
                                    "-d" #$(spect-dicom-dir-snmmi 1)
                                    "-d" #$(spect-dicom-dir-snmmi 2)
                                    "-d" #$(spect-dicom-dir-snmmi 3)
                                    "-d" #$(spect-dicom-dir-snmmi 4)
                                    "-i" (string-append #$(spect-snmmi 1)
                                                        "/spect.nii")
                                    "-i"
                                    (string-append #$(registered-spect-snmmi 2)
                                                   "/result.0.nii")
                                    "-i"
                                    (string-append #$(registered-spect-snmmi 3)
                                                   "/result.0.nii")
                                    "-i"
                                    (string-append #$(registered-spect-snmmi 4)
                                                   "/result.0.nii"))
                            (install-file "tia.nii" #$output)))))


;;; TIA image included in the benchmark dataset.

(define snmmi-tia.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17680076"
                        "/files/patient_4.zip?download=1"))
    (file-name "zenodo-17680076-patient_4.zip")
    (sha256
     (base32 "01b4z28w41ws13dvlffw41zz5b23i0q8an38zxqarszx10fqg49s"))))

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
                    ;; (approx. 28 MBq.h/mL).
                    z "1e11" "0.5")
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
