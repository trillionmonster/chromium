// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/page_reader.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

// === ScoreReadability Tests ===
// ScoreReadability is a static method, so we can test it without
// constructing a full PageReader.

TEST(PageReaderScoreReadabilityTest, EmptyStringReturnsZero) {
  EXPECT_DOUBLE_EQ(PageReader::ScoreReadability(""), 0.0);
}

TEST(PageReaderScoreReadabilityTest, VeryShortTextReturnsLowScore) {
  // Less than 50 chars should return 0.1.
  EXPECT_DOUBLE_EQ(PageReader::ScoreReadability("Hi"), 0.1);
  EXPECT_DOUBLE_EQ(PageReader::ScoreReadability("Short text"), 0.1);
  EXPECT_DOUBLE_EQ(PageReader::ScoreReadability(std::string(49, 'a')), 0.1);
}

TEST(PageReaderScoreReadabilityTest, JustAbove50CharsThreshold) {
  // 50 chars should NOT return 0.1 anymore.
  std::string text(50, 'a');
  double score = PageReader::ScoreReadability(text);
  EXPECT_GT(score, 0.1);
}

TEST(PageReaderScoreReadabilityTest, GoodEnglishTextGetsHighScore) {
  // A well-formed English paragraph.
  std::string text =
      "Chromium is an open-source browser project that aims to build a safer, "
      "faster, and more stable way for all Internet users to experience the "
      "web. This page contains design documents, architecture overviews, "
      "testing information, and more to help you learn to build and work with "
      "the Chromium source code. The project has thousands of contributors "
      "from around the world working on the codebase every day. The browser "
      "engine is used by Google Chrome, Microsoft Edge, Opera, Brave, and "
      "many other browsers.";
  double score = PageReader::ScoreReadability(text);
  EXPECT_GE(score, 0.6);
}

TEST(PageReaderScoreReadabilityTest, GarbageTextGetsLowScore) {
  // Mostly non-meaningful characters.
  std::string garbage(500, '\n');
  double score = PageReader::ScoreReadability(garbage);
  EXPECT_LT(score, 0.3);
}

TEST(PageReaderScoreReadabilityTest, SpecialCharsOnlyGetLowScore) {
  std::string special(500, '!');
  double score = PageReader::ScoreReadability(special);
  EXPECT_LT(score, 0.3);
}

TEST(PageReaderScoreReadabilityTest, LongerTextScoresHigher) {
  // The length_score component (capped at 1.0 at 1000 chars) should
  // make longer text score higher than shorter equivalent-density text.
  std::string short_text = "Hello world this is a test string with words.";
  // Pad to >50 chars.
  while (short_text.size() < 100) short_text += " More text here.";

  std::string long_text;
  for (int i = 0; i < 20; i++) {
    long_text += "This is a longer text with many words and sentences. ";
  }

  double short_score = PageReader::ScoreReadability(short_text);
  double long_score = PageReader::ScoreReadability(long_text);
  EXPECT_GT(long_score, short_score);
}

