// SPDX-FileCopyrightText: 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_ZMQLOGGER_COMPAT_H
#define OPENSHOT_ZMQLOGGER_COMPAT_H
#include "Logger.h"
namespace openshot {
/// Deprecated source compatibility alias. Rebuild clients against this release.
using ZmqLogger = Logger;
}
#endif
