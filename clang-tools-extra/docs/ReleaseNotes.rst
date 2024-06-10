====================================================
Extra Clang Tools |release| |ReleaseNotesTitle|
====================================================

.. contents::
   :local:
   :depth: 3

Written by the `LLVM Team <https://llvm.org/>`_

.. only:: PreRelease

  .. warning::
     These are in-progress notes for the upcoming Extra Clang Tools |version| release.
     Release notes for previous releases can be found on
     `the Download Page <https://releases.llvm.org/download.html>`_.

Introduction
============

This document contains the release notes for the Extra Clang Tools, part of the
Clang release |release|. Here we describe the status of the Extra Clang Tools in
some detail, including major improvements from the previous release and new
feature work. All LLVM releases may be downloaded from the `LLVM releases web
site <https://llvm.org/releases/>`_.

For more information about Clang or LLVM, including information about
the latest release, please see the `Clang Web Site <https://clang.llvm.org>`_ or
the `LLVM Web Site <https://llvm.org>`_.

Note that if you are reading this file from a Git checkout or the
main Clang web page, this document applies to the *next* release, not
the current one. To see the release notes for a specific release, please
see the `releases page <https://llvm.org/releases/>`_.

What's New in Extra Clang Tools |release|?
==========================================

Some of the major new features and improvements to Extra Clang Tools are listed
here. Generic improvements to Extra Clang Tools as a whole or to its underlying
infrastructure are described first, followed by tool-specific sections.

Major New Features
------------------

...

Improvements to clangd
----------------------

Inlay hints
^^^^^^^^^^^

Diagnostics
^^^^^^^^^^^

Semantic Highlighting
^^^^^^^^^^^^^^^^^^^^^

Compile flags
^^^^^^^^^^^^^

Hover
^^^^^

Code completion
^^^^^^^^^^^^^^^

Code actions
^^^^^^^^^^^^

Signature help
^^^^^^^^^^^^^^

Cross-references
^^^^^^^^^^^^^^^^

Objective-C
^^^^^^^^^^^

Miscellaneous
^^^^^^^^^^^^^

Improvements to clang-doc
-------------------------

Improvements to clang-query
---------------------------

The improvements are...

Improvements to clang-rename
----------------------------

The improvements are...

Improvements to clang-tidy
--------------------------

New checks
^^^^^^^^^^

- New :doc:`bugprone-casting-through-void
  <clang-tidy/checks/bugprone/casting-through-void>` check.

  Detects unsafe or redundant two-step casting operations involving ``void*``.

- New :doc:`bugprone-compare-pointer-to-member-virtual-function
  <clang-tidy/checks/bugprone/compare-pointer-to-member-virtual-function>` check.

  Detects equality comparison between pointer to member virtual function and
  anything other than null-pointer-constant.

- New :doc:`bugprone-inc-dec-in-conditions
  <clang-tidy/checks/bugprone/inc-dec-in-conditions>` check.

  Detects when a variable is both incremented/decremented and referenced inside
  a complex condition and suggests moving them outside to avoid ambiguity in
  the variable's value.

- New :doc:`bugprone-incorrect-enable-if
  <clang-tidy/checks/bugprone/incorrect-enable-if>` check.

  Detects incorrect usages of ``std::enable_if`` that don't name the nested
  ``type`` type.

- New :doc:`bugprone-multi-level-implicit-pointer-conversion
  <clang-tidy/checks/bugprone/multi-level-implicit-pointer-conversion>` check.

  Detects implicit conversions between pointers of different levels of
  indirection.

- New :doc:`bugprone-optional-value-conversion
  <clang-tidy/checks/bugprone/optional-value-conversion>` check.

  Detects potentially unintentional and redundant conversions where a value is
  extracted from an optional-like type and then used to create a new instance
  of the same optional-like type.

- New :doc:`bugprone-unused-local-non-trivial-variable
  <clang-tidy/checks/bugprone/unused-local-non-trivial-variable>` check.

  Warns when a local non trivial variable is unused within a function.

- New :doc:`cppcoreguidelines-no-suspend-with-lock
  <clang-tidy/checks/cppcoreguidelines/no-suspend-with-lock>` check.

  Flags coroutines that suspend while a lock guard is in scope at the
  suspension point.

- New :doc:`hicpp-ignored-remove-result
  <clang-tidy/checks/hicpp/ignored-remove-result>` check.

  Ensure that the result of ``std::remove``, ``std::remove_if`` and
  ``std::unique`` are not ignored according to rule 17.5.1.

- New :doc:`misc-coroutine-hostile-raii
  <clang-tidy/checks/misc/coroutine-hostile-raii>` check.

  Detects when objects of certain hostile RAII types persists across suspension
  points in a coroutine. Such hostile types include scoped-lockable types and
  types belonging to a configurable denylist.

- New :doc:`modernize-use-constraints
  <clang-tidy/checks/modernize/use-constraints>` check.

  Replace ``enable_if`` with C++20 requires clauses.

