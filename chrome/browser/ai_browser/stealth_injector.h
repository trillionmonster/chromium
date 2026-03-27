// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_STEALTH_INJECTOR_H_
#define CHROME_BROWSER_AI_BROWSER_STEALTH_INJECTOR_H_

#include <string>

#include "base/memory/weak_ptr.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ai_browser {

// Injects anti-fingerprinting and stealth scripts into web pages before
// page JavaScript executes. This makes automated browsing appear as normal
// human browsing by setting realistic browser properties.
//
// This is the same approach used by legitimate automation tools (e.g.,
// puppeteer-extra-plugin-stealth) to prevent overzealous bot-detection
// from blocking users who simply happen to use browser automation.
class StealthInjector {
 public:
  StealthInjector();
  ~StealthInjector();

  StealthInjector(const StealthInjector&) = delete;
  StealthInjector& operator=(const StealthInjector&) = delete;

  // Inject stealth scripts into all frames of |web_contents|.
  void InjectStealth(content::WebContents* web_contents);

  // Set a custom User-Agent string.
  void SetUserAgent(content::WebContents* web_contents,
                    const std::string& user_agent);

 private:
  // Inject into a single frame using an isolated world.
  void InjectIntoFrame(content::RenderFrameHost* frame);

  // The stealth JavaScript payload.
  static const char kAntiDetectScript[];

  // A realistic desktop User-Agent.
  static const char kDefaultUserAgent[];

  base::WeakPtrFactory<StealthInjector> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_STEALTH_INJECTOR_H_
