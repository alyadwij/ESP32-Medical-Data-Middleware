#include <WiFi.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Keypad.h>

// Include custom headers
#include "hl7_parser.h"
#include "http_client.h"

// WiFi Credentials
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASS"

// Display definitions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Ethernet pin mapping
#define W5500_CS   33
#define W5500_RST  32
#define W5500_MISO 25
#define W5500_MOSI 5
#define W5500_SCLK 4

//Serial mapping
#define RX_PIN 16
#define TX_PIN 17

// Buffer size for HL7 messages
#define DATA_BUFFER_SIZE 2048

// Auto-reconnect configuration
#define WIFI_RECONNECT_INTERVAL 30000      // 30 seconds
#define ETHERNET_RECONNECT_INTERVAL 10000  // 10 seconds
#define CONNECTION_CHECK_INTERVAL 5000     // 5 seconds
#define MAX_RECONNECT_ATTEMPTS 5

// Initialize display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Keypad setup
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {19, 18, 26, 27};
byte colPins[COLS] = {23, 13, 12, 15}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Menu state enumeration
enum MenuState {
  MAIN_MENU,
  TCP_MENU,
  SERIAL_MENU,
  STATUS_MENU,
  TCP_INPUT_IP,
  TCP_INPUT_PORT,
  SERIAL_INPUT_BAUD,
  SERIAL_INPUT_STOP,
  SERIAL_INPUT_DATA,
};

MenuState menuState = MAIN_MENU;

// Ethernet configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress staticIP(XXX, XXX, XX, XX);  // Default static IP for ESP32
IPAddress serverIP(XXX, XXX, XX, XX); // Default server IP
int tcpPort = XXXX;                    // Default TCP server port
EthernetServer server(tcpPort);
String endpoint = "/your-endpoint"; // Default endpoint

// TCP Configuration
String tcp_ip = "YOUR_TCP_IP";     // Default values
String tcp_port = "YOUR_TCP_PORT";

// Serial Configuration
String serial_baud = "9600";
String serial_stop = "1";
String serial_data = "8";

// Connection status and auto-reconnect variables
bool connected = false;
String connectedType = "None";
bool wifiConnected = false;
bool ethernetConnected = false;
bool autoReconnectEnabled = true;
String lastUseConnection = "None";

// Timing variables for auto-reconnect
unsigned long lastWifiCheck = 0;
unsigned long lastEthernetCheck = 0;
unsigned long lastConnectionCheck = 0;
unsigned long lastStatusUpdate = 0;
unsigned long lastWifiReconnectAttempt = 0;
unsigned long lastEthernetReconnectAttempt = 0;

// Reconnect attempt counters
int wifiReconnectAttempts = 0;
int ethernetReconnectAttempts = 0;

const unsigned long statusInterval = 1000; // per 1 detik

// HL7 parsing data structures
char data[DATA_BUFFER_SIZE] = {0};
Metadata metadata;
Parameter params[MAX_PARAM];
int param_count = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting HL7 Gateway with Auto Reconnect...");

  setupDisplay();
  
  // Initialize WiFi with auto-reconnect
  initializeWiFi();
  
  // Initialize Serial2 for HL7 data
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // RX = 2, TX = 34
  
  // Initialize W5500 Ethernet
  initializeEthernet();
  
  // Display main menu
  drawMainMenu();
  
  Serial.println("System initialized with auto-reconnect enabled");
}

void loop() {
  // Handle keypad input
  char key = keypad.getKey();
  if (key) {
    handleKey(key);
  }
  
  // Auto-reconnect management
  if (autoReconnectEnabled) {
    manageConnections();
  }
  
  // Check for incoming TCP connections
  if (ethernetConnected) {
    EthernetClient client = server.available();
    if (client) {
      Serial.println("\nTCP client connected.");
      int data_len = 0;
      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 2000 && data_len < DATA_BUFFER_SIZE - 1) {
        while (client.available()) {
          data[data_len++] = client.read();
          timeout = millis();
        }
      }
      data[data_len] = '\0';
      handleHL7(data, data_len);
      client.stop();
      Serial.println("TCP client disconnected.");
    }
  }
  
  // Check for incoming serial data
  if (Serial2.available()) {
    int data_len = 0;
    while (Serial2.available() && data_len < DATA_BUFFER_SIZE - 1) {
      data[data_len++] = Serial2.read();
    }
    data[data_len] = '\0';
    Serial.println("\nData received via Serial2:");
    Serial.println(data);
    handleHL7(data, data_len);
  }
  
  // Update status display if in status menu
  if (menuState == STATUS_MENU && millis() - lastStatusUpdate > statusInterval) {
    drawStatusMenu();
    lastStatusUpdate = millis();
  }
}

