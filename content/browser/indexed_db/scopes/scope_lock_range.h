// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_
#define CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_

#include <iosfwd>
#include <string>

#include "base/logging.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {

// The range is [begin, end).
struct CONTENT_EXPORT ScopeLockRange {
  ScopeLockRange(std::string begin, std::string end);
  ScopeLockRange() = default;
  ~ScopeLockRange() = default;
  std::string begin;
  std::string end;
};

// Logging support.
CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const ScopeLockRange& range);

CONTENT_EXPORT bool operator<(const ScopeLockRange& x, const ScopeLockRange& y);
CONTENT_EXPORT bool operator==(const ScopeLockRange& x,
                               const ScopeLockRange& y);
CONTENT_EXPORT bool operator!=(const ScopeLockRange& x,
                               const ScopeLockRange& y);

}  // namespace content

#endif /* CONTENT_BROWSER_INDEXED_DB_SCOPES_SCOPE_LOCK_RANGE_H_ */
