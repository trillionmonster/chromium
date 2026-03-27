// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/page_reader.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ai_browser/ai_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ai_browser {

namespace {

// JavaScript that extracts clean text content from the DOM.
// It clones the body, removes noise elements, then extracts innerText
// and link information.
constexpr char kExtractDOMScript[] = R"js(
(function() {
  try {
    // Clone body to avoid mutating the live DOM.
    var clone = document.body.cloneNode(true);

    // Remove noise elements.
    var noiseSelectors = [
      'script', 'style', 'noscript', 'iframe', 'nav', 'footer',
      'header', 'aside', '[role="banner"]', '[role="navigation"]',
      '[role="complementary"]', '.ad', '.ads', '.advertisement',
      '[class*="cookie"]', '[class*="popup"]', '[class*="modal"]',
      '[class*="sidebar"]', '[class*="menu"]'
    ];
    for (var i = 0; i < noiseSelectors.length; i++) {
      var els = clone.querySelectorAll(noiseSelectors[i]);
      for (var j = 0; j < els.length; j++) {
        els[j].remove();
      }
    }

    // Extract text.
    var text = clone.innerText || '';
    // Collapse multiple newlines.
    text = text.replace(/\n{3,}/g, '\n\n').trim();

    // Extract links.
    var links = [];
    var anchors = document.querySelectorAll('a[href]');
    for (var k = 0; k < anchors.length && k < 200; k++) {
      var a = anchors[k];
      var href = a.href;
      var linkText = (a.innerText || '').trim();
      if (href && linkText && !href.startsWith('javascript:')) {
        links.push({url: href, text: linkText.substring(0, 200)});
      }
    }

    return JSON.stringify({
      text: text.substring(0, 500000),
      links: links,
      title: document.title || '',
      url: window.location.href
    });
  } catch(e) {
    return JSON.stringify({text: '', links: [], title: '', url: '', error: e.message});
  }
})();
)js";

}  // namespace

PageReader::PageReader(AIClient* ai_client) : ai_client_(ai_client) {}

PageReader::~PageReader() = default;

void PageReader::ReadPage(content::WebContents* web_contents,
                          ReadCallback callback) {
  // Start with Tier 1: DOM extraction.
  ExtractViaDOM(web_contents, std::move(callback));
}

void PageReader::ExtractViaDOM(content::WebContents* web_contents,
                               ReadCallback callback) {
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    PageContent empty;
    empty.extraction_method = "failed";
    std::move(callback).Run(std::move(empty));
    return;
  }

  frame->ExecuteJavaScript(
      base::UTF8ToUTF16(kExtractDOMScript),
      base::BindOnce(
          [](base::WeakPtr<PageReader> self,
             content::WebContents* wc,
             ReadCallback cb, base::Value result) {
            if (!self) return;

            PageContent content;
            content.extraction_method = "dom";

            if (result.is_string()) {
              auto parsed = base::JSONReader::Read(result.GetString());
              if (parsed && parsed->is_dict()) {
                const auto& dict = parsed->GetDict();
                if (const auto* text = dict.FindString("text")) {
                  content.text = *text;
                }
                if (const auto* title = dict.FindString("title")) {
                  content.title = *title;
                }
                if (const auto* links_list = dict.FindList("links")) {
                  for (const auto& link : *links_list) {
                    if (link.is_dict()) {
                      if (const auto* url = link.GetDict().FindString("url")) {
                        content.links.push_back(*url);
                      }
                    }
                  }
                }
              }
            }

            content.readability_score = ScoreReadability(content.text);

            // If readability is good enough, return DOM result.
            if (content.readability_score >= self->readability_threshold_) {
              std::move(cb).Run(std::move(content));
              return;
            }

            LOG(INFO) << "DOM readability score "
                      << content.readability_score
                      << " below threshold, trying OCR...";

            // Tier 2: Try OCR.
            self->ExtractViaOCR(wc, std::move(content), std::move(cb));
          },
          weak_factory_.GetWeakPtr(), web_contents, std::move(callback)));
}

