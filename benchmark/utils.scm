;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2026 South Australia Medical Imaging

;;; Commentary:
;;;
;;; Tools for evaluating benchmark datasets.
;;;
;;; Code:

(use-modules (guix packages)
             (gnu packages)
             (guix gexp))

;; For spider.
(include "../guix.scm")

(define* (run-spider dirs #:key verbose? (time-zone '()))
  ;; A bare bones implementation of the Spider program using
  ;; G-expressions.  DIRS is a list of file-like objects, in Guix
  ;; parlance.  The output includes tia.nii.
  (define dcm2niix
    (specification->package "dcm2niix"))

  (define (run-dcm2niix dir)
    (computed-file "dcm2niix-output"
                   (with-imported-modules '((guix build utils)) ;for invoke
                     #~(begin
                         (use-modules (guix build utils))
                         (mkdir #$output)
                         (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                                 "-o" #$output "-f" "image" #$dir)))))

  (define elastix
    (specification->package "elastix"))

  (define (run-elastix fixed moving)
    (computed-file "elastix-output"
                   (with-imported-modules '((guix build utils)) ;for invoke
                     #~(begin
                         (use-modules (guix build utils))
                         (mkdir #$output)
                         (invoke (string-append #$elastix "/bin/elastix")
                                 "-f" #$fixed
                                 "-m" #$moving
                                 "-p"
                                 #$(local-file "../etc/Parameters_Rigid.txt")
                                 "-out" #$output)))))

  (define registered-images
    (let ((image1 (file-append (run-dcm2niix (car dirs)) "/image.nii")))
      (cons image1
            (map (lambda (dir)
                   (file-append
                    (run-elastix image1
                                 (file-append (run-dcm2niix dir) "/image.nii"))
                    "/result.0.nii"))
                 (cdr dirs)))))

  (define build
    (with-imported-modules '((guix build utils)) ;for invoke, install-file
      #~(begin
          (use-modules (guix build utils)
                       (srfi srfi-1))   ;for append-map
          (apply invoke (string-append #$spider "/bin/spider_tia")
                 (append (if #$verbose?
                             (list "-v")
                             '())
                         (append-map (lambda (tz)
                                       (list "-z" tz))
                                     (list #$@time-zone))
                         (append-map (lambda (dir)
                                       (list "-d" dir))
                                     (list #$@dirs))
                         (append-map (lambda (image)
                                       (list "-i" image))
                                     (list #$@registered-images))))
          (install-file "tia.nii" #$output))))

  (computed-file "tia" build))

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
                (install-file "benchmark/joint_hist" #$output)
                (install-file "benchmark/snmmi/pt6/set_modality_pt"
                              #$output)))))))))
