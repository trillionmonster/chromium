// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_PAGE_READER_H_
#define CHROME_BROWSER_AI_BROWSER_PAGE_READER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ai_browser {

class AIClient;

// Content extracted from a web page.
struct PageContent {
  std::string text;
  std::vector<std::string> links;
  std::string title;
  std::string extraction_method;  // "dom", "ocr", "vlm"
  double readability_score = 0.0;
};

using ReadCallback = base::OnceCallback<void(PageContent)>;

// Three-tier page content extraction:
//   Tier 1: DOM text extraction via JavaScript injection
//   Tier 2: Screenshot + PaddleOCR
//   Tier 3: Screenshot + Claude Vision (VLM)
// Each tier is tried in order; if the readability score is below
// the threshold, the next tier is attempted.
class PageReader {
 public:
  explicit PageReader(AIClient* ai_client);
  ~PageReader();

  PageReader(const PageReader&) = delete;
  PageReader& operator=(const PageReader&) = delete;

  // Read page content with automatic tier fallback.
  void ReadPage(content::WebContents* web_contents, ReadCallback callback);

  // Set the minimum readability score threshold (0.0 to 1.0).
  void set_readability_threshold(double threshold) {
    readability_threshold_ = threshold;
  }

 private:
  // Tier 1: Extract text via DOM JavaScript.
  void ExtractViaDOM(content::WebContents* web_contents,
                     ReadCallback callback);

  // Tier 2: Screenshot + OCR.
  void ExtractViaOCR(content::WebContents* web_contents,
                     PageContent partial_content,
                     ReadCallback callback);

  // Tier 3: Screenshot + Claude Vision.
  void ExtractViaVLM(content::WebContents* web_contents,
                     PageContent partial_content,
                     ReadCallback callback);

  // Take a full-page screenshot as PNG bytes.
  void CaptureScreenshot(
      content::WebContents* web_contents,
      base::OnceCallback<void(std::vector<uint8_t>)> callback);

  // Heuristic readability score based on text quality.
  static double ScoreReadability(const std::string& text);

  raw_ptr<AIClient> ai_client_;
  double readability_threshold_ = 0.6;

  base::WeakPtrFactory<PageReader> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_PAGE_READER_H_
