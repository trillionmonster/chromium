// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview JavaScript for chrome://ai-settings page.
 * Communicates with AISettingsHandler via chrome.send / WebUI listeners.
 */

'use strict';

/** @type {Object<string, *>} Current configuration values. */
let currentConfig = {};

/** @type {Array<Object>} Current credentials list. */
let currentCredentials = [];

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

document.addEventListener('DOMContentLoaded', () => {
  initTabs();
  initTogglePasswords();
  initEventListeners();
  initWebUIListeners();

  // Request current config and credentials from C++ backend.
  chrome.send('getConfig');
  chrome.send('getCredentials');
});

// ---------------------------------------------------------------------------
// Tab Navigation
// ---------------------------------------------------------------------------

function initTabs() {
  const tabs = document.querySelectorAll('.tab');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      // Deactivate all tabs and content.
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));

      // Activate clicked tab and corresponding content.
      tab.classList.add('active');
      const targetId = 'tab-' + tab.dataset.tab;
      const content = document.getElementById(targetId);
      if (content) {
        content.classList.add('active');
      }
    });
  });
}

// ---------------------------------------------------------------------------
// Password Toggle
// ---------------------------------------------------------------------------

function initTogglePasswords() {
  document.querySelectorAll('.toggle-password').forEach(btn => {
    btn.addEventListener('click', () => {
      const targetId = btn.dataset.target;
      const input = document.getElementById(targetId);
      if (!input) return;
      if (input.type === 'password') {
        input.type = 'text';
      } else {
        input.type = 'password';
      }
    });
  });
}

// ---------------------------------------------------------------------------
// Event Listeners
// ---------------------------------------------------------------------------

function initEventListeners() {
  // Save all settings.
  document.getElementById('save-config').addEventListener('click', saveConfig);

  // Test LLM connection.
  document.getElementById('test-llm').addEventListener('click', testLLMConnection);

  // Test OCR connection.
  document.getElementById('test-ocr').addEventListener('click', testOCRConnection);

  // Save credential.
  document.getElementById('save-credential').addEventListener('click', saveCredential);

  // Close status bar.
  document.getElementById('status-close').addEventListener('click', () => {
    document.getElementById('status-bar').classList.add('hidden');
  });
}

// ---------------------------------------------------------------------------
// WebUI Listeners (C++ -> JS callbacks)
// ---------------------------------------------------------------------------

function initWebUIListeners() {
  // Config loaded from prefs.
  cr.addWebUIListener('config-loaded', (config) => {
    currentConfig = config;
    populateForm(config);
  });

  // Config saved confirmation.
  cr.addWebUIListener('config-saved', (success) => {
    if (success) {
      showStatus('Settings saved successfully!', 'success');
      showSaveStatus();
    } else {
      showStatus('Failed to save settings.', 'error');
    }
  });

  // LLM test result.
  cr.addWebUIListener('llm-test-result', (result) => {
    const btn = document.getElementById('test-llm');
    const resultEl = document.getElementById('llm-test-result');
    setButtonLoading(btn, false);
    showTestResult(resultEl, result.success, result.message);
  });

  // OCR test result.
  cr.addWebUIListener('ocr-test-result', (result) => {
    const btn = document.getElementById('test-ocr');
    const resultEl = document.getElementById('ocr-test-result');
    setButtonLoading(btn, false);
    showTestResult(resultEl, result.success, result.message);
  });

  // Credentials loaded.
  cr.addWebUIListener('credentials-loaded', (credentials) => {
    currentCredentials = credentials;
    renderCredentialsList(credentials);
  });

  // Credential saved.
  cr.addWebUIListener('credential-saved', () => {
    showStatus('Credential saved successfully!', 'success');
    clearCredentialForm();
    // Reload credentials list.
    chrome.send('getCredentials');
  });

  // Credential deleted.
  cr.addWebUIListener('credential-deleted', () => {
    showStatus('Credential deleted.', 'success');
    chrome.send('getCredentials');
  });
}

// ---------------------------------------------------------------------------
// Form Population
// ---------------------------------------------------------------------------

function populateForm(config) {
  setInputValue('llm_api_key', config.llm_api_key || '');
  setSelectValue('llm_model', config.llm_model || 'claude-sonnet-4-20250514');
  setInputValue('llm_endpoint', config.llm_endpoint || '');
  setInputValue('llm_max_tokens', config.llm_max_tokens || 4096);
  setInputValue('ocr_endpoint', config.ocr_endpoint || '');
  setSelectValue('ocr_lang', config.ocr_lang || 'ch');
  setInputValue('max_concurrent_tabs', config.max_concurrent_tabs || 5);
  setInputValue('page_load_timeout', config.page_load_timeout || 30);
  setCheckbox('stealth_mode', config.stealth_mode !== false);
  setCheckbox('auto_scroll', config.auto_scroll !== false);
  setCheckbox('api_server_enabled', config.api_server_enabled !== false);
  setInputValue('api_server_port', config.api_server_port || 9333);
}

function setInputValue(id, value) {
  const el = document.getElementById(id);
  if (el) el.value = value;
}

function setSelectValue(id, value) {
  const el = document.getElementById(id);
  if (!el) return;
  // Set value if option exists, otherwise keep default.
  for (const option of el.options) {
    if (option.value === value) {
      el.value = value;
      return;
    }
  }
}

function setCheckbox(id, checked) {
  const el = document.getElementById(id);
  if (el) el.checked = checked;
}

// ---------------------------------------------------------------------------
// Collect Form Data
// ---------------------------------------------------------------------------

