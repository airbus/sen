;;; stl-mode.el --- Major mode for Sen STL (Sen Type Language) -*- lexical-binding: t -*-

;; Copyright (C) Airbus SAS, Airbus Helicopters, Airbus Defence and Space
;; License: Apache-2.0
;; Keywords: languages

;;; Commentary:
;;
;; Simple major mode providing font-lock, indentation, and comment support
;; for Sen STL (`.stl`) interface-definition files.
;;
;; To enable:
;;   (add-to-list 'load-path "/path/to/stl-mode")
;;   (require 'stl-mode)
;;

;;; Code:

(defvar stl-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; `//` starts a line comment up to end-of-line.
    (modify-syntax-entry ?/  ". 12" table)
    (modify-syntax-entry ?\n ">"    table)
    ;; Strings
    (modify-syntax-entry ?\" "\"" table)
    (modify-syntax-entry ?\' "\"" table)
    ;; Word constituents
    (modify-syntax-entry ?_ "_" table)
    table)
  "Syntax table for `stl-mode'.")

(defconst stl-storage-type-keywords
  '("class" "struct" "enum" "variant" "interface"
    "sequence" "array" "optional" "quantity" "alias"))

(defconst stl-member-keywords
  '("var" "fn" "event"))

(defconst stl-control-keywords
  '("package" "import" "extends" "implements" "abstract"))

(defconst stl-attribute-keywords
  '("static" "static_no_config" "writable" "confirmed" "bestEffort"
    "const" "local" "tag" "min" "max"))

(defconst stl-primitive-type-keywords
  '("u8" "u16" "u32" "u64" "i16" "i32" "i64" "f32" "f64"
    "bool" "string" "TimeStamp" "Duration"))

(defconst stl-query-keywords
  '("SELECT" "FROM" "WHERE" "BETWEEN" "IN" "NOT" "AND" "OR"))

(defun stl--kw-regexp (kws)
  (concat "\\_<" (regexp-opt kws t) "\\_>"))

(defconst stl-font-lock-keywords
  `(
    ;; Declaration openers followed by the type name
    (,(concat (stl--kw-regexp stl-storage-type-keywords)
              "\\s-+\\([A-Za-z_][A-Za-z0-9_]*\\)")
     (1 font-lock-keyword-face)
     (2 font-lock-type-face))
    ;; `var name` / `fn name` / `event name`
    (,(concat (stl--kw-regexp stl-member-keywords)
              "\\s-+\\([A-Za-z_][A-Za-z0-9_]*\\)")
     (1 font-lock-keyword-face)
     (2 font-lock-function-name-face))
    ;; Control / inheritance keywords
    (,(stl--kw-regexp stl-control-keywords) 1 font-lock-keyword-face)
    ;; Query-language keywords reserved in STL
    (,(stl--kw-regexp stl-query-keywords) 1 font-lock-preprocessor-face)
    ;; Attributes
    (,(stl--kw-regexp stl-attribute-keywords) 1 font-lock-builtin-face)
    ;; Primitive types
    (,(stl--kw-regexp stl-primitive-type-keywords) 1 font-lock-type-face)
    ;; Booleans
    ("\\_<\\(true\\|false\\)\\_>" 1 font-lock-constant-face)
    ;; Float / integer literals
    ("-?\\_<[0-9]+\\.[0-9]+\\_>" . font-lock-constant-face)
    ("-?\\_<[0-9]+\\_>"          . font-lock-constant-face)
    ;; Type-like identifiers (uppercase-leading)
    ("\\_<[A-Z][A-Za-z0-9_]*\\_>" . font-lock-type-face)
    ;; Method return arrow
    ("->" . font-lock-keyword-face)
    ;; Documentation @param
    ("@param\\s-+\\([A-Za-z_][A-Za-z0-9_]*\\)"
     (0 font-lock-doc-face t)
     (1 font-lock-variable-name-face t))))

(defun stl-indent-line ()
  "Indent current line as Sen STL."
  (interactive)
  (let ((indent 0)
        (pos (- (point-max) (point))))
    (save-excursion
      (beginning-of-line)
      (if (bobp)
          (setq indent 0)
        (let ((prev-indent 0)
              (prev-opens-block nil))
          (save-excursion
            (forward-line -1)
            (while (and (not (bobp)) (looking-at "^\\s-*$"))
              (forward-line -1))
            (setq prev-indent (current-indentation))
            (end-of-line)
            (skip-chars-backward " \t")
            (when (eq (char-before) ?{)
              (setq prev-opens-block t)))
          (setq indent prev-indent)
          (when prev-opens-block
            (setq indent (+ indent tab-width)))
          (when (looking-at "\\s-*}")
            (setq indent (- indent tab-width)))))
      (when (< indent 0) (setq indent 0))
      (indent-line-to indent))
    (when (> (- (point-max) pos) (point))
      (goto-char (- (point-max) pos)))))

(define-derived-mode stl-mode prog-mode "STL"
  "Major mode for editing Sen STL (Sen Type Language) files."
  :syntax-table stl-mode-syntax-table
  (setq-local font-lock-defaults '(stl-font-lock-keywords))
  (setq-local comment-start "// ")
  (setq-local comment-end   "")
  (setq-local comment-start-skip "//+\\s-*")
  (setq-local indent-tabs-mode nil)
  (setq-local tab-width 2)
  (setq-local indent-line-function #'stl-indent-line))

(add-to-list 'auto-mode-alist '("\\.stl\\'" . stl-mode))

(provide 'stl-mode)
;;; stl-mode.el ends here