TEST(PageReaderScoreReadabilityTest, CJKTextGetsBonusScore) {
  // Chinese text (multi-byte UTF-8, bytes >= 0xE0).
  // "这是一段中文测试文本，用于测试可读性评分函数。这段文字需要超过五十个字符才能通过最低阈值检测。"
  std::string cjk_text =
      "\xe8\xbf\x99\xe6\x98\xaf\xe4\xb8\x80\xe6\xae\xb5\xe4\xb8\xad\xe6"
      "\x96\x87\xe6\xb5\x8b\xe8\xaf\x95\xe6\x96\x87\xe6\x9c\xac\xef\xbc"
      "\x8c\xe7\x94\xa8\xe4\xba\x8e\xe6\xb5\x8b\xe8\xaf\x95\xe5\x8f\xaf"
      "\xe8\xaf\xbb\xe6\x80\xa7\xe8\xaf\x84\xe5\x88\x86\xe5\x87\xbd\xe6"
      "\x95\xb0\xe3\x80\x82\xe8\xbf\x99\xe6\xae\xb5\xe6\x96\x87\xe5\xad"
      "\x97\xe9\x9c\x80\xe8\xa6\x81\xe8\xb6\x85\xe8\xbf\x87\xe4\xba\x94"
      "\xe5\x8d\x81\xe4\xb8\xaa\xe5\xad\x97\xe7\xac\xa6\xe6\x89\x8d\xe8"
      "\x83\xbd\xe9\x80\x9a\xe8\xbf\x87\xe6\x9c\x80\xe4\xbd\x8e\xe9\x98"
      "\x88\xe5\x80\xbc\xe6\xa3\x80\xe6\xb5\x8b\xe3\x80\x82";

  double score = PageReader::ScoreReadability(cjk_text);
  // CJK text should still get a reasonable score.
  EXPECT_GT(score, 0.2);
}

TEST(PageReaderScoreReadabilityTest, MixedContentText) {
  // Mix of meaningful text and some noise.
  std::string text =
      "This is meaningful content.\n\n\n\n\n"
      "More content here with some useful information.\n"
      "                                              \n"
      "Final paragraph with good content for scoring.";
  double score = PageReader::ScoreReadability(text);
  EXPECT_GT(score, 0.2);
  EXPECT_LT(score, 0.8);
}

TEST(PageReaderScoreReadabilityTest, ScoreIsClamped01) {
  // Verify the result is always between 0.0 and 1.0.

  // Normal text.
  std::string normal(2000, 'a');
  double score = PageReader::ScoreReadability(normal);
  EXPECT_GE(score, 0.0);
  EXPECT_LE(score, 1.0);

  // Whitespace only.
  std::string whitespace(2000, ' ');
  score = PageReader::ScoreReadability(whitespace);
  EXPECT_GE(score, 0.0);
  EXPECT_LE(score, 1.0);

  // All digits.
  std::string digits(2000, '5');
  score = PageReader::ScoreReadability(digits);
  EXPECT_GE(score, 0.0);
  EXPECT_LE(score, 1.0);
}

TEST(PageReaderScoreReadabilityTest, OnlyWhitespaceGetsVeryLowScore) {
  std::string text(200, ' ');
  text += std::string(200, '\t');
  text += std::string(200, '\n');
  double score = PageReader::ScoreReadability(text);
  // All whitespace, no meaningful chars.
  EXPECT_LT(score, 0.15);
}

TEST(PageReaderScoreReadabilityTest, PureAlphaNumericGetsHighDensity) {
  // 100% meaningful characters.
  std::string text(1000, 'x');
  double score = PageReader::ScoreReadability(text);
  // char_density=1.0*0.4=0.4, length_score=1.0*0.3=0.3, word_score
  // is 1 word so low; but density+length alone pushes it >0.7.
  EXPECT_GT(score, 0.6);
}

TEST(PageReaderScoreReadabilityTest, WordCountAffectsScore) {
  // Many short words → higher word_score component.
  std::string many_words;
  for (int i = 0; i < 100; i++) {
    many_words += "word ";
  }
  // Single continuous string → 1 word boundary.
  std::string one_block(500, 'a');

  double many_score = PageReader::ScoreReadability(many_words);
  double one_score = PageReader::ScoreReadability(one_block);

  // Many words should score higher due to word_score component.
  EXPECT_GT(many_score, one_score);
}

// === PageContent Struct Tests ===

TEST(PageContentTest, DefaultValues) {
  PageContent content;
  EXPECT_TRUE(content.text.empty());
  EXPECT_TRUE(content.links.empty());
  EXPECT_TRUE(content.title.empty());
  EXPECT_TRUE(content.extraction_method.empty());
  EXPECT_DOUBLE_EQ(content.readability_score, 0.0);
}

}  // namespace ai_browser
