// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_client.h"

#include <utility>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ai_browser/ai_browser_prefs.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ai_browser {

namespace {

constexpr int kMaxResponseSize = 10 * 1024 * 1024;  // 10 MB

constexpr net::NetworkTrafficAnnotationTag kClaudeTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ai_browser_claude_api", R"(
    semantics {
      sender: "AI Browser"
      description: "Calls Claude API for AI-powered page analysis and QA."
      trigger: "User initiates a page analysis or search query."
      data: "Page text content or screenshots in base64."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting: "Configure in chrome://ai-settings"
    })");

constexpr net::NetworkTrafficAnnotationTag kOCRTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ai_browser_ocr", R"(
    semantics {
      sender: "AI Browser"
      description: "Calls local PaddleOCR server for text extraction."
      trigger: "Page content extraction when DOM text is insufficient."
      data: "Screenshot images in base64 or PNG format."
      destination: LOCAL
    }
    policy {
      cookies_allowed: NO
      setting: "Configure in chrome://ai-settings"
    })");

}  // namespace

AIClient::AIClient(
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : prefs_(prefs), url_loader_factory_(std::move(url_loader_factory)) {}

AIClient::~AIClient() = default;

// -- Public API --

void AIClient::AskLLM(const std::string& question,
                       const std::string& context,
                       LLMCallback callback) {
  std::string prompt;
  if (!context.empty()) {
    prompt = "Based on the following web page content:\n\n" + context +
             "\n\nQuestion: " + question;
  } else {
    prompt = question;
  }

  std::vector<base::Value::Dict> content_blocks;
  base::Value::Dict text_block;
  text_block.Set("type", "text");
  text_block.Set("text", prompt);
  content_blocks.push_back(std::move(text_block));

  std::string body = BuildClaudeRequestBody(
      "You are an AI browser assistant. Analyze web page content and answer "
      "questions accurately. Respond in the same language as the question.",
      content_blocks);
  CallClaudeAPI(body, std::move(callback));
}

void AIClient::AnalyzeScreenshot(const std::vector<uint8_t>& png_data,
                                 const std::string& question,
                                 VisionCallback callback) {
  std::string base64_image = base::Base64Encode(png_data);

  std::vector<base::Value::Dict> content_blocks;

  // Image block.
  base::Value::Dict source;
  source.Set("type", "base64");
  source.Set("media_type", "image/png");
  source.Set("data", base64_image);

  base::Value::Dict image_block;
  image_block.Set("type", "image");
  image_block.Set("source", std::move(source));
  content_blocks.push_back(std::move(image_block));

  // Text block.
  base::Value::Dict text_block;
  text_block.Set("type", "text");
  text_block.Set("text", question);
  content_blocks.push_back(std::move(text_block));

  std::string body = BuildClaudeRequestBody(
      "You are an AI browser assistant with vision. Analyze the screenshot "
      "and answer the question. Extract all visible text if asked.",
      content_blocks);
  CallClaudeAPI(body, std::move(callback));
}

void AIClient::OCRImage(const std::vector<uint8_t>& png_data,
                        OCRCallback callback) {
  std::string ocr_endpoint = GetOCREndpoint();
  if (ocr_endpoint.empty()) {
    std::move(callback).Run(
        OCRResult{false, "", "OCR endpoint not configured"});
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(ocr_endpoint + "/predict/ocr_system");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  // Build PaddleOCR request body: {"images": ["base64..."]}
  base::Value::Dict body;
  base::Value::List images;
  images.Append(base::Base64Encode(png_data));
  body.Set("images", std::move(images));

  std::string body_str;
  base::JSONWriter::Write(body, &body_str);

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                  kOCRTrafficAnnotation);
  loader->AttachStringForUpload(body_str, "application/json");
  loader->SetTimeoutDuration(base::Seconds(30));

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AIClient::OnOCRResponse, weak_factory_.GetWeakPtr(),
                     std::move(callback), std::move(loader)),
      kMaxResponseSize);
}

void AIClient::SolveCaptcha(const std::vector<uint8_t>& captcha_png,
                            CaptchaCallback callback) {
  AnalyzeScreenshot(
      captcha_png,
      "This image contains a CAPTCHA. Please read the characters/numbers "
      "shown in the CAPTCHA image and return ONLY the text/numbers, nothing "
      "else. No explanation needed.",
      base::BindOnce(
          [](CaptchaCallback cb, LLMResult result) {
            if (result.success) {
              // Trim whitespace from the CAPTCHA answer.
              std::string answer = result.text;
              base::TrimWhitespaceASCII(answer, base::TRIM_ALL, &answer);
              std::move(cb).Run(answer);
            } else {
              std::move(cb).Run(std::string());
            }
          },
          std::move(callback)));
}