void initializeWiFi() {
  Serial.println("Initializing WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiReconnectAttempts = 0;
    Serial.println("\nWiFi connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi connection failed");
  }
}

void initializeEthernet() {
  Serial.println("Initializing Ethernet...");
  
  SPI.begin(W5500_SCLK, W5500_MISO, W5500_MOSI, W5500_CS);
  pinMode(W5500_RST, OUTPUT);
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(100);
  
  Ethernet.init(W5500_CS);
  
  // Convert string IP to IPAddress
  IPAddress ip;
  if (ip.fromString(tcp_ip)) {
    staticIP = ip;
  } else { //fallback ke default
    staticIP = IPAddress(192, 168, 18, 50);
  }

  // Initialize with static IP (no DHCP check needed for static IP)
  Ethernet.begin(mac, staticIP);
  
  delay(1000);
  
  // Check if Ethernet is properly connected
  if (Ethernet.linkStatus() == LinkON) {
    ethernetConnected = true;
    ethernetReconnectAttempts = 0;
    connected = true;
    connectedType = "Ethernet";
    lastUseConnection = "Ethernet";
    
    Serial.println("Ethernet connected.");
    Serial.print("IP Address: ");
    Serial.println(Ethernet.localIP());
    
    // Update TCP server port if needed
    int port = tcp_port.toInt();
    if (port > 0 && port != tcpPort) {
      server = EthernetServer(port);
      tcpPort = port;
    }
    
    server.begin();
    Serial.print("TCP Server active on port ");
    Serial.println(tcpPort);
  } else {
    ethernetConnected = false;
    Serial.println("Ethernet connection failed - no link detected");
  }
}

void manageConnections() {
  unsigned long currentTime = millis();
  
  // Check connection status periodically
  if (currentTime - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    checkConnectionStatus();
    lastConnectionCheck = currentTime;
  }
  
  // WiFi auto-reconnect
  if (!wifiConnected && 
      currentTime - lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL &&
      wifiReconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    
    Serial.println("Attempting WiFi reconnection...");
    reconnectWiFi();
    lastWifiReconnectAttempt = currentTime;
  }
  
  // Ethernet auto-reconnect
  if (!ethernetConnected && lastUseConnection == "Ethernet" &&
      currentTime - lastEthernetReconnectAttempt > ETHERNET_RECONNECT_INTERVAL &&
      ethernetReconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    
    Serial.println("Attempting Ethernet reconnection...");
    reconnectEthernet();
    lastEthernetReconnectAttempt = currentTime;
  }
  
  // Reset reconnect attempts if we've been trying for too long
  if (currentTime - lastWifiReconnectAttempt > WIFI_RECONNECT_INTERVAL * MAX_RECONNECT_ATTEMPTS * 2) {
    wifiReconnectAttempts = 0;
  }
  
  if (currentTime - lastEthernetReconnectAttempt > ETHERNET_RECONNECT_INTERVAL * MAX_RECONNECT_ATTEMPTS * 2) {
    ethernetReconnectAttempts = 0;
  }
}

void checkConnectionStatus() {
  // Check WiFi status
  bool previousWifiStatus = wifiConnected;
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  if (previousWifiStatus != wifiConnected) {
    if (wifiConnected) {
      Serial.println("WiFi connection restored!");
      wifiReconnectAttempts = 0;
    } else {
      Serial.println("WiFi connection lost!");
    }
  }
  
  // Check Ethernet status
  bool previousEthernetStatus = ethernetConnected;
  ethernetConnected = (Ethernet.linkStatus() == LinkON);
  
  if (previousEthernetStatus != ethernetConnected) {
    if (ethernetConnected) {
      Serial.println("Ethernet connection restored!");
      ethernetReconnectAttempts = 0;
      connected = true;
      connectedType = "Ethernet";
    } else {
      Serial.println("Ethernet connection lost!");
      connected = false;
      connectedType = "None";
    }
  }
}