void PageReader::ExtractViaOCR(content::WebContents* web_contents,
                               PageContent partial_content,
                               ReadCallback callback) {
  CaptureScreenshot(
      web_contents,
      base::BindOnce(
          [](base::WeakPtr<PageReader> self,
             content::WebContents* wc,
             PageContent partial,
             ReadCallback cb,
             std::vector<uint8_t> png_data) {
            if (!self || png_data.empty()) {
              // OCR capture failed, try VLM directly.
              self->ExtractViaVLM(wc, std::move(partial), std::move(cb));
              return;
            }

            self->ai_client_->OCRImage(
                png_data,
                base::BindOnce(
                    [](base::WeakPtr<PageReader> self2,
                       content::WebContents* wc2,
                       PageContent partial2,
                       ReadCallback cb2,
                       OCRResult ocr_result) {
                      if (!self2) return;

                      if (ocr_result.success && !ocr_result.text.empty()) {
                        // Merge OCR text with DOM text.
                        if (!partial2.text.empty()) {
                          partial2.text += "\n\n--- OCR Text ---\n\n";
                        }
                        partial2.text += ocr_result.text;
                        partial2.extraction_method = "dom+ocr";
                        partial2.readability_score =
                            ScoreReadability(partial2.text);

                        if (partial2.readability_score >=
                            self2->readability_threshold_) {
                          std::move(cb2).Run(std::move(partial2));
                          return;
                        }
                      }

                      LOG(INFO) << "OCR readability "
                                << partial2.readability_score
                                << " still low, trying VLM...";

                      // Tier 3: Try VLM.
                      self2->ExtractViaVLM(wc2, std::move(partial2),
                                           std::move(cb2));
                    },
                    self, wc, std::move(partial), std::move(cb)));
          },
          weak_factory_.GetWeakPtr(), web_contents, std::move(partial_content),
          std::move(callback)));
}

void PageReader::ExtractViaVLM(content::WebContents* web_contents,
                               PageContent partial_content,
                               ReadCallback callback) {
  CaptureScreenshot(
      web_contents,
      base::BindOnce(
          [](base::WeakPtr<PageReader> self,
             PageContent partial,
             ReadCallback cb,
             std::vector<uint8_t> png_data) {
            if (!self || png_data.empty()) {
              partial.extraction_method = "failed";
              std::move(cb).Run(std::move(partial));
              return;
            }

            self->ai_client_->AnalyzeScreenshot(
                png_data,
                "Extract ALL text content visible in this screenshot. "
                "Include headings, body text, captions, and any other "
                "readable text. Preserve the structure and order.",
                base::BindOnce(
                    [](PageContent partial2, ReadCallback cb2,
                       LLMResult vlm_result) {
                      if (vlm_result.success) {
                        if (!partial2.text.empty()) {
                          partial2.text += "\n\n--- VLM Analysis ---\n\n";
                        }
                        partial2.text += vlm_result.text;
                        partial2.extraction_method = "dom+vlm";
                      }
                      partial2.readability_score =
                          ScoreReadability(partial2.text);
                      std::move(cb2).Run(std::move(partial2));
                    },
                    std::move(partial), std::move(cb)));
          },
          weak_factory_.GetWeakPtr(), std::move(partial_content),
          std::move(callback)));
}

void PageReader::CaptureScreenshot(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::vector<uint8_t>)> callback) {
  content::RenderWidgetHostView* view =
      web_contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run({});
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(),  // Capture full viewport.
      gfx::Size(),  // Use default size.
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<uint8_t>)> cb,
             const SkBitmap& bitmap) {
            if (bitmap.drawsNothing()) {
              std::move(cb).Run({});
              return;
            }
            std::vector<uint8_t> png_data;
            if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false,
                                                     &png_data)) {
              std::move(cb).Run({});
              return;
            }
            std::move(cb).Run(std::move(png_data));
          },
          std::move(callback)));
}

// static
double PageReader::ScoreReadability(const std::string& text) {
  if (text.empty()) return 0.0;

  size_t total_chars = text.size();
  if (total_chars < 50) return 0.1;

  // Count meaningful characters (letters, digits, CJK characters).
  size_t meaningful_chars = 0;
  size_t cjk_chars = 0;
  size_t word_boundary_count = 0;
  bool prev_was_space = true;

  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (std::isalnum(c)) {
      ++meaningful_chars;
      if (prev_was_space) ++word_boundary_count;
      prev_was_space = false;
    } else if (c >= 0xE0) {
      // Rough check for CJK multi-byte chars (UTF-8: 3+ bytes starting >= 0xE0)
      ++cjk_chars;
      ++meaningful_chars;
      prev_was_space = false;
    } else if (c == ' ' || c == '\n' || c == '\t') {
      prev_was_space = true;
    }
  }

  // Calculate sub-scores.
  double char_density =
      static_cast<double>(meaningful_chars) / static_cast<double>(total_chars);
  double length_score = std::min(1.0, static_cast<double>(total_chars) / 1000.0);
  double word_score =
      std::min(1.0, static_cast<double>(word_boundary_count) / 50.0);
  double cjk_bonus =
      (cjk_chars > 20) ? 0.1 : 0.0;  // CJK content gets a small bonus.

  // Weighted combination.
  double score = (char_density * 0.4) + (length_score * 0.3) +
                 (word_score * 0.2) + cjk_bonus;

  return std::clamp(score, 0.0, 1.0);
}

}  // namespace ai_browser