void AIClient::PlanActions(const std::string& page_state,
                           const std::string& goal,
                           PlanCallback callback) {
  std::string prompt =
      "Current page state:\n" + page_state +
      "\n\nGoal: " + goal +
      "\n\nPlan the next browser actions to achieve this goal. "
      "Return a JSON array of actions, each with:\n"
      "- type: \"click\", \"type\", \"scroll\", \"navigate\", or \"wait\"\n"
      "- selector: CSS selector (for click/type)\n"
      "- value: text to type, URL to navigate to, or pixels to scroll\n\n"
      "Return ONLY valid JSON, no other text.";

  AskLLM(prompt, "",
         base::BindOnce(
             [](PlanCallback cb, LLMResult result) {
               if (!result.success) {
                 std::move(cb).Run(false, {});
                 return;
               }
               auto parsed = base::JSONReader::Read(result.text);
               if (!parsed || !parsed->is_list()) {
                 std::move(cb).Run(false, {});
                 return;
               }
               std::vector<PlannedAction> actions;
               for (const auto& item : parsed->GetList()) {
                 if (!item.is_dict()) continue;
                 const auto& dict = item.GetDict();
                 PlannedAction action;
                 if (auto* t = dict.FindString("type")) action.type = *t;
                 if (auto* s = dict.FindString("selector"))
                   action.selector = *s;
                 if (auto* v = dict.FindString("value")) action.value = *v;
                 actions.push_back(std::move(action));
               }
               std::move(cb).Run(true, std::move(actions));
             },
             std::move(callback)));
}

void AIClient::TestLLMConnection(
    base::OnceCallback<void(bool, std::string)> callback) {
  AskLLM("Say 'hello' in one word.", "",
         base::BindOnce(
             [](base::OnceCallback<void(bool, std::string)> cb,
                LLMResult result) {
               if (result.success) {
                 std::move(cb).Run(true, "Connected. Response: " + result.text);
               } else {
                 std::move(cb).Run(false, "Error: " + result.error);
               }
             },
             std::move(callback)));
}

void AIClient::TestOCRConnection(
    base::OnceCallback<void(bool, std::string)> callback) {
  // Send a minimal white 1x1 PNG to test connectivity.
  std::vector<uint8_t> test_png = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,  // PNG header
      0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,  // IHDR chunk
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
      0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
      0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
      0x00, 0x00, 0x02, 0x00, 0x01, 0xE2, 0x21, 0xBC,
      0x33, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
      0x44, 0xAE, 0x42, 0x60, 0x82};

  OCRImage(test_png,
           base::BindOnce(
               [](base::OnceCallback<void(bool, std::string)> cb,
                  OCRResult result) {
                 if (result.success) {
                   std::move(cb).Run(true, "OCR server connected.");
                 } else {
                   std::move(cb).Run(false, "OCR error: " + result.error);
                 }
               },
               std::move(callback)));
}

// -- Private implementation --

std::string AIClient::BuildClaudeRequestBody(
    const std::string& system_prompt,
    const std::vector<base::Value::Dict>& content_blocks) {
  base::Value::Dict body;
  body.Set("model", GetModel());
  body.Set("max_tokens", GetMaxTokens());

  if (!system_prompt.empty()) {
    body.Set("system", system_prompt);
  }

  // Build messages array.
  base::Value::List messages;
  base::Value::Dict user_message;
  user_message.Set("role", "user");

  base::Value::List content;
  for (const auto& block : content_blocks) {
    content.Append(block.Clone());
  }
  user_message.Set("content", std::move(content));
  messages.Append(std::move(user_message));
  body.Set("messages", std::move(messages));

  std::string json;
  base::JSONWriter::Write(body, &json);
  return json;
}

void AIClient::CallClaudeAPI(
    const std::string& json_body,
    base::OnceCallback<void(LLMResult)> callback) {
  std::string api_key = GetApiKey();
  std::string endpoint = GetLLMEndpoint();

  if (api_key.empty()) {
    std::move(callback).Run(
        LLMResult{false, "", "API key not configured", 0, 0});
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(endpoint);
  resource_request->method = "POST";
  resource_request->headers.SetHeader("x-api-key", api_key);
  resource_request->headers.SetHeader("anthropic-version", "2023-06-01");
  resource_request->headers.SetHeader("Content-Type", "application/json");
  resource_request->credentials_mode =
      network::mojom::CredentialsMode::kOmit;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                  kClaudeTrafficAnnotation);
  loader->AttachStringForUpload(json_body, "application/json");
  loader->SetTimeoutDuration(base::Seconds(120));
  loader->SetRetryOptions(
      2, network::SimpleURLLoader::RETRY_ON_5XX |
             network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AIClient::OnClaudeResponse, weak_factory_.GetWeakPtr(),
                     std::move(callback), std::move(loader)),
      kMaxResponseSize);
}

