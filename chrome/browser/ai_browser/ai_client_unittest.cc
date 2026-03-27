// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ai_browser/ai_browser_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

class AIClientTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
    prefs_.SetString(kLLMApiKey, "sk-test-key");
    prefs_.SetString(kLLMModel, "claude-sonnet-4-20250514");
    prefs_.SetString(kLLMEndpoint,
                     "https://api.anthropic.com/v1/messages");
    prefs_.SetInteger(kLLMMaxTokens, 4096);
    prefs_.SetString(kOCREndpoint, "http://localhost:8866");

    auto factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    client_ = std::make_unique<AIClient>(&prefs_, factory);
  }

  // Build a mock Claude API successful response JSON string.
  std::string BuildClaudeSuccessResponse(const std::string& text) {
    base::Value::Dict response;
    response.Set("id", "msg_test123");
    response.Set("type", "message");
    response.Set("role", "assistant");

    base::Value::List content;
    base::Value::Dict text_block;
    text_block.Set("type", "text");
    text_block.Set("text", text);
    content.Append(std::move(text_block));
    response.Set("content", std::move(content));

    base::Value::Dict usage;
    usage.Set("input_tokens", 100);
    usage.Set("output_tokens", 50);
    response.Set("usage", std::move(usage));

    std::string json;
    base::JSONWriter::Write(response, &json);
    return json;
  }

  // Build a mock Claude API error response JSON string.
  std::string BuildClaudeErrorResponse(const std::string& message) {
    base::Value::Dict response;
    base::Value::Dict error;
    error.Set("type", "invalid_request_error");
    error.Set("message", message);
    response.Set("error", std::move(error));

    std::string json;
    base::JSONWriter::Write(response, &json);
    return json;
  }

  // Build a mock PaddleOCR successful response.
  std::string BuildOCRSuccessResponse(
      const std::vector<std::string>& texts) {
    base::Value::Dict response;
    base::Value::List results;
    base::Value::Dict result_item;
    base::Value::List data;
    for (const auto& text : texts) {
      base::Value::Dict item;
      item.Set("text", text);
      item.Set("confidence", 0.95);
      data.Append(std::move(item));
    }
    result_item.Set("data", std::move(data));
    results.Append(std::move(result_item));
    response.Set("results", std::move(results));

    std::string json;
    base::JSONWriter::Write(response, &json);
    return json;
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<AIClient> client_;
};

// === AskLLM Tests ===

TEST_F(AIClientTest, AskLLMSuccessful) {
  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages",
      BuildClaudeSuccessResponse("The answer is 42."));

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "What is the meaning?", "context text",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "The answer is 42.");
  EXPECT_EQ(captured_result.input_tokens, 100);
  EXPECT_EQ(captured_result.output_tokens, 50);
  EXPECT_TRUE(captured_result.error.empty());
}

TEST_F(AIClientTest, AskLLMWithEmptyContext) {
  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages",
      BuildClaudeSuccessResponse("Hello!"));

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "Say hello", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "Hello!");
}

TEST_F(AIClientTest, AskLLMAPIError) {
  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages",
      BuildClaudeErrorResponse("Invalid API key"));

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_EQ(captured_result.error, "Invalid API key");
  EXPECT_TRUE(captured_result.text.empty());
}

TEST_F(AIClientTest, AskLLMNoApiKeyFails) {
  prefs_.SetString(kLLMApiKey, "");

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_EQ(captured_result.error, "API key not configured");
}

TEST_F(AIClientTest, AskLLMNetworkError) {
  // Don't add any response — the loader will report a network error.
  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages", "",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  // Should fail — either no body parsed or error from invalid JSON.
  EXPECT_FALSE(captured_result.success);
}

TEST_F(AIClientTest, AskLLMInvalidJSONResponse) {
  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages",
      "this is not json at all");

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_EQ(captured_result.error, "Failed to parse API response JSON");
}

TEST_F(AIClientTest, AskLLMMultipleContentBlocks) {
  // Build a response with multiple text blocks.
  base::Value::Dict response;
  response.Set("id", "msg_test");
  base::Value::List content;
  base::Value::Dict block1;
  block1.Set("type", "text");
  block1.Set("text", "First part.");
  content.Append(std::move(block1));
  base::Value::Dict block2;
  block2.Set("type", "text");
  block2.Set("text", "Second part.");
  content.Append(std::move(block2));
  response.Set("content", std::move(content));
  base::Value::Dict usage;
  usage.Set("input_tokens", 10);
  usage.Set("output_tokens", 20);
  response.Set("usage", std::move(usage));

  std::string json;
  base::JSONWriter::Write(response, &json);

  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages", json);

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "First part.\nSecond part.");
}

TEST_F(AIClientTest, AskLLMEmptyContentArray) {
  base::Value::Dict response;
  response.Set("id", "msg_test");
  base::Value::List content;
  response.Set("content", std::move(content));

  std::string json;
  base::JSONWriter::Write(response, &json);

  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages", json);

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_TRUE(captured_result.text.empty());
}

// === OCRImage Tests ===

