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

(define* (run-spider dirs #:key verbose? (time-zone '()) split-build?)
  ;; A bare bones implementation of the Spider program using
  ;; G-expressions.  DIRS is a list of file-like objects, in Guix
  ;; parlance.  When SPLIT-BUILD? is true, it is faster to rebuild
  ;; run-spider with the same DIRS after changes to the spider
  ;; package, at the cost of additional store space.
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

  (define (run-elastix-and-clamp fixed moving verbose?)
    (computed-file "spider-elastix-output-clamped"
                   (with-imported-modules '((guix build utils)) ;for invoke
                     #~(begin
                         (use-modules (guix build utils))
                         (mkdir #$output)
                         (invoke (string-append #$elastix "/bin/elastix")
                                 "-f" #$fixed
                                 "-m" #$moving
                                 "-p"
                                 #$(local-file "../etc/Parameters_Rigid.txt")
                                 "-out" #$output)
                         ;; Set negative pixel values to zero to reduce .nii.gz
                         ;; size (for the SNMMI challenge datasets, from 38-45
                         ;; MiB to 26-29 MiB).
                         (apply invoke (string-append #$spider
                                                      "/bin/spider_clamp")
                                (append (if #$verbose?
                                            (list "-v")
                                            '())
                                        (list (string-append #$output
                                                             "/result.0.nii")
                                              (string-append #$output
                                                             "/result.0.nii"
                                                             ".gz"))))
                         (delete-file (string-append #$output
                                                     "/result.0.nii"))))))

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

  (define (clamp-elastix-output elastix-output verbose?)
    (computed-file "spider-elastix-output-clamped-split"
                   (with-imported-modules '((guix build utils)) ;for invoke
                     #~(begin
                         (use-modules (guix build utils))
                         (mkdir #$output)
                         (apply invoke (string-append #$spider
                                                      "/bin/spider_clamp")
                                (append (if #$verbose?
                                            (list "-v")
                                            '())
                                        (list (string-append #$elastix-output
                                                             "/result.0.nii")
                                              (string-append #$output
                                                             "/result.0.nii"
                                                             ".gz"))))
                         (for-each (lambda (file)
                                     (let ((f (basename file)))
                                       (unless (equal? f "result.0.nii")
                                         (symlink file
                                                  (format #f "~a/~a" #$output
                                                          f)))))
                                   (find-files #$elastix-output))))))

  (define spect-dirs
    (map run-dcm2niix dirs))

  (define spect-images
    (map (lambda (dir)
           (file-append dir "/image.nii.gz"))
         spect-dirs))

  (define registered-spect-dirs
    (let ((fixed (car spect-images)))
      (map (lambda (moving)
             (if split-build?
                 (clamp-elastix-output (run-elastix fixed moving) verbose?)
                 (run-elastix-and-clamp fixed moving verbose?)))
           (cdr spect-images))))

  (define registered-images
    (cons (car spect-images)
          (map (lambda (dir)
                 (file-append dir "/result.0.nii.gz"))
               registered-spect-dirs)))

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
                  (iota (length (list #$@registered-spect-dirs)) 2)
                  (list #$@registered-spect-dirs))

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
