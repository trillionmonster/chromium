// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// anti_detect.js — Injected into pages to set realistic browser properties.
// Prevents overzealous bot detection from blocking automated browsing.

(function() {
  'use strict';

  // 1. navigator.webdriver — the primary automation detection flag.
  Object.defineProperty(navigator, 'webdriver', {
    get: function() { return false; },
    configurable: true
  });

  // 2. navigator.plugins — realistic plugin list.
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

  // 3. navigator.languages
  Object.defineProperty(navigator, 'languages', {
    get: function() { return ['en-US', 'en', 'zh-CN', 'zh']; },
    configurable: true
  });

  // 4. window.chrome runtime object.
  if (!window.chrome) {
    window.chrome = {};
  }
  if (!window.chrome.runtime) {
    window.chrome.runtime = {
      connect: function() { return {}; },
      sendMessage: function() {}
    };
  }

  // 5. Permissions API — return 'prompt' for notifications.
  var origQuery = window.Permissions && Permissions.prototype.query;
  if (origQuery) {
    Permissions.prototype.query = function(params) {
      if (params.name === 'notifications') {
        return Promise.resolve({state: 'prompt', onchange: null});
      }
      return origQuery.call(this, params);
    };
  }

  // 6. Remove automation-related window properties.
  var propsToRemove = [
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
  for (var i = 0; i < propsToRemove.length; i++) {
    try { delete window[propsToRemove[i]]; } catch (e) {}
    try { delete document[propsToRemove[i]]; } catch (e) {}
  }

  // 7. WebGL vendor/renderer — common Intel values.
  var origGetParam = WebGLRenderingContext.prototype.getParameter;
  WebGLRenderingContext.prototype.getParameter = function(param) {
    if (param === 0x9245) return 'Intel Inc.';       // UNMASKED_VENDOR
    if (param === 0x9246) return 'Intel Iris OpenGL Engine'; // UNMASKED_RENDERER
    return origGetParam.call(this, param);
  };

  if (typeof WebGL2RenderingContext !== 'undefined') {
    var origGetParam2 = WebGL2RenderingContext.prototype.getParameter;
    WebGL2RenderingContext.prototype.getParameter = function(param) {
      if (param === 0x9245) return 'Intel Inc.';
      if (param === 0x9246) return 'Intel Iris OpenGL Engine';
      return origGetParam2.call(this, param);
    };
  }

  // 8. Hardware concurrency and device memory.
  Object.defineProperty(navigator, 'hardwareConcurrency', {
    get: function() { return 8; },
    configurable: true
  });
  Object.defineProperty(navigator, 'deviceMemory', {
    get: function() { return 8; },
    configurable: true
  });

  // 9. Screen dimensions — ensure they look realistic.
  if (screen.width === 0 || screen.height === 0) {
    Object.defineProperty(screen, 'width', { get: function() { return 1920; } });
    Object.defineProperty(screen, 'height', { get: function() { return 1080; } });
    Object.defineProperty(screen, 'availWidth', { get: function() { return 1920; } });
    Object.defineProperty(screen, 'availHeight', { get: function() { return 1040; } });
    Object.defineProperty(screen, 'colorDepth', { get: function() { return 24; } });
    Object.defineProperty(screen, 'pixelDepth', { get: function() { return 24; } });
  }
})();
