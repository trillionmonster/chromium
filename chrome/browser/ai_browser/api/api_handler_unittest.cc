// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/api/api_handler.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// === ParseRequestBody Tests ===

TEST(ApiHandlerTest, ParseValidJSON) {
  auto result = ApiHandler::ParseRequestBody(
      R"({"url":"https://example.com","question":"What is this?"})");
  EXPECT_FALSE(result.empty());

  const std::string* url = result.FindString("url");
  ASSERT_NE(url, nullptr);
  EXPECT_EQ(*url, "https://example.com");

  const std::string* question = result.FindString("question");
  ASSERT_NE(question, nullptr);
  EXPECT_EQ(*question, "What is this?");
}

TEST(ApiHandlerTest, ParseEmptyStringReturnsEmptyDict) {
  auto result = ApiHandler::ParseRequestBody("");
  EXPECT_TRUE(result.empty());
}

TEST(ApiHandlerTest, ParseMalformedJSONReturnsEmptyDict) {
  auto result = ApiHandler::ParseRequestBody("{bad json");
  EXPECT_TRUE(result.empty());
}

TEST(ApiHandlerTest, ParseJSONArrayReturnsEmptyDict) {
  auto result = ApiHandler::ParseRequestBody("[1, 2, 3]");
  EXPECT_TRUE(result.empty());
}

TEST(ApiHandlerTest, ParseJSONPrimitiveReturnsEmptyDict) {
  auto result = ApiHandler::ParseRequestBody("42");
  EXPECT_TRUE(result.empty());
}

TEST(ApiHandlerTest, ParseJSONStringReturnsEmptyDict) {
  auto result = ApiHandler::ParseRequestBody(R"("just a string")");
  EXPECT_TRUE(result.empty());
}

TEST(ApiHandlerTest, ParseNestedJSON) {
  auto result = ApiHandler::ParseRequestBody(
      R"({"outer":{"inner":"value"},"count":5})");
  EXPECT_FALSE(result.empty());

  const base::Value::Dict* outer = result.FindDict("outer");
  ASSERT_NE(outer, nullptr);
  const std::string* inner = outer->FindString("inner");
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(*inner, "value");

  EXPECT_EQ(result.FindInt("count").value_or(0), 5);
}

TEST(ApiHandlerTest, ParseEmptyObject) {
  auto result = ApiHandler::ParseRequestBody("{}");
  // An empty object is a valid dict — it should not be treated as failure.
  EXPECT_TRUE(result.empty());  // empty() == no keys, but it IS a valid dict
}

TEST(ApiHandlerTest, ParseJSONWithSpecialChars) {
  auto result = ApiHandler::ParseRequestBody(
      R"({"text":"Hello \"world\"\nNew line"})");
  EXPECT_FALSE(result.empty());
  const std::string* text = result.FindString("text");
  ASSERT_NE(text, nullptr);
  EXPECT_EQ(*text, "Hello \"world\"\nNew line");
}

TEST(ApiHandlerTest, ParseJSONWithUnicode) {
  auto result = ApiHandler::ParseRequestBody(
      R"({"query":"\u4f60\u597d"})");
  EXPECT_FALSE(result.empty());
  const std::string* query = result.FindString("query");
  ASSERT_NE(query, nullptr);
  // \u4f60\u597d = "你好"
  EXPECT_FALSE(query->empty());
}

// === BuildSuccessResponse Tests ===

TEST(ApiHandlerTest, BuildSuccessResponseContainsData) {
  base::Value::Dict data;
  data.Set("answer", "42");
  data.Set("method", "dom");

  auto response = ApiHandler::BuildSuccessResponse(data);

  EXPECT_TRUE(response.FindBool("success").value_or(false));

  const base::Value::Dict* resp_data = response.FindDict("data");
  ASSERT_NE(resp_data, nullptr);

  const std::string* answer = resp_data->FindString("answer");
  ASSERT_NE(answer, nullptr);
  EXPECT_EQ(*answer, "42");

  const std::string* method = resp_data->FindString("method");
  ASSERT_NE(method, nullptr);
  EXPECT_EQ(*method, "dom");
}

TEST(ApiHandlerTest, BuildSuccessResponseWithEmptyData) {
  base::Value::Dict empty_data;
  auto response = ApiHandler::BuildSuccessResponse(empty_data);

  EXPECT_TRUE(response.FindBool("success").value_or(false));
  const base::Value::Dict* resp_data = response.FindDict("data");
  ASSERT_NE(resp_data, nullptr);
  EXPECT_TRUE(resp_data->empty());
}

TEST(ApiHandlerTest, BuildSuccessResponseClonesData) {
  base::Value::Dict data;
  data.Set("key", "value");

  auto response = ApiHandler::BuildSuccessResponse(data);

  // Modify original data — response should not be affected.
  data.Set("key", "modified");
  const base::Value::Dict* resp_data = response.FindDict("data");
  ASSERT_NE(resp_data, nullptr);
  EXPECT_EQ(*resp_data->FindString("key"), "value");
}

// === BuildErrorResponse Tests ===

TEST(ApiHandlerTest, BuildErrorResponseContainsCodeAndMessage) {
  auto response = ApiHandler::BuildErrorResponse(400, "Bad request");

  EXPECT_FALSE(response.FindBool("success").value_or(true));
  EXPECT_EQ(response.FindInt("error_code").value_or(0), 400);

  const std::string* error = response.FindString("error");
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(*error, "Bad request");
}

TEST(ApiHandlerTest, BuildErrorResponseVariousCodes) {
  struct TestCase {
    int code;
    std::string message;
  };
  TestCase cases[] = {
      {400, "Bad request"},
      {401, "Unauthorized"},
      {403, "Forbidden"},
      {404, "Not found"},
      {500, "Internal server error"},
  };

  for (const auto& tc : cases) {
    auto response = ApiHandler::BuildErrorResponse(tc.code, tc.message);
    EXPECT_FALSE(response.FindBool("success").value_or(true));
    EXPECT_EQ(response.FindInt("error_code").value_or(0), tc.code);
    EXPECT_EQ(*response.FindString("error"), tc.message);
  }
}

TEST(ApiHandlerTest, BuildErrorResponseEmptyMessage) {
  auto response = ApiHandler::BuildErrorResponse(500, "");
  EXPECT_FALSE(response.FindBool("success").value_or(true));
  EXPECT_EQ(response.FindInt("error_code").value_or(0), 500);
  EXPECT_EQ(*response.FindString("error"), "");
}

}  // namespace ai_browser
