// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/api/api_handler.h"

#include "base/json/json_reader.h"
#include "base/logging.h"

namespace ai_browser {

// static
base::Value::Dict ApiHandler::ParseRequestBody(const std::string& body) {
  auto parsed = base::JSONReader::Read(body);
  if (parsed && parsed->is_dict()) {
    return std::move(parsed->GetDict());
  }
  return base::Value::Dict();
}

// static
base::Value::Dict ApiHandler::BuildSuccessResponse(
    const base::Value::Dict& data) {
  base::Value::Dict response;
  response.Set("success", true);
  response.Set("data", data.Clone());
  return response;
}

// static
base::Value::Dict ApiHandler::BuildErrorResponse(
    int code,
    const std::string& message) {
  base::Value::Dict response;
  response.Set("success", false);
  response.Set("error_code", code);
  response.Set("error", message);
  return response;
}

}  // namespace ai_browser
