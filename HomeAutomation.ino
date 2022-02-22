

#include <WiFiEspAT.h>  // Include WiFiEspAT library
#include <EEPROM.h>     // Include EEPROM library
#include <ArduinoJson.h>  // ArduinoJson
StaticJsonDocument<150> doc;


const char ssid[] = "TOPLINK_BD4";    // WIFI NAME
const char pass[] = "Pass123@#";    // WIFI PASSWORD


char* password = "PASS";   // Password for connection requests.

int relays[] = {NULL, 2};       // Relay pins

//Gas sensor pin
int sensor = A1;
int gas_value = 0;
int gas_state_eeprom;
int gas_threshold = 390;

int count;
int relayState;
int body = 0;
String respond = "";
char requestBody[100];
boolean relayrequest = false;
boolean get_all_request = false;

#if defined(ARDUINO_ARCH_AVR) && !defined(HAVE_HWSERIAL1)
#include <SoftwareSerial.h>
SoftwareSerial Serial1(19,18); // RX, TX
#define BANDWIDTH 9600
#else
#define BANDWIDTH 115200
#endif

WiFiServer server(80);

void setup() {

   for (count = 1; count <= 2; count++) {
    relayState = EEPROM.read(count);
    pinMode(count+1, OUTPUT);
    if (relayState == 1) {
      onOff(count, "ON", false);
    } else {
      onOff(count, "OFF", false);
    }
  }

  pinMode(sensor, INPUT);

  Serial.begin(115200);
  while (!Serial);

  // Initiallizing ESP8266
  setEspBaudRate(BANDWIDTH);

  WiFi.init(Serial1);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("ESP8266 not found"));
    while (true);
  }

  int status = WiFi.begin(ssid, pass);

  Serial.println(F("Connecting to WIFI....: "));
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print('.');
  }
  Serial.println();
  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.println(F("WiFi connected."));
  Serial.print(F("For request to the server \"http://"));
  Serial.print(ip);
  Serial.println(F("/\" Link."));
}

void loop() {

  WiFiClient client = server.available(); //HTTP request
  if (client) {
    IPAddress ip = client.remoteIP();
    Serial.print(F("New client: "));
    Serial.println(ip);

    Serial.println(F("--> Data request"));
    while (client.connected()) {
      if (client.available()) {
        // Setting gas variable to zero
        memset(requestBody, 0, sizeof requestBody);
        body = 0;

        while (client.available()) {
          char c = client.read();
          Serial.write(c);
          if (body != 4) {
            if (c == '\r' || c == '\n') { // r or n
              body ++;
            } else {
              body = 0;
            }
          }
          if (body == 4 && c != '\n') {
            strncat(requestBody, &c, 1);
          }
        }
        Serial.println();
        Serial.println(F("--> Received posted data:"));
        Serial.println(requestBody);
        
        char* token = strtok(requestBody, "&");
        if (strncmp(token, "pass=", 5) == 0) {
          char* PWD = token + 5;
          if (strcmp(PWD, password) == 0) {
            Serial.println(F("--> Password is correct, access granted!"));
            while (token != NULL) {
              token = strtok(NULL, "&");
              
              // Röle
              if (strncmp(token, "rl=", 3) == 0) {
                relayrequest = true;
                if (get_all_request == false) {
                  Serial.print(F("--> Relay number: "));
                  int relayNumber = atol(token + 3);
                  Serial.print(relayNumber);
                  Serial.print(F(" Command: "));
                  char* relayCommand = token + 4;
                  Serial.println(relayCommand);
                  onOff(relayNumber, relayCommand, true);
                  respond = "";
                  doc["status"] = F("OK");
                  doc["rl"] = (String)relayNumber;
                  doc["relayCommand"] = (String)relayCommand;
                  serializeJson(doc, respond);
                }
              }
              //Get all request
              if (strncmp(token, "getall", 6) == 0) {
                get_all_request == true;
                if (relayrequest == false) {
                  Serial.println(F("--> get all REQUEST"));
                  respond = ""; 
                  // GAS sensor data is read from EEPROM at address 9:
                  gas_state_eeprom = EEPROM.read(9);
                  doc["status"] = F("OK");
                  doc["gas"] = gas_state_eeprom;
                  
                  JsonArray relays = doc.createNestedArray("relays");
                  for (count = 1; count <= 2; count++) {
                    relayState = EEPROM.read(count);
                    relays.add(relayState);
                  }
                  serializeJson(doc, respond);
                 
                  if (gas_state_eeprom == 1) {
                    EEPROM.update(9, 0);
                  }
                
                }
              }
            }
          } else {
            Serial.println(F("--> Password provided is incorrect!"));
            respond = "";
            doc["status"] = F("ERROR");
            doc["message"] = F("Password Incorrect");
            serializeJson(doc, respond);
          }
        } else {
          Serial.println(F("--> Parameter passed not sent"));
          respond = "";
          doc["status"] = F("ERROR");
          doc["message"] = F("Parameter passed not sent");
          serializeJson(doc, respond);
        }
        // At the end of HTTP header a blank line appears
        //It signifies that HTTP request has reached the end, a resqust is sent.
        
        Serial.println(F("--> Reply forwarded"));
        Serial.println(respond);

        // http respond headers
        client.println(F("200 OK"));
        client.println();
        client.print(respond);
        client.flush();
        relayrequest = false;
        get_all_request == false;
        doc.clear();
        break;
      }
    }

    // close connection:
    client.stop();
    Serial.println(F("--> Connection closed"));
  }

 
  // Gas sensor data reading
  gas_value = analogRead(sensor);
  // Reading at address 9 of the EEPROM
  gas_state_eeprom = EEPROM.read(9);
  
  if (gas_value >= gas_threshold) {
    if (gas_state_eeprom != 1) {
      EEPROM.update(9, 1);
    }
  } else { // If below the limit value
    // If the 9th address of the EEPROM is 1, The data will not be converted to 0 because the data is yet not received by the client.
    if (gas_state_eeprom != 1) {
      EEPROM.update(9, 0);
    }
  }
}

// This function sets the Baudrate of ESP8266
void setEspBaudRate(unsigned long baudrate) {
  long rates[6] = {115200, 74880, 57600, 38400, 19200, 9600};

  Serial.print(F("Setting baudrate to: "));
  Serial.print(baudrate);
  Serial.println(F("..."));

  for (int i = 0; i < 6; i++) {
    Serial1.begin(rates[i]);
    delay(150);
    Serial1.print(F("AT+UART_DEF="));
    Serial1.print(baudrate);
    Serial1.print(F(",8,1,0,0\r\n"));
    delay(150);
  }

  Serial1.begin(baudrate);
}

// Relay function
void onOff(int relayNumber, String relayState, boolean updateEeprom) {
  if (relayState == "ON") {
    digitalWrite(relays[relayNumber], LOW);
    if (updateEeprom == true) {
      EEPROM.update(relayNumber, 1);
    }
  }
  if (relayState == "OFF") {
    digitalWrite(relays[relayNumber], HIGH);
    if (updateEeprom == true) {
      EEPROM.update(relayNumber, 0);
    }
  }
}