LLMResult AIClient::ParseClaudeResponse(const std::string& json_response) {
  LLMResult result;
  auto parsed = base::JSONReader::Read(json_response);
  if (!parsed || !parsed->is_dict()) {
    result.error = "Failed to parse API response JSON";
    return result;
  }

  const base::Value::Dict& dict = parsed->GetDict();

  // Check for error.
  if (const auto* error = dict.FindDict("error")) {
    if (const auto* msg = error->FindString("message")) {
      result.error = *msg;
    } else {
      result.error = "Unknown API error";
    }
    return result;
  }

  // Extract text from content blocks.
  const auto* content = dict.FindList("content");
  if (content) {
    for (const auto& block : *content) {
      if (!block.is_dict()) continue;
      const auto& block_dict = block.GetDict();
      const auto* type = block_dict.FindString("type");
      if (type && *type == "text") {
        const auto* text = block_dict.FindString("text");
        if (text) {
          if (!result.text.empty()) {
            result.text += "\n";
          }
          result.text += *text;
        }
      }
    }
  }

  // Extract usage info.
  if (const auto* usage = dict.FindDict("usage")) {
    result.input_tokens = usage->FindInt("input_tokens").value_or(0);
    result.output_tokens = usage->FindInt("output_tokens").value_or(0);
  }

  result.success = !result.text.empty();
  return result;
}

void AIClient::OnClaudeResponse(
    base::OnceCallback<void(LLMResult)> callback,
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> response_body) {
  if (!response_body.has_value()) {
    int net_error = loader->NetError();
    LLMResult result;
    result.error = "Network error: " + std::to_string(net_error);
    if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
      result.error +=
          " (HTTP " +
          std::to_string(loader->ResponseInfo()->headers->response_code()) +
          ")";
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  std::move(callback).Run(ParseClaudeResponse(*response_body));
}

void AIClient::OnOCRResponse(
    OCRCallback callback,
    std::unique_ptr<network::SimpleURLLoader> loader,
    std::optional<std::string> response_body) {
  OCRResult result;

  if (!response_body.has_value()) {
    result.error = "OCR network error: " + std::to_string(loader->NetError());
    std::move(callback).Run(std::move(result));
    return;
  }

  // Parse PaddleOCR response.
  // Expected format: {"results": [{"data": [{"text": "...", "confidence": 0.9}]}]}
  auto parsed = base::JSONReader::Read(*response_body);
  if (!parsed || !parsed->is_dict()) {
    result.error = "Failed to parse OCR response";
    std::move(callback).Run(std::move(result));
    return;
  }

  const auto& dict = parsed->GetDict();
  const auto* results = dict.FindList("results");
  if (!results || results->empty()) {
    // Try alternate format: {"result": [{"text": "..."}]}
    const auto* alt_results = dict.FindList("result");
    if (alt_results) {
      for (const auto& item : *alt_results) {
        if (item.is_dict()) {
          if (const auto* text = item.GetDict().FindString("text")) {
            if (!result.text.empty()) result.text += "\n";
            result.text += *text;
          }
        }
      }
      result.success = !result.text.empty();
    } else {
      result.error = "No OCR results in response";
    }
    std::move(callback).Run(std::move(result));
    return;
  }

  // Parse standard PaddleOCR Serving format.
  for (const auto& res : *results) {
    if (!res.is_dict()) continue;
    const auto* data = res.GetDict().FindList("data");
    if (!data) continue;
    for (const auto& item : *data) {
      if (!item.is_dict()) continue;
      if (const auto* text = item.GetDict().FindString("text")) {
        if (!result.text.empty()) result.text += "\n";
        result.text += *text;
      }
    }
  }

  result.success = !result.text.empty();
  if (!result.success && result.error.empty()) {
    result.error = "OCR returned no text";
  }
  std::move(callback).Run(std::move(result));
}

std::string AIClient::GetApiKey() const {
  return prefs_->GetString(kLLMApiKey);
}

std::string AIClient::GetModel() const {
  return prefs_->GetString(kLLMModel);
}

std::string AIClient::GetLLMEndpoint() const {
  return prefs_->GetString(kLLMEndpoint);
}

std::string AIClient::GetOCREndpoint() const {
  return prefs_->GetString(kOCREndpoint);
}

int AIClient::GetMaxTokens() const {
  return prefs_->GetInteger(kLLMMaxTokens);
}

}  // namespace ai_browser