void reconnectWiFi() {
  wifiReconnectAttempts++;
  Serial.printf("WiFi reconnect attempt %d/%d\n", wifiReconnectAttempts, MAX_RECONNECT_ATTEMPTS);
  
  WiFi.disconnect();
  delay(1000);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // Wait up to 10 seconds for connection
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiReconnectAttempts = 0;
    Serial.println("\nWiFi reconnected successfully!");
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi reconnection failed");
  }
}

void reconnectEthernet() {
  ethernetReconnectAttempts++;
  Serial.printf("Ethernet reconnect attempt %d/%d\n", ethernetReconnectAttempts, MAX_RECONNECT_ATTEMPTS);
  
  // Reset W5500
  digitalWrite(W5500_RST, LOW);
  delay(100);
  digitalWrite(W5500_RST, HIGH);
  delay(500);
  
  // Reinitialize Ethernet
  Ethernet.init(W5500_CS);
  
  // Initialize with static IP (no DHCP check needed)
  Ethernet.begin(mac, staticIP);
  
  delay(2000);
  
  if (Ethernet.linkStatus() == LinkON) {
    ethernetConnected = true;
    ethernetReconnectAttempts = 0;
    connected = true;
    connectedType = "Ethernet";
    lastUseConnection = "Ethernet";

    // Restart TCP server
    server = EthernetServer(tcpPort);
    server.begin();
    
    Serial.println("Ethernet reconnected successfully!");
    Serial.print("IP Address: ");
    Serial.println(Ethernet.localIP());
  } else {
    ethernetConnected = false;
    connected = false;
    connectedType = "None";
    Serial.println("Ethernet reconnection failed");
  }
}

void handleHL7(const char* input, int length) {
  if (length > 0) {
    Serial.println("HL7 Data:");
    Serial.println(input);
    param_count = 0;
    
    if (parse_hl7_message(input, length, &metadata, params, &param_count)) {
      Serial.println("HL7 parsing complete.");
      
      // Display parameters
      for (int i = 0; i < param_count; i++) {
        String paramLine = String(params[i].name) + ": " + String(params[i].value);
        Serial.println(paramLine);
      }
      
      // Send data to server (with retry logic)
      bool sendSuccess = false;
      int retryAttempts = 3;
      
      for (int attempt = 1; attempt <= retryAttempts && !sendSuccess; attempt++) {
        if (send_json_data(endpoint.c_str(), &metadata, params, param_count)) {
          Serial.println("JSON sent to endpoint.");
          sendSuccess = true;
        } else {
          Serial.printf("Failed to send JSON (attempt %d/%d)\n", attempt, retryAttempts);
          if (attempt < retryAttempts) {
            delay(1000 * attempt); // Exponential backoff
          }
        }
      }
      
      if (!sendSuccess) {
        Serial.println("All retry attempts failed. Data not sent.");
      }
    } else {
      Serial.println("HL7 parsing failed.");
    }
  }
}

