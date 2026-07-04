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
  ;; parlance.
  (define dcm2niix
    (specification->package "dcm2niix"))

  (define pigz
    (specification->package "pigz"))

  (define (run-dcm2niix dir)
    (computed-file "spider-dcm2niix-output"
                   (with-imported-modules '((guix build utils)) ;for invoke
                     #~(begin
                         (use-modules (guix build utils))
                         ;; Make dcm2niix write compressed images faster.
                         (setenv "PATH" (string-append #$pigz "/bin"))
                         (mkdir #$output)
                         (invoke (string-append #$dcm2niix "/bin/dcm2niix")
                                 "-o" #$output "-f" "image" "-z" "y" #$dir)))))

  (define elastix
    (specification->package "elastix"))

  (define (run-elastix fixed moving)
    (computed-file "spider-elastix-output"
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

  (define spect-dirs
    (map run-dcm2niix dirs))

  (define spect-images
    (map (lambda (dir)
           (file-append dir "/image.nii.gz"))
         spect-dirs))

  (define elastix-output-dirs
    (map (lambda (moving)
           (run-elastix (car spect-images) moving))
         (cdr spect-images)))

  (define registered-images
    (cons (car spect-images)
          (map (lambda (dir)
                 (file-append dir "/result.0.nii"))
               elastix-output-dirs)))

  (define build-tia-image
    (with-imported-modules '((guix build utils)) ;for invoke
      #~(begin
          (use-modules (guix build utils)
                       (srfi srfi-1))   ;for append-map
          (mkdir #$output)
          (apply invoke (string-append #$spider "/bin/spider_tia")
                 (append (if #$verbose?
                             (list "-v")
                             '())
                         (list "-o" (string-append #$output "/tia.nii.gz"))
                         (append-map (lambda (tz)
                                       (list "-z" tz))
                                     (list #$@time-zone))
                         (append-map (lambda (dir)
                                       (list "-d" dir))
                                     (list #$@dirs))
                         (append-map (lambda (image)
                                       (list "-i" image))
                                     (list #$@registered-images)))))))

  (define tia-image
    (computed-file "spider-tia-image" build-tia-image))

  (define build
    #~(begin
        (mkdir #$output)
        (for-each (lambda (i dir)
                    (symlink dir (format #f "~a/spect~a" #$output i)))
                  (iota (length (list #$@spect-dirs)) 1)
                  (list #$@spect-dirs))

        (for-each (lambda (i dir)
                    (symlink dir
                             (format #f "~a/registered_spect~a" #$output i)))
                  (iota (length (list #$@elastix-output-dirs)) 2)
                  (list #$@elastix-output-dirs))

        (symlink (string-append #$tia-image "/tia.nii.gz")
                 (string-append #$output "/tia.nii.gz"))))

  (computed-file "spider-output" build))

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
