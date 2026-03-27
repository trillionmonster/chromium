// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_API_API_HANDLER_H_
#define CHROME_BROWSER_AI_BROWSER_API_API_HANDLER_H_

#include <string>
#include "base/values.h"

namespace ai_browser {

// Utility functions for REST API request/response handling.
class ApiHandler {
 public:
  // Parse a JSON string into a Dict. Returns empty Dict on failure.
  static base::Value::Dict ParseRequestBody(const std::string& body);

  // Build a standard success response.
  static base::Value::Dict BuildSuccessResponse(
      const base::Value::Dict& data);

  // Build a standard error response.
  static base::Value::Dict BuildErrorResponse(int code,
                                              const std::string& message);
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_API_API_HANDLER_H_