void handleKey(char key) {
  switch(menuState) {
    case MAIN_MENU:
      if(key == '1') {
        menuState = TCP_MENU;
        drawTcpMenu();
      } else if(key == '2') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == '3') {
        menuState = STATUS_MENU;
        drawStatusMenu();
      } else if(key == 'A') {
        // Toggle auto-reconnection
        autoReconnectEnabled = !autoReconnectEnabled;
        Serial.printf("Auto-reconnect %s\n", autoReconnectEnabled ? "enabled" : "disabled");
        drawMainMenu();
      }
      break;

    case TCP_MENU:
      if(key == 'C') {  // Connect
        // Apply TCP settings and reconnect
        initializeEthernet();
        lastUseConnection = "Ethernet";
        drawStatusMenu();
        menuState = STATUS_MENU;
      } else if(key == 'B') { // Back
        menuState = MAIN_MENU;
        drawMainMenu();
      } else if (key == 'D') { // Clear
        tcp_ip = "";
        tcp_port = "";
        drawTcpMenu();
      } else if(key == '1') { // Edit IP
        menuState = TCP_INPUT_IP;
        drawInputScreen("IP:", tcp_ip);
      } else if(key == '2') { // Edit Port
        menuState = TCP_INPUT_PORT;
        drawInputScreen("Port:", tcp_port);
      }
      break;

    case SERIAL_MENU:
      if(key == 'C') {  // Connect
        // Apply serial settings
        int baud = serial_baud.toInt();
        if (baud > 0) {
          Serial2.end();
          int dataBits = (serial_data == "7") ? 7 : 8;
          int stopBits = (serial_stop == "2") ? 2 : 1;
          
          if (dataBits == 7 && stopBits == 1) {
            Serial2.begin(baud, SERIAL_7N1, RX_PIN, TX_PIN);
          } else if (dataBits == 7 && stopBits == 2) {
            Serial2.begin(baud, SERIAL_7N2, RX_PIN, TX_PIN);
          } else if (dataBits == 8 && stopBits == 2) {
            Serial2.begin(baud, SERIAL_8N2, RX_PIN, TX_PIN);
          } else {
            Serial2.begin(baud, SERIAL_8N1, RX_PIN, TX_PIN);
          }
          
          connected = true;
          connectedType = "Serial";
          lastUseConnection = "Serial";
          drawStatusMenu();
          menuState = STATUS_MENU;
        }
      } else if(key == 'B') { // Back
        menuState = MAIN_MENU;
        drawMainMenu();
      } else if (key == 'D') { // Clear All
        serial_baud = "";
        serial_stop = "";
        serial_data = "";
        drawSerialMenu();
      } else if(key == '1') { // Edit Baudrate
        menuState = SERIAL_INPUT_BAUD;
        drawInputScreen("Baudrate:", serial_baud);
      } else if(key == '2') { // Edit Stopbit
        menuState = SERIAL_INPUT_STOP;
        drawInputScreen("Stopbit:", serial_stop);
      } else if(key == '3') { // Edit Databit
        menuState = SERIAL_INPUT_DATA;
        drawInputScreen("Databit:", serial_data);
      }
      break;

    case STATUS_MENU:
      if(key == 'B') {
        menuState = MAIN_MENU;
        drawMainMenu();
      } else if(key == 'A') {
        // Force reconnection attempt
        Serial.println("Manual reconnection triggered...");
        if (!wifiConnected) reconnectWiFi();
        if (!ethernetConnected) reconnectEthernet();
      }
      break;

    // Input states TCP IP & Port
    case TCP_INPUT_IP:
      if(key == 'B') {
        menuState = TCP_MENU;
        drawTcpMenu();
      } else if(key == 'C') {
        menuState = TCP_MENU;
        drawTcpMenu();
      } else if(key == 'D') {
        if (tcp_ip.length() > 0) {
          // Delete dot if last character, or just delete character
          if (tcp_ip.endsWith(".")) {
            tcp_ip.remove(tcp_ip.length() - 1);
          } else {
            tcp_ip.remove(tcp_ip.length() - 1);
            if (tcp_ip.endsWith(".")) {
              tcp_ip.remove(tcp_ip.length() - 1);
            }
          }
        }
        drawInputScreen("IP:", tcp_ip);
      } else if(isDigit(key)) {
        String raw = tcp_ip;
        raw.replace(".", "");
        if (raw.length() < 12) { // max 4 octets
          raw += key;
          tcp_ip = "";
          for (int i = 0; i < raw.length(); i++) {
            tcp_ip += raw[i];
            if ((i + 1) % 3 == 0 && i < raw.length() - 1 && tcp_ip.length() < 15) {
              tcp_ip += ".";
            }
          }
        }
        drawInputScreen("IP:", tcp_ip);
      }
      break;

    case TCP_INPUT_PORT:
      if(key == 'B') {
        menuState = TCP_MENU;
        drawTcpMenu();
      } else if(key == 'C') {
        menuState = TCP_MENU;
        drawTcpMenu();
      } else if(key == 'D') {
        if (tcp_port.length() > 0) tcp_port.remove(tcp_port.length() - 1);
        drawInputScreen("Port:", tcp_port);
      } else if(isDigit(key)) {
        tcp_port += key;
        drawInputScreen("Port:", tcp_port);
      }
      break;

    // Input states serial config
    case SERIAL_INPUT_BAUD:
      if(key == 'B') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'C') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'D') {
        if (serial_baud.length() > 0) serial_baud.remove(serial_baud.length() - 1);
        drawInputScreen("Baudrate:", serial_baud);
      } else if(isDigit(key)) {
        serial_baud += key;
        drawInputScreen("Baudrate:", serial_baud);
      }
      break;

    case SERIAL_INPUT_STOP:
      if(key == 'B') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'C') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'D') {
        serial_stop = "";
        drawInputScreen("Stopbit:", serial_stop);
      } else if(key == '1' || key == '2') {
        serial_stop = key;
        drawInputScreen("Stopbit:", serial_stop);
      }
      break;

    case SERIAL_INPUT_DATA:
      if(key == 'B') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'C') {
        menuState = SERIAL_MENU;
        drawSerialMenu();
      } else if(key == 'D') {
        serial_data = "";
        drawInputScreen("Databit:", serial_data);
      } else if(key == '7' || key == '8') {
        serial_data = key;
        drawInputScreen("Databit:", serial_data);
      }
      break;
  }
}

