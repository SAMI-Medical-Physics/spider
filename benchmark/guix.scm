;;; SPDX-License-Identifier: AGPL-3.0-or-later
;;; Copyright (C) 2026 South Australia Medical Imaging

;;; Commentary:

;; Evaluate the performance of the Spider dosimetry pipeline by
;; applying it to publicly available datasets.  The benchmarks are ran
;; in an isolated, self-contained, and reproducible build environment
;; with bullet-proof caching (work re-use) using GNU Guix.

;;; Code:

(use-modules (git)
             (guix channels)            ;for channel->code
             (guix describe)            ;for current-channesl
             (guix gexp))

;; For tia-comparison.
(include "tia/guix.scm")

(define channels
  `(list ,@(map channel->code (current-channels))))

;; Courtesy: https://gitlab.inria.fr/lcourtes-phd/edcc-2006-redone
(define (this-commit)
  "Return the commit ID for the tip of this repository."
  (let* ((repository (repository-open "."))
         (head (repository-head repository))
         (target (reference-target head))
         (id (oid->string target)))
    (repository-close! repository)
    id))

(define python-docutils
  (specification->package "python-docutils"))

(computed-file "site"
               (with-imported-modules '((guix build utils))
                 #~(begin
                     (use-modules (guix build utils)
                                  (ice-9 pretty-print)
                                  (srfi srfi-13))  ; string-split, string-join
                     (define (pp->string x)
                       (with-output-to-string
                         (lambda () (pretty-print x))))

                     (define (indent-string s n)
                       (define prefix (make-string n #\space))
                       (string-join
                        (map (lambda (line) (string-append prefix line))
                             (string-split s #\newline))
                        "\n"))

                     (copy-file #$(local-file "index.rst") "index.rst")
                     (make-file-writable "index.rst")
                     (substitute* "index.rst"
                       ((".. Placeholder for provenance information.")
                        (format #f "\
Provenance
----------

These results were obtained with Spider commit ``~a``.
To reproduce the experiments, check out this commit of the `Spider
repository <spider-repository_>`_, then run ``guix build -f
benchmark/guix.scm`` using the following revision of `Guix
<https://guix.gnu.org>`_::

~a

To run the experiments on Unix-like operating systems without Guix,
build Spider with ``-DSPIDER_BUILD_BENCHMARKS=ON``, then run
``benchmark/run.sh`` in the build directory.
But running the experiments this way is not reproducible."
                                #$(this-commit)
                                (indent-string (pp->string '#$channels) 2))))

                     (mkdir-p #$output)
                     ;; (install-file "index.rst" #$output) ;for debugging
                     (invoke (string-append #$python-docutils "/bin/rst2html5")
                             "index.rst"
                             (string-append #$output "/index.html"))
                     (mkdir-p (string-append #$output "/tia"))
                     (symlink (string-append #$tia-comparison "/hist-.svg")
                              (string-append #$output "/tia/hist-.svg"))
                     (symlink (string-append #$tia-comparison "/hist-1e10.svg")
                              (string-append #$output "/tia/hist-1e10.svg")))))
