// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/api/ai_browser_api_server.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ai_browser/ai_browser_service.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_server_socket.h"
#include "url/gurl.h"

namespace ai_browser {

AIBrowserAPIServer::AIBrowserAPIServer(AIBrowserService* service, int port)
    : service_(service), port_(port) {}

AIBrowserAPIServer::~AIBrowserAPIServer() {
  Stop();
}

bool AIBrowserAPIServer::Start() {
  if (server_) return true;

  auto socket =
      std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
  int result = socket->ListenWithAddressAndPort("127.0.0.1", port_, 10);
  if (result != net::OK) {
    LOG(ERROR) << "AI Browser API: Failed to listen on port " << port_
               << ": " << net::ErrorToString(result);
    return false;
  }

  server_ = std::make_unique<net::HttpServer>(std::move(socket), this);
  LOG(INFO) << "AI Browser API server started on http://127.0.0.1:" << port_;
  return true;
}

void AIBrowserAPIServer::Stop() {
  server_.reset();
}

void AIBrowserAPIServer::OnConnect(int connection_id) {}

void AIBrowserAPIServer::OnHttpRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // Add CORS headers for all responses.
  std::string method = info.method;
  std::string path = info.path;

  // Handle CORS preflight.
  if (method == "OPTIONS") {
    server_->SendRaw(
        connection_id,
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Access-Control-Max-Age: 86400\r\n"
        "\r\n",
        net::NetworkTrafficAnnotationTag::NotReached());
    return;
  }

  // Route requests.
  if (path == "/api/health" && method == "GET") {
    HandleHealth(connection_id);
    return;
  }

  if (path == "/api/config" && method == "GET") {
    HandleGetConfig(connection_id);
    return;
  }

  // Parse JSON body for POST/PUT endpoints.
  base::Value::Dict body;
  if (method == "POST" || method == "PUT") {
    auto parsed = base::JSONReader::Read(info.data);
    if (parsed && parsed->is_dict()) {
      body = std::move(parsed->GetDict());
    }
  }

  if (path == "/api/analyze" && method == "POST") {
    HandleAnalyze(connection_id, body);
  } else if (path == "/api/search" && method == "POST") {
    HandleSearch(connection_id, body);
  } else if (path == "/api/batch" && method == "POST") {
    HandleBatch(connection_id, body);
  } else if (path == "/api/config" && method == "PUT") {
    HandleSetConfig(connection_id, body);
  } else {
    SendError(connection_id, 404, "Not found: " + path);
  }
}

void AIBrowserAPIServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  // WebSocket not used for now.
  server_->Close(connection_id);
}

void AIBrowserAPIServer::OnWebSocketMessage(int connection_id,
                                            std::string data) {}

void AIBrowserAPIServer::OnClose(int connection_id) {}

// -- Route Handlers --

void AIBrowserAPIServer::HandleHealth(int connection_id) {
  base::Value::Dict response;
  response.Set("status", "ok");
  response.Set("service", "ai-browser");
  SendJSON(connection_id, 200, response);
}

void AIBrowserAPIServer::HandleAnalyze(int connection_id,
                                       const base::Value::Dict& body) {
  const std::string* url_str = body.FindString("url");
  const std::string* question = body.FindString("question");

  if (!url_str || !question) {
    SendError(connection_id, 400,
              "Missing required fields: 'url' and 'question'");
    return;
  }

  GURL url(*url_str);
  if (!url.is_valid()) {
    SendError(connection_id, 400, "Invalid URL: " + *url_str);
    return;
  }

  service_->AnalyzePage(
      url, *question,
      base::BindOnce(
          [](base::WeakPtr<AIBrowserAPIServer> self, int conn_id,
             AnalyzeResult result) {
            if (!self) return;
            base::Value::Dict response;
            response.Set("success", result.success);
            response.Set("answer", result.answer);
            response.Set("url", result.url);
            response.Set("extraction_method", result.extraction_method);
            if (!result.error.empty()) {
              response.Set("error", result.error);
            }
            self->SendJSON(conn_id, result.success ? 200 : 500, response);
          },
          weak_factory_.GetWeakPtr(), connection_id));
}

void AIBrowserAPIServer::HandleSearch(int connection_id,
                                      const base::Value::Dict& body) {
  const std::string* query = body.FindString("query");
  if (!query) {
    SendError(connection_id, 400, "Missing required field: 'query'");
    return;
  }

  int max_results = body.FindInt("max_results").value_or(10);

  service_->SearchAndAnalyze(
      *query, max_results,
      base::BindOnce(
          [](base::WeakPtr<AIBrowserAPIServer> self, int conn_id,
             SearchAnalyzeResult result) {
            if (!self) return;
            base::Value::Dict response;
            response.Set("success", result.success);
            response.Set("answer", result.answer);
            if (!result.error.empty()) {
              response.Set("error", result.error);
            }

            base::Value::List sources;
            for (const auto& src : result.sources) {
              base::Value::Dict s;
              s.Set("url", src.url);
              s.Set("success", src.success);
              s.Set("extraction_method", src.extraction_method);
              sources.Append(std::move(s));
            }
            response.Set("sources", std::move(sources));
            self->SendJSON(conn_id, result.success ? 200 : 500, response);
          },
          weak_factory_.GetWeakPtr(), connection_id));
}

void AIBrowserAPIServer::HandleBatch(int connection_id,
                                     const base::Value::Dict& body) {
  const base::Value::List* urls_list = body.FindList("urls");
  const std::string* question = body.FindString("question");

  if (!urls_list || !question) {
    SendError(connection_id, 400,
              "Missing required fields: 'urls' (array) and 'question'");
    return;
  }

  std::vector<GURL> urls;
  for (const auto& item : *urls_list) {
    if (item.is_string()) {
      GURL url(item.GetString());
      if (url.is_valid()) {
        urls.push_back(url);
      }
    }
  }

  if (urls.empty()) {
    SendError(connection_id, 400, "No valid URLs provided");
    return;
  }

  service_->BatchAnalyze(
      urls, *question,
      base::BindOnce(
          [](base::WeakPtr<AIBrowserAPIServer> self, int conn_id,
             std::vector<AnalyzeResult> results) {
            if (!self) return;
            base::Value::Dict response;
            response.Set("success", true);

            base::Value::List items;
            for (const auto& r : results) {
              base::Value::Dict item;
              item.Set("url", r.url);
              item.Set("success", r.success);
              item.Set("answer", r.answer);
              item.Set("extraction_method", r.extraction_method);
              if (!r.error.empty()) item.Set("error", r.error);
              items.Append(std::move(item));
            }
            response.Set("results", std::move(items));
            self->SendJSON(conn_id, 200, response);
          },
          weak_factory_.GetWeakPtr(), connection_id));
}

void AIBrowserAPIServer::HandleGetConfig(int connection_id) {
  // TODO: Read from PrefService and return all ai_browser.* prefs.
  base::Value::Dict response;
  response.Set("status", "ok");
  response.Set("message", "Use chrome://ai-settings to configure");
  SendJSON(connection_id, 200, response);
}

void AIBrowserAPIServer::HandleSetConfig(int connection_id,
                                         const base::Value::Dict& body) {
  // TODO: Write to PrefService.
  base::Value::Dict response;
  response.Set("status", "ok");
  response.Set("message", "Configuration updated");
  SendJSON(connection_id, 200, response);
}

// -- Utility --

void AIBrowserAPIServer::SendJSON(int connection_id,
                                  int http_status,
                                  const base::Value::Dict& response) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      response, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  server_->Send(connection_id, net::HTTP_OK, json, "application/json",
                {{"Access-Control-Allow-Origin", "*"}});
}

void AIBrowserAPIServer::SendError(int connection_id,
                                   int http_status,
                                   const std::string& error) {
  base::Value::Dict response;
  response.Set("success", false);
  response.Set("error", error);
  SendJSON(connection_id, http_status, response);
}

}  // namespace ai_browser