function collectConfig() {
  return {
    llm_api_key: document.getElementById('llm_api_key').value.trim(),
    llm_model: document.getElementById('llm_model').value,
    llm_endpoint: document.getElementById('llm_endpoint').value.trim(),
    llm_max_tokens: parseInt(document.getElementById('llm_max_tokens').value, 10) || 4096,
    ocr_endpoint: document.getElementById('ocr_endpoint').value.trim(),
    ocr_lang: document.getElementById('ocr_lang').value,
    max_concurrent_tabs: parseInt(document.getElementById('max_concurrent_tabs').value, 10) || 5,
    page_load_timeout: parseInt(document.getElementById('page_load_timeout').value, 10) || 30,
    stealth_mode: document.getElementById('stealth_mode').checked,
    auto_scroll: document.getElementById('auto_scroll').checked,
    api_server_enabled: document.getElementById('api_server_enabled').checked,
    api_server_port: parseInt(document.getElementById('api_server_port').value, 10) || 9333,
  };
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

function saveConfig() {
  const config = collectConfig();

  // Basic validation.
  if (config.llm_max_tokens < 256 || config.llm_max_tokens > 200000) {
    showStatus('Max Tokens must be between 256 and 200000.', 'error');
    return;
  }
  if (config.max_concurrent_tabs < 1 || config.max_concurrent_tabs > 20) {
    showStatus('Max Concurrent Tabs must be between 1 and 20.', 'error');
    return;
  }
  if (config.page_load_timeout < 5 || config.page_load_timeout > 120) {
    showStatus('Page Load Timeout must be between 5 and 120 seconds.', 'error');
    return;
  }
  if (config.api_server_port < 1024 || config.api_server_port > 65535) {
    showStatus('API Server Port must be between 1024 and 65535.', 'error');
    return;
  }

  chrome.send('saveConfig', [config]);
}

function testLLMConnection() {
  const btn = document.getElementById('test-llm');
  const resultEl = document.getElementById('llm-test-result');
  setButtonLoading(btn, true);
  resultEl.classList.add('hidden');
  chrome.send('testLLMConnection');
}

function testOCRConnection() {
  const btn = document.getElementById('test-ocr');
  const resultEl = document.getElementById('ocr-test-result');
  setButtonLoading(btn, true);
  resultEl.classList.add('hidden');
  chrome.send('testOCRConnection');
}

function saveCredential() {
  const domain = document.getElementById('cred_domain').value.trim();
  const username = document.getElementById('cred_username').value.trim();
  const password = document.getElementById('cred_password').value;

  if (!domain) {
    showStatus('Please enter a domain.', 'error');
    return;
  }
  if (!username) {
    showStatus('Please enter a username.', 'error');
    return;
  }
  if (!password) {
    showStatus('Please enter a password.', 'error');
    return;
  }

  chrome.send('saveCredential', [domain, username, password]);
}

function deleteCredential(domain) {
  if (!confirm(`Delete credential for "${domain}"?`)) return;
  chrome.send('deleteCredential', [domain]);
}

// ---------------------------------------------------------------------------
// Credentials List
// ---------------------------------------------------------------------------

function renderCredentialsList(credentials) {
  const container = document.getElementById('credentials-list');
  if (!credentials || credentials.length === 0) {
    container.innerHTML = '<div class="credentials-empty">No saved credentials.</div>';
    return;
  }

  container.innerHTML = '';
  credentials.forEach(cred => {
    const item = document.createElement('div');
    item.className = 'credential-item';
    item.innerHTML = `
      <div class="credential-info">
        <span class="credential-domain">${escapeHtml(cred.domain)}</span>
        <span class="credential-username">${escapeHtml(cred.username)}</span>
      </div>
      <button class="btn btn-danger" data-domain="${escapeHtml(cred.domain)}">Delete</button>
    `;

    const deleteBtn = item.querySelector('.btn-danger');
    deleteBtn.addEventListener('click', () => {
      deleteCredential(cred.domain);
    });

    container.appendChild(item);
  });
}

function clearCredentialForm() {
  document.getElementById('cred_domain').value = '';
  document.getElementById('cred_username').value = '';
  document.getElementById('cred_password').value = '';
}

// ---------------------------------------------------------------------------
// UI Helpers
// ---------------------------------------------------------------------------

function showStatus(message, type) {
  const bar = document.getElementById('status-bar');
  const msg = document.getElementById('status-message');
  bar.classList.remove('hidden', 'success', 'error');
  bar.classList.add(type);
  msg.textContent = message;

  // Auto-hide after 5 seconds.
  clearTimeout(showStatus._timer);
  showStatus._timer = setTimeout(() => {
    bar.classList.add('hidden');
  }, 5000);
}

function showSaveStatus() {
  const el = document.getElementById('save-status');
  el.classList.remove('hidden');
  clearTimeout(showSaveStatus._timer);
  showSaveStatus._timer = setTimeout(() => {
    el.classList.add('hidden');
  }, 3000);
}

function setButtonLoading(btn, loading) {
  const spinner = btn.querySelector('.spinner');
  if (loading) {
    btn.disabled = true;
    if (spinner) spinner.classList.remove('hidden');
  } else {
    btn.disabled = false;
    if (spinner) spinner.classList.add('hidden');
  }
}

function showTestResult(el, success, message) {
  el.classList.remove('hidden', 'success', 'error');
  el.classList.add(success ? 'success' : 'error');
  el.textContent = message || (success ? 'Connection successful!' : 'Connection failed.');
}

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}
