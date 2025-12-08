;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2025 South Australia Medical Imaging

(use-modules (guix gexp)
             (guix packages)
             (gnu packages)
             (gnu packages base)
             (gnu packages compression))

;;; Commentary:

;; Pass the dataset from patient 4 in the SNMMI Lu-177 Dosimetry
;; Challenge 2021
;; (<https://snmmi.org/Therapy/SNMMI-THERAPY/Dosimetry_Challenge.aspx>)
;; through Spider's time-integrated activity (TIA) pipeline in an
;; isolated, self-contained, and reproducible build environment with
;; bullet-proof caching (work re-use) using GNU Guix.

;;; Code:

(define spect/cts.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17271433"
                        "/files/patient_4.zip?download=1"))
    (file-name "zenodo-17271433-patient_4.zip")
    (sha256
     (base32 "0f1al7v45w0zksv6hh1gwjmr8v97ad9cb43ibhpwg4mxh6dfiy77"))))

(define unzip
  (specification->package "unzip"))

(define spect/cts
  (computed-file "benchmark-patient-4-spect-cts"
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$spect/cts.zip "-d" #$output)))))

(define dcm2niix
  (specification->package "dcm2niix"))

(define (spect n)
  ;; 3D NIfTI image and BIDS sidecar of SPECT scan number N.
  (computed-file (string-append "benchmark-patient-4-spect" (number->string n))
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" #$output
                               "-f" "spect"
                               (string-append #$spect/cts
                                              "/patient_4/SPECT_Cts/scan"
                                              #$(number->string n)
                                              "/spect"))))))

(define elastix
  (specification->package "elastix"))

(define (registered-spect n)
  ;; 'result.0.nii' is the Nth SPECT registered to the first SPECT.
  (computed-file (string-append "benchmark-patient-4-spect-"
                                (number->string n)
                                "-registered")
                 (with-imported-modules '((guix build utils)) ;for invoke
                   #~(begin
                       (use-modules (guix build utils))
                       (mkdir #$output)
                       (invoke (string-append #$elastix "/bin/elastix")
                               "-f" (string-append #$(spect 1) "/spect.nii")
                               "-m" (string-append #$(spect n) "/spect.nii")
                               "-p" #$(local-file "../../etc/Parameters_Rigid.txt")
                               "-out" #$output)))))

;; Include the top-level file, which provides spider.
(include "../../guix.scm")

;; Variant of spider that builds, potentially tests, and installs
;; benchmark targets.
(define spider-benchmark
  (package/inherit spider
    (name "spider-benchmark")
    (arguments
     (substitute-keyword-arguments (package-arguments spider)
       ((#:configure-flags cf)
        #~(append #$cf (list "-DSPIDER_BUILD_BENCHMARKS=ON"
                             "-DSPIDER_DOWNLOAD_BENCHMARK_DATA=OFF")))
       ((#:phases phases #~%standard-phases)
        #~(modify-phases #$phases
            (replace 'install
              (lambda _
                (install-file "benchmark/tia/log10_ratio" #$output)))))))))

(define spider-tia
  (computed-file "benchmark-patient-4-spider-tia"
                 ;; For invoke and install-file.
                 (with-imported-modules '((guix build utils))
                   #~(begin (use-modules (guix build utils)
                                         (ice-9 time))
                            (apply invoke (string-append
                                           #$spider
                                           "/bin/spider_make_tia_benchmark")
                                   (string-append #$(spect 1) "/spect.nii")
                                   (map (lambda (x)
                                          (string-append x "/result.0.nii"))
                                        (list #$(registered-spect 2)
                                              #$(registered-spect 3)
                                              #$(registered-spect 4))))
                            (install-file "tia.nii" #$output)))))


;;; TIA image included in the benchmark dataset.

(define benchmark-tia.zip
  (origin
    (method url-fetch)
    (uri (string-append "https://zenodo.org/records/17680076"
                        "/files/patient_4.zip?download=1"))
    (file-name "zenodo-17680076-patient_4.zip")
    (sha256
     (base32 "01b4z28w41ws13dvlffw41zz5b23i0q8an38zxqarszx10fqg49s"))))

(define benchmark-tia
  (computed-file "benchmark-patient-4-benchmark-tia"
                 (with-imported-modules '((guix build utils))
                   #~(begin
                       (use-modules (guix build utils))
                       (invoke (string-append #$unzip "/bin/unzip")
                               #$benchmark-tia.zip)
                       (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                               "-o" "."
                               "-f" "tia"
                               "patient_4/TIA")
                       (install-file "tia.nii" #$output)))))


;;; Compare TIA images from Spider and the benchmark dataset.

(define gnuplot
  (specification->package "gnuplot"))

(define coreutils
  (specification->package "coreutils"))

(define tia-comparison
  (computed-file
   "benchmark-patient-4-tia-comparison"
   #~(begin
       (use-modules (ice-9 popen)
                    (ice-9 receive))
       (mkdir #$output)
       ;; hist.gp uses 'cat'.
       (setenv "PATH" (string-append #$coreutils "/bin"))
       (setenv "XDG_CACHE_HOME" ".")    ;placate Fontconfig
       (map
        (lambda (threshold)
          (receive (from to pids)
              (pipeline
               (list (append (list (string-append #$spider-benchmark
                                                  "/log10_ratio")
                                   (string-append #$spider-tia "/tia.nii")
                                   (string-append #$benchmark-tia "/tia.nii"))
                             (if (string=? threshold "") '() (list threshold)))
                     (list (string-append #$gnuplot "/bin/gnuplot")
                           "-p" #$(local-file "hist.gp"))))
            (close to)
            (close from)
            (for-each waitpid pids))
          (copy-file "hist.pdf"
                     (string-append #$output "/hist-" threshold ".pdf")))
        '("" "1e9")))))

tia-comparison
