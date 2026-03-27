// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/stealth_injector.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace ai_browser {

const char StealthInjector::kDefaultUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/125.0.0.0 Safari/537.36";

// This script sets up realistic browser properties that are typically
// absent or different when running under automation. It runs in an
// isolated JavaScript world so page scripts cannot tamper with it.
const char StealthInjector::kAntiDetectScript[] = R"js(
(function() {
  'use strict';

  // 1. navigator.webdriver — set to false (automation flag).
  Object.defineProperty(navigator, 'webdriver', {
    get: function() { return false; },
    configurable: true
  });

  // 2. navigator.plugins — provide a realistic plugin list.
  Object.defineProperty(navigator, 'plugins', {
    get: function() {
      return [
        {
          name: 'Chrome PDF Plugin',
          description: 'Portable Document Format',
          filename: 'internal-pdf-viewer',
          length: 1
        },
        {
          name: 'Chrome PDF Viewer',
          description: '',
          filename: 'mhjfbmdgcfjbbpaeojofohoefgiehjai',
          length: 1
        },
        {
          name: 'Native Client',
          description: '',
          filename: 'internal-nacl-plugin',
          length: 2
        }
      ];
    },
    configurable: true
  });

  // 3. navigator.languages — realistic language list.
  Object.defineProperty(navigator, 'languages', {
    get: function() { return ['en-US', 'en', 'zh-CN', 'zh']; },
    configurable: true
  });

  // 4. window.chrome — ensure the chrome object exists with runtime.
  if (!window.chrome) {
    window.chrome = {};
  }
  if (!window.chrome.runtime) {
    window.chrome.runtime = {
      connect: function() { return {}; },
      sendMessage: function() {}
    };
  }

  // 5. Permissions API — return 'prompt' for notification permission query.
  var originalQuery = window.Permissions && Permissions.prototype.query;
  if (originalQuery) {
    Permissions.prototype.query = function(parameters) {
      if (parameters.name === 'notifications') {
        return Promise.resolve({state: 'prompt', onchange: null});
      }
      return originalQuery.call(this, parameters);
    };
  }

  // 6. Remove automation-related properties.
  var automationProps = [
    'cdc_adoQpoasnfa76pfcZLmcfl_Array',
    'cdc_adoQpoasnfa76pfcZLmcfl_Promise',
    'cdc_adoQpoasnfa76pfcZLmcfl_Symbol',
    '__webdriver_evaluate',
    '__selenium_evaluate',
    '__fxdriver_evaluate',
    '__driver_evaluate',
    '__webdriver_unwrap',
    '__selenium_unwrap',
    '__fxdriver_unwrap',
    '__driver_unwrap',
    '_Selenium_IDE_Recorder',
    '_selenium',
    'calledSelenium',
    '__nightmare',
    '__phantomas',
    'domAutomation',
    'domAutomationController'
  ];
  for (var i = 0; i < automationProps.length; i++) {
    try { delete window[automationProps[i]]; } catch(e) {}
    try { delete document[automationProps[i]]; } catch(e) {}
  }

  // 7. WebGL renderer/vendor — override with common values.
  var getParameterProto = WebGLRenderingContext.prototype.getParameter;
  WebGLRenderingContext.prototype.getParameter = function(param) {
    // UNMASKED_VENDOR_WEBGL
    if (param === 0x9245) {
      return 'Intel Inc.';
    }
    // UNMASKED_RENDERER_WEBGL
    if (param === 0x9246) {
      return 'Intel Iris OpenGL Engine';
    }
    return getParameterProto.call(this, param);
  };

  // Also handle WebGL2.
  if (typeof WebGL2RenderingContext !== 'undefined') {
    var getParam2Proto = WebGL2RenderingContext.prototype.getParameter;
    WebGL2RenderingContext.prototype.getParameter = function(param) {
      if (param === 0x9245) return 'Intel Inc.';
      if (param === 0x9246) return 'Intel Iris OpenGL Engine';
      return getParam2Proto.call(this, param);
    };
  }

  // 8. Connection/hardware concurrency — realistic values.
  Object.defineProperty(navigator, 'hardwareConcurrency', {
    get: function() { return 8; },
    configurable: true
  });

  Object.defineProperty(navigator, 'deviceMemory', {
    get: function() { return 8; },
    configurable: true
  });
})();
)js";

StealthInjector::StealthInjector() = default;
StealthInjector::~StealthInjector() = default;

void StealthInjector::InjectStealth(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  // Set a realistic User-Agent.
  SetUserAgent(web_contents, kDefaultUserAgent);

  // Inject into the main frame.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  if (main_frame) {
    InjectIntoFrame(main_frame);
  }

  // Inject into all child frames.
  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* frame) {
        InjectIntoFrame(frame);
      });
}

void StealthInjector::SetUserAgent(content::WebContents* web_contents,
                                   const std::string& user_agent) {
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = user_agent;
  web_contents->SetUserAgentOverride(ua_override, false);
}

void StealthInjector::InjectIntoFrame(content::RenderFrameHost* frame) {
  if (!frame || !frame->IsRenderFrameLive()) {
    return;
  }

  // Use isolated world (id=999) so page scripts cannot interfere.
  const int kStealthWorldId = 999;
  frame->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(kAntiDetectScript),
      base::DoNothing(),
      kStealthWorldId);
}

}  // namespace ai_browser