- New :doc:`modernize-use-starts-ends-with
  <clang-tidy/checks/modernize/use-starts-ends-with>` check.

  Checks whether a ``find`` or ``rfind`` result is compared with 0 and suggests
  replacing with ``starts_with`` when the method exists in the class. Notably,
  this will work with ``std::string`` and ``std::string_view``.

- New :doc:`modernize-use-std-numbers
  <clang-tidy/checks/modernize/use-std-numbers>` check.

  Finds constants and function calls to math functions that can be replaced
  with C++20's mathematical constants from the ``numbers`` header and
  offers fix-it hints.

- New :doc:`performance-enum-size
  <clang-tidy/checks/performance/enum-size>` check.

  Recommends the smallest possible underlying type for an ``enum`` or ``enum``
  class based on the range of its enumerators.

- New :doc:`performance-vector-initializer-list
  <clang-tidy/checks/performance/vector-initializer-list>` check.

  FIXME: add release notes.

- New :doc:`readability-avoid-nested-conditional-operator
  <clang-tidy/checks/readability/avoid-nested-conditional-operator>` check.

  Identifies instances of nested conditional operators in the code.

- New :doc:`readability-avoid-return-with-void-value
  <clang-tidy/checks/readability/avoid-return-with-void-value>` check.

  Finds return statements with ``void`` values used within functions with
  ``void`` result types.

- New :doc:`readability-redundant-casting
  <clang-tidy/checks/readability/redundant-casting>` check.

  Detects explicit type casting operations that involve the same source and
  destination types, and subsequently recommend their removal.
  
- New :doc:`readability-redundant-inline-specifier
  <clang-tidy/checks/readability/redundant-inline-specifier>` check.

  Detects redundant ``inline`` specifiers on function and variable declarations.

- New :doc:`readability-reference-to-constructed-temporary
  <clang-tidy/checks/readability/reference-to-constructed-temporary>` check.

  Detects C++ code where a reference variable is used to extend the lifetime
  of a temporary object that has just been constructed.

New check aliases
^^^^^^^^^^^^^^^^^

Changes in existing checks
^^^^^^^^^^^^^^^^^^^^^^^^^^

- Improved :doc:`bugprone-suspicious-include
  <clang-tidy/checks/bugprone/suspicious-include>` check by replacing the local
  options `HeaderFileExtensions` and `ImplementationFileExtensions` by the
  global options of the same name.

- Cleaned up :doc:`cppcoreguidelines-prefer-member-initializer
  <clang-tidy/checks/cppcoreguidelines/prefer-member-initializer>`
  by removing enforcement of rule `C.48
  <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c48-prefer-in-class-initializers-to-member-initializers-in-constructors-for-constant-initializers>`_,
  which was deprecated since :program:`clang-tidy` 17. This rule is now covered
  by :doc:`cppcoreguidelines-use-default-member-init
  <clang-tidy/checks/cppcoreguidelines/use-default-member-init>`.

- Improved :doc:`google-build-namespaces
  <clang-tidy/checks/google/build-namespaces>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.

- Improved :doc:`google-global-names-in-headers
  <clang-tidy/checks/google/global-names-in-headers>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.

- Improved :doc:`llvm-header-guard
  <clang-tidy/checks/llvm/header-guard>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.

- Improved :doc:`misc-definitions-in-headers
  <clang-tidy/checks/misc/definitions-in-headers>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.
  Additionally, the option `UseHeaderFileExtensions` is removed, so that the
  check uses the `HeaderFileExtensions` option unconditionally.

- Improved :doc:`misc-unused-using-decls
  <clang-tidy/checks/misc/unused-using-decls>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.

- Improved :doc:`misc-use-anonymous-namespace
  <clang-tidy/checks/misc/use-anonymous-namespace>` check by replacing the local
  option `HeaderFileExtensions` by the global option of the same name.

- Improved :doc:`modernize-avoid-c-arrays
  <clang-tidy/checks/modernize/avoid-c-arrays>` check by introducing the new
  `AllowStringArrays` option, enabling the exclusion of array types with deduced
  length initialized from string literals.

Removed checks
^^^^^^^^^^^^^^

Miscellaneous
^^^^^^^^^^^^^

- Removed `cert-dcl21-cpp`, which was deprecated since :program:`clang-tidy` 17,
  since the rule DCL21-CPP has been removed from the CERT guidelines.

- Fixed incorrect formatting in ``clang-apply-repalcements`` when no ``--format``
  option is specified. Now ``clang-apply-replacements`` applies formatting only with
  the option.

Improvements to include-fixer
-----------------------------

The improvements are...

Improvements to clang-include-fixer
-----------------------------------

The improvements are...

Improvements to modularize
--------------------------

The improvements are...

Improvements to pp-trace
------------------------

Clang-tidy Visual Studio plugin
-------------------------------
