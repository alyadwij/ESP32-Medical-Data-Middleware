#include <WiFi.h>
#include <ArduinoJson.h>
#include "http_client.h"

bool send_json_data(const char* endpoint, Metadata* metadata, Parameter* params, int param_count) {
  // Parse the endpoint URL to extract host, port and path
  String url = String(endpoint);
  String host;
  String path = "/your-path";  // Default path
  int port = YOUR_PORT;              // Default port
  
  // Parse URL to get host, port and path
  int protocolEnd = url.indexOf("://");
  if (protocolEnd > 0) {
    url = url.substring(protocolEnd + 3);
  }
  
  int pathStart = url.indexOf('/');
  if (pathStart > 0) {
    path = url.substring(pathStart);
    host = url.substring(0, pathStart);
  } else {
    host = url;
  }
  
  int portStart = host.indexOf(':');
  if (portStart > 0) {
    port = host.substring(portStart + 1).toInt();
    host = host.substring(0, portStart);
  }

  // Create WiFi client
  WiFiClient client;
  
  Serial.print("Connecting to: ");
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
  
  if (!client.connect(host.c_str(), port)) {
    Serial.println("WiFi client connection failed");
    return false;
  }
  
  // Create JSON document
  DynamicJsonDocument doc(2048);
  JsonObject metadata_obj = doc.createNestedObject("metadata");
  metadata_obj["alat"] = metadata->alat;
  metadata_obj["waktu"] = metadata->waktu;
  metadata_obj["id"] = metadata->id;

  JsonArray parameters = doc.createNestedArray("parameters");
  for (int i = 0; i < param_count; i++) {
    JsonObject param = parameters.createNestedObject();
    param["name"] = params[i].name;
    param["value"] = params[i].value;
  }

  String json_str;
  serializeJson(doc, json_str);

  // Send HTTP request
  client.print("POST ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.println(host);
  client.println("Connection: close");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(json_str.length());
  client.println();
  client.println(json_str);

  // Wait for response with timeout
  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 10000) {
    if (client.available()) {
      String response = client.readStringUntil('\n');
      Serial.println("HTTP Response: " + response);
      if (response.indexOf("200 OK") >= 0 || response.indexOf("201 Created") >= 0) {
        Serial.println("JSON data sent successfully via WiFi.");
        client.stop();
        return true;
      }
    }
  }

  Serial.println("Failed to receive valid HTTP response.");
  client.stop();
  return false;
}