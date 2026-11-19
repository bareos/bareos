/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "prover.h"
#include "common.h"

#include <sstream>
#include <memory>
#include <thread>

#include <curl/curl.h>
#include <curl/easy.h>
#include <jansson.h>

namespace auth::Oidc {
bool Prover::step(std::vector<char>& to_send, std::span<const char> received)
{
  switch (current_state) {
    case INITIAL: {
      json_error_t err{};
      json_ptr received_json{
          json_loadb(received.data(), received.size(), 0, &err)};
      if (!received_json) {
        return Error(std::string{"received bad json request: "} + err.text);
      }

      json_t* flows_json{};
      if (json_unpack_ex(received_json.get(), &err, 0, "{s:o}",
                         "supported_auth_flows", &flows_json)
          < 0) {
        return Error(std::string{"received bad json: "} + err.text);
      }
      if (!json_is_array(flows_json)) {
        return Error(std::string{
            "received bad json request: supported_auth_flows is not an array"});
      }

      bool device_flow_found = false;

      json_int_t index{};
      json_t* flow_json;
      json_array_foreach(flows_json, index, flow_json)
      {
        if (!json_is_string(flow_json)) {
          return Error(
              std::string{"received bad json request: supported_auth_flows is "
                          "not a string array"});
        }

        std::string flow{json_string_value(flow_json),
                         json_string_length(flow_json)};
        if (flow == "device") {
          device_flow_found = true;
          break;
        }
      }

      if (!device_flow_found) {
        return Error(
            std::string{"received bad json request: supported_auth_flows "
                        "contains no known flows"});
      }

      std::string_view selected_flow = "device";
      to_send.assign(selected_flow.begin(), selected_flow.end());

      Dmsg0(200, "Selected oidc auth flow: %.*s\n", (int)selected_flow.size(),
            selected_flow.data());

      current_state = WAITING_FOR_CHALLENGE;

      return true;

    } break;
    case WAITING_FOR_CHALLENGE: {
      json_error_t err{};
      json_ptr received_json{
          json_loadb(received.data(), received.size(), 0, &err)};
      if (!received_json) {
        return Error(std::string{"received bad json: "} + err.text);
      }

      const char* url;
      const char* code;
      const char* complete_url = nullptr;
      if (json_unpack_ex(received_json.get(), &err, 0, "{s:s, s:s, s?s}", "url",
                         &url, "code", &code, "complete_url", &complete_url)
          < 0) {
        return Error(std::string{"received bad json request: "} + err.text);
      }

      if (complete_url) {
        printf(
            "Please visit %s to verify.\nAlternatively head to %s and enter "
            "code %s\n",
            complete_url, url, code);
      } else {
        printf("Please visit %s to verify.\nThe authorization code is %s\n",
               url, code);
      }

      current_state = RESPONSE_SENT;
      return true;
    } break;
    case RESPONSE_SENT: {
      std::string_view received_str{received.data(), received.size()};

      if (received_str == "ping") {
        static constexpr std::string_view pong = "pong";
        to_send.clear();
        to_send.insert(to_send.end(), pong.begin(), pong.end());
        return true;
      } else if (received_str == "done") {
        current_state = TOKEN_RECEIVED;
        return true;
      } else {
        return Error("Unknown response");
      }
    } break;
    default: {
      return Error("Entered step() on bad state");
    } break;
  }
}
};  // namespace auth::Oidc