TEST_F(AIClientTest, OCRImageSuccessful) {
  test_url_loader_factory_.AddResponse(
      "http://localhost:8866/predict/ocr_system",
      BuildOCRSuccessResponse({"Hello", "World"}));

  base::RunLoop run_loop;
  OCRResult captured_result;

  std::vector<uint8_t> fake_png = {0x89, 0x50, 0x4E, 0x47};  // PNG magic
  client_->OCRImage(
      fake_png,
      base::BindOnce(
          [](base::RunLoop* rl, OCRResult* out, OCRResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "Hello\nWorld");
}

TEST_F(AIClientTest, OCRImageNoEndpointConfigured) {
  prefs_.SetString(kOCREndpoint, "");

  base::RunLoop run_loop;
  OCRResult captured_result;

  std::vector<uint8_t> fake_png = {0x89};
  client_->OCRImage(
      fake_png,
      base::BindOnce(
          [](base::RunLoop* rl, OCRResult* out, OCRResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_EQ(captured_result.error, "OCR endpoint not configured");
}

TEST_F(AIClientTest, OCRImageInvalidResponseJSON) {
  test_url_loader_factory_.AddResponse(
      "http://localhost:8866/predict/ocr_system", "not json");

  base::RunLoop run_loop;
  OCRResult captured_result;

  std::vector<uint8_t> fake_png = {0x89};
  client_->OCRImage(
      fake_png,
      base::BindOnce(
          [](base::RunLoop* rl, OCRResult* out, OCRResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
  EXPECT_EQ(captured_result.error, "Failed to parse OCR response");
}

TEST_F(AIClientTest, OCRImageEmptyResults) {
  // Response with empty results array.
  test_url_loader_factory_.AddResponse(
      "http://localhost:8866/predict/ocr_system",
      R"({"results":[]})");

  base::RunLoop run_loop;
  OCRResult captured_result;

  std::vector<uint8_t> fake_png = {0x89};
  client_->OCRImage(
      fake_png,
      base::BindOnce(
          [](base::RunLoop* rl, OCRResult* out, OCRResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_FALSE(captured_result.success);
}

TEST_F(AIClientTest, OCRImageAlternateFormat) {
  // PaddleOCR alternate response: {"result": [{"text": "..."}]}
  test_url_loader_factory_.AddResponse(
      "http://localhost:8866/predict/ocr_system",
      R"({"result":[{"text":"Line A"},{"text":"Line B"}]})");

  base::RunLoop run_loop;
  OCRResult captured_result;

  std::vector<uint8_t> fake_png = {0x89};
  client_->OCRImage(
      fake_png,
      base::BindOnce(
          [](base::RunLoop* rl, OCRResult* out, OCRResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "Line A\nLine B");
}

// === Request Building Verification ===

TEST_F(AIClientTest, AskLLMSendsCorrectHeaders) {
  test_url_loader_factory_.SetInterceptor(
      base::BindRepeating([](const network::ResourceRequest& request) {
        EXPECT_EQ(request.url.spec(),
                  "https://api.anthropic.com/v1/messages");
        EXPECT_EQ(request.method, "POST");

        std::string api_key;
        request.headers.GetHeader("x-api-key", &api_key);
        EXPECT_EQ(api_key, "sk-test-key");

        std::string version;
        request.headers.GetHeader("anthropic-version", &version);
        EXPECT_EQ(version, "2023-06-01");

        std::string content_type;
        request.headers.GetHeader("Content-Type", &content_type);
        EXPECT_EQ(content_type, "application/json");
      }));

  test_url_loader_factory_.AddResponse(
      "https://api.anthropic.com/v1/messages",
      BuildClaudeSuccessResponse("ok"));

  base::RunLoop run_loop;
  client_->AskLLM(
      "test", "",
      base::BindOnce([](base::RunLoop* rl, LLMResult) { rl->Quit(); },
                     &run_loop));
  run_loop.Run();
}

TEST_F(AIClientTest, AskLLMUsesCustomEndpoint) {
  prefs_.SetString(kLLMEndpoint, "https://custom-api.example.com/v1/chat");

  test_url_loader_factory_.AddResponse(
      "https://custom-api.example.com/v1/chat",
      BuildClaudeSuccessResponse("custom response"));

  base::RunLoop run_loop;
  LLMResult captured_result;

  client_->AskLLM(
      "test", "",
      base::BindOnce(
          [](base::RunLoop* rl, LLMResult* out, LLMResult result) {
            *out = std::move(result);
            rl->Quit();
          },
          &run_loop, &captured_result));

  run_loop.Run();

  EXPECT_TRUE(captured_result.success);
  EXPECT_EQ(captured_result.text, "custom response");
}

// === LLMResult/OCRResult Struct Defaults ===

TEST(LLMResultTest, DefaultValues) {
  LLMResult result;
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.text.empty());
  EXPECT_TRUE(result.error.empty());
  EXPECT_EQ(result.input_tokens, 0);
  EXPECT_EQ(result.output_tokens, 0);
}

TEST(OCRResultTest, DefaultValues) {
  OCRResult result;
  EXPECT_FALSE(result.success);
  EXPECT_TRUE(result.text.empty());
  EXPECT_TRUE(result.error.empty());
}

TEST(PlannedActionTest, DefaultValues) {
  PlannedAction action;
  EXPECT_TRUE(action.type.empty());
  EXPECT_TRUE(action.selector.empty());
  EXPECT_TRUE(action.value.empty());
}

}  // namespace ai_browser
