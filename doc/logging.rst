.. SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
.. SPDX-License-Identifier: LGPL-3.0-or-later

Logging
=========

``openshot::Logger`` provides synchronous, thread-safe ordinary logging to an
append-only file and stderr, with independent severity thresholds. It uses
standard C++ and requires no ZeroMQ dependency or forwarding thread.

Configuration
-------------

Both thresholds default to INFO. Supported levels are DEBUG, INFO, WARNING,
ERROR, CRITICAL, and OFF (case-insensitive). Existing method traces use DEBUG.
No file is opened until a path is supplied through the API or environment.

.. code-block:: cpp

   #include "Logger.h"

   auto* logger = openshot::Logger::Instance();
   logger->Path("render.log");             // UTF-8; relative to the working directory
   logger->SetFileLevel("DEBUG");
   logger->SetConsoleLevel("WARNING");
   logger->Log("Starting export", openshot::Logger::LevelInfo);

The same API is exposed by the Python binding. For a standalone script:

.. code-block:: bash

   LIBOPENSHOT_LOG_FILE=render.log LIBOPENSHOT_LOG_LEVEL=debug python3 render.py

.. list-table:: Environment variables
   :header-rows: 1
   :widths: 50 50

   * - Variable
     - Scope
   * - ``LIBOPENSHOT_LOG_FILE``
     - Output file path; no standalone default
   * - ``LIBOPENSHOT_LOG_LEVEL``
     - File and stderr thresholds
   * - ``LIBOPENSHOT_LOG_FILE_LEVEL``
     - File threshold
   * - ``LIBOPENSHOT_LOG_CONSOLE_LEVEL``
     - Stderr threshold
   * - ``OPENSHOT_LOG_LEVEL``
     - Shared fallback for both thresholds
   * - ``OPENSHOT_LOG_FILE_LEVEL`` / ``OPENSHOT_LOG_CONSOLE_LEVEL``
     - Shared fallback for the respective threshold
   * - ``LIBOPENSHOT_DEBUG``
     - Legacy stderr DEBUG fallback; any present value, including ``0``

Environment variables are read at singleton initialization. Precedence is
explicit API configuration > ``LIBOPENSHOT_`` variables > ``OPENSHOT_`` variables
> legacy debug switch > INFO. Within each variable group, destination-specific
levels override the general level. Invalid environment levels emit a diagnostic
and fall back; invalid API levels throw ``std::invalid_argument``.

In openshot-qt, the host resolves CLI > environment > saved preferences > defaults
and configures this API. ``--debug-engine`` enables engine DEBUG in its file and
stderr; **Video & Audio Engine Debug Logging** affects only ``libopenshot.log``.
``--debug-ui`` (aliases ``--debug``, ``-d``) and **User Interface Debug Logging**
control the separate Python logger and ``openshot-qt.log``. The GUI selects its
own file path under ``~/.openshot_qt/``.

Behavior and compatibility
--------------------------

* Records include local time, severity, and thread ID. ``ShouldLog(level)`` can
  guard expensive message construction. File writes flush per record; there is
  no rotation or asynchronous queue. DEBUG can generate substantial I/O; avoid
  logging from real-time audio callbacks.
* ``Close()`` closes the file and disables ordinary output. Reopening requires
  setting the path and desired levels again.
* ``ZmqLogger`` is a deprecated C++/Python source alias; rebuild consumers and
  bindings. ``Connection()`` is a no-op. ``Enable(true/false)`` selects file
  DEBUG/OFF. ``Settings::DEBUG_TO_STDERR`` remains a legacy console switch,
  overridden by explicit modern console configuration.
* ``LogToFile()`` preserves the existing unfiltered raw crash-write path and
  does not acquire the ordinary logger mutex. Crash markers and error handling
  are unchanged. Stream flushing does not guarantee durability after power loss.
