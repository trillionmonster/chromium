// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_API_AI_BROWSER_API_SERVER_H_
#define CHROME_BROWSER_AI_BROWSER_API_AI_BROWSER_API_SERVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

namespace ai_browser {

class AIBrowserService;

// HTTP REST API server for the AI Browser. Exposes the core service
// functionality via JSON endpoints on a configurable local port.
//
// Endpoints:
//   POST /api/analyze  — Analyze a single URL with a question
//   POST /api/search   — Search + multi-page analysis
//   POST /api/batch    — Batch analyze multiple URLs
//   GET  /api/config   — Get current AI configuration
//   PUT  /api/config   — Update AI configuration
//   GET  /api/health   — Health check
class AIBrowserAPIServer : public net::HttpServer::Delegate {
 public:
  AIBrowserAPIServer(AIBrowserService* service, int port);
  ~AIBrowserAPIServer() override;

  AIBrowserAPIServer(const AIBrowserAPIServer&) = delete;
  AIBrowserAPIServer& operator=(const AIBrowserAPIServer&) = delete;

  bool Start();
  void Stop();
  bool is_running() const { return server_ != nullptr; }
  int port() const { return port_; }

 private:
  // net::HttpServer::Delegate
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, std::string data) override;
  void OnClose(int connection_id) override;

  // Route handlers.
  void HandleAnalyze(int connection_id, const base::Value::Dict& body);
  void HandleSearch(int connection_id, const base::Value::Dict& body);
  void HandleBatch(int connection_id, const base::Value::Dict& body);
  void HandleGetConfig(int connection_id);
  void HandleSetConfig(int connection_id, const base::Value::Dict& body);
  void HandleHealth(int connection_id);

  // Utility: send JSON response.
  void SendJSON(int connection_id,
                int http_status,
                const base::Value::Dict& response);
  void SendError(int connection_id, int http_status, const std::string& error);

  raw_ptr<AIBrowserService> service_;
  int port_;
  std::unique_ptr<net::HttpServer> server_;

  base::WeakPtrFactory<AIBrowserAPIServer> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_API_AI_BROWSER_API_SERVER_H_
