// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AI_CLIENT_H_
#define CHROME_BROWSER_AI_BROWSER_AI_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ai_browser {

// Result from an LLM call.
struct LLMResult {
  bool success = false;
  std::string text;
  std::string error;
  int input_tokens = 0;
  int output_tokens = 0;
};

// Result from an OCR call.
struct OCRResult {
  bool success = false;
  std::string text;
  std::string error;
};

// A planned browser action from the LLM.
struct PlannedAction {
  std::string type;     // "click", "type", "scroll", "navigate", "wait"
  std::string selector; // CSS selector for target element
  std::string value;    // Text to type, URL to navigate to, etc.
};

using LLMCallback = base::OnceCallback<void(LLMResult)>;
using OCRCallback = base::OnceCallback<void(OCRResult)>;
using VisionCallback = base::OnceCallback<void(LLMResult)>;
using CaptchaCallback = base::OnceCallback<void(std::string answer)>;
using PlanCallback =
    base::OnceCallback<void(bool success, std::vector<PlannedAction> actions)>;

// Client for external AI services — Claude API for LLM/Vision, and
// PaddleOCR server for local OCR. All calls are asynchronous.
class AIClient {
 public:
  AIClient(PrefService* prefs,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AIClient();

  AIClient(const AIClient&) = delete;
  AIClient& operator=(const AIClient&) = delete;

  // Text-only LLM question answering.
  void AskLLM(const std::string& question,
              const std::string& context,
              LLMCallback callback);

  // Send a screenshot to Claude Vision for analysis.
  void AnalyzeScreenshot(const std::vector<uint8_t>& png_data,
                         const std::string& question,
                         VisionCallback callback);

  // Local OCR via PaddleOCR HTTP server.
  void OCRImage(const std::vector<uint8_t>& png_data,
                OCRCallback callback);

  // Solve a CAPTCHA using Claude Vision.
  void SolveCaptcha(const std::vector<uint8_t>& captcha_png,
                    CaptchaCallback callback);

  // Ask Claude to plan next browser actions given current page state.
  void PlanActions(const std::string& page_state,
                   const std::string& goal,
                   PlanCallback callback);

  // Check connectivity to Claude API.
  void TestLLMConnection(base::OnceCallback<void(bool, std::string)> callback);

  // Check connectivity to OCR server.
  void TestOCRConnection(base::OnceCallback<void(bool, std::string)> callback);

 private:
  // Build the JSON body for a Claude API call.
  std::string BuildClaudeRequestBody(
      const std::string& system_prompt,
      const std::vector<base::Value::Dict>& content_blocks);

  // Send a request to the Claude API.
  void CallClaudeAPI(const std::string& json_body,
                     base::OnceCallback<void(LLMResult)> callback);

  // Parse Claude API response JSON.
  LLMResult ParseClaudeResponse(const std::string& json_response);

  // Called when a SimpleURLLoader completes.
  void OnClaudeResponse(base::OnceCallback<void(LLMResult)> callback,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        std::optional<std::string> response_body);

  void OnOCRResponse(OCRCallback callback,
                     std::unique_ptr<network::SimpleURLLoader> loader,
                     std::optional<std::string> response_body);

  // Read current configuration from PrefService.
  std::string GetApiKey() const;
  std::string GetModel() const;
  std::string GetLLMEndpoint() const;
  std::string GetOCREndpoint() const;
  int GetMaxTokens() const;

  raw_ptr<PrefService> prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<AIClient> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AI_CLIENT_H_
