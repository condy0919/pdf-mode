;;; yapdf-view.el --- Yet another pdf dynamic module -*- lexical-binding: t -*-

;; Copyright (C) 2021 Zhiwei Chen
;; SPDX-License-Identifier: GPL-2.0-or-later
;; Author: Zhiwei Chen <condy0919@gmail.com>
;; Keywords: pdf, tools
;; URL: https://github.com/condy0919/pdf-mode
;; Version: 0.1
;; Package-Requires: ((emacs "24.4"))

;; This program is free software; you can redistribute it and/or modify it under
;; the terms of the GNU General Public License as published by the Free Software
;; Foundation, either version 3 of the License, or (at your option) any later
;; version.

;; This program is distributed in the hope that it will be useful, but WITHOUT
;; ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
;; FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
;; details.

;; You should have received a copy of the GNU General Public License along with
;; this program. If not, see <http://www.gnu.org/licenses/>.


;;; Commentary:
;; TODO docs

;;; Code:

;; Don't require dynamic module at byte compile time.
(declare-function yapdf--new "libyapdf")
(declare-function yapdf--hide "libyapdf")
(declare-function yapdf--show "libyapdf")

(defvar yapdf--buffers nil)
(defvar-local yapdf--id nil)

(define-derived-mode yapdf-view-mode special-mode "yapdf-view-mode"
  "Major mode for pdf viewing.

\\{yapdf-view-mode-map}"
  (setq buffer-read-only nil)
  ;; (add-hook 'kill-buffer-hook #'yapdf--kill-buffer nil t)
  )

(defun yapdf--adjust-size (_frame)
  (ignore _frame)
  (dolist (buffer yapdf--buffers)
    (when (buffer-live-p buffer)
      (with-current-buffer buffer
        (let ((windows (get-buffer-window-list buffer 'nomini t)))
          (if (not windows)
              (yapdf--hide yapdf--id)
            (yapdf--show yapdf--id)))))))

(defun yapdf--kill-buffer ()
  (yapdf--hide yapdf--id)
  (yapdf--destroy yapdf--id)
  (setq yapdf--buffers (delq (current-buffer) yapdf--buffers)))

(defun yapdf--filter (proc str)
  (when (buffer-live-p (process-buffer proc))
    (with-current-buffer (process-buffer proc)
      (goto-char (point-max))
      (insert str))))

(defun yapdf-new (&optional file)
  "Create a new yapdf with FILE."
  (interactive)
  (let ((buffer (generate-new-buffer "*yapdf*")))
    (with-current-buffer buffer
      (yapdf-view-mode)
      (setq cursor-type nil)
      (setq yapdf--id (yapdf--new (make-pipe-process :name "yapdf"
                                                     :buffer buffer
                                                     :filter 'yapdf--filter
                                                     :noquery t)))
      (push buffer yapdf--buffers)
      (switch-to-buffer buffer))))

(add-hook 'window-size-change-functions #'yapdf--adjust-size)

(provide 'yapdf-view)
;;; yapdf-view.el ends here