void setupDisplay() {
  // Initialize I2C with specific pins
  Wire.begin(21, 22); // SDA=21, SCL=22
  
  // Initialize display with proper parameters
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true) {
      delay(1000);
      Serial.println("Display init failed - check wiring!");
    }
  }
  
  // Set display parameters
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();
  
  Serial.println("Display initialized successfully");
  
  // Test display
  display.setCursor(0, 0);
  display.println("HL7 Gateway v2.0");
  display.println("Auto-Reconnect ON");
  display.display();
  delay(2000);
}

void drawMainMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(F("==== MAIN MENU ===="));
  display.println();
  display.println(F("1.Ethernet"));
  display.println(F("2.Serial"));
  display.println(F("3.Status"));
  display.println();
  display.print(F("AutoRecon: "));
  display.println(autoReconnectEnabled ? F("ON") : F("OFF"));
  display.println(F("A=Toggle"));
  
  display.display();
  Serial.println("Main menu displayed");
}

void drawTcpMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(F("===== ETHERNET ====="));
  display.println();
  display.print(F("1.IP: "));
  display.println(tcp_ip.length() > 0 ? tcp_ip : "Not Set");
  display.print(F("2.Port: "));
  display.println(tcp_port.length() > 0 ? tcp_port : "Not Set");
  display.println();
  display.println(F("C=Conn D=Clear B=Back"));
  
  display.display();
  Serial.println("TCP menu displayed");
}

void drawSerialMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(F("====== SERIAL ======"));
  display.println();
  display.print(F("1.Baud: "));
  display.println(serial_baud.length() > 0 ? serial_baud : "9600");
  display.print(F("2.Stop: "));
  display.println(serial_stop.length() > 0 ? serial_stop : "1");
  display.print(F("3.Data: "));
  display.println(serial_data.length() > 0 ? serial_data : "8");
  display.println();
  display.println(F("C=Conn D=Clear B=Back"));
  
  display.display();
  Serial.println("Serial menu displayed");
}

void drawStatusMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(F("====== STATUS ======"));
  display.println();
  display.print(F("Type: "));
  display.println(connectedType);
  display.print(F("Status: "));
  display.println(connected ? F("Connected") : F("Disconnected"));
  display.println();
  display.print(F("WiFi: "));
  display.println(wifiConnected ? "Connected" : "Disconnected");
  display.println();
  display.println(F("A=Reconnect B=Back"));
  
  display.display();
}

void drawInputScreen(const char* label, String &value) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println(F("====== INPUT ======"));
  display.println();
  display.print(label);
  display.println();
  display.setTextSize(1);  // Larger text for input value
  display.println(value.length() > 0 ? value : "_");
  display.setTextSize(1);  // Back to normal size
  display.println();
  display.println(F("C=OK D=Del B=Back"));
  
  display.display();
  Serial.print("Input screen: ");
  Serial.print(label);
  Serial.print(" = ");
  Serial.println(value);
}