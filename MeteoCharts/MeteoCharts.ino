// Environment configuration file
#include "conf.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>
// #include <DHT.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <Dns.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <time.h>


/* Put your SSID & Password */
const char* ssid = WIFI_SSID;  // Enter SSID here
const char* password = WIFI_PASSWD;  //Enter Password here
const IPAddress local_IP(IP);
const IPAddress gateway(GATEWAY);
const IPAddress subnet(SUBNET);
const IPAddress primaryDNS(PRIMARY_DNS);   //optional
const IPAddress secondaryDNS(SECONDARY_DNS); //optional

// SQL
WiFiClient client;
const IPAddress server_ip(SERVER_IP);
MySQL_Connection conn((Client *)&client);
// Create an instance of the cursor passing in the connection
MySQL_Cursor cur = MySQL_Cursor(&conn);

const char db_hostname[] = DB_HOSTNAME;
const int db_port = DB_PORT;
const char database[] = DB_NAME;
char db_user[] = DB_USER;
char db_password[] = DB_PASS;

const char loc[] = LOCALIZATION; 

// DNS
DNSClient dns_client; 

// PINS
#define DHT_PIN 33
#define BME_SDA 21
#define BME_SCL 22
#define LED_BLUE 17

// Sensor type
// #define DHT_TYPE DHT22

// vars
float temp;
float hum;
int hpa;
// float lux;
int meteocharts_client_id = -1;

// sensor
Adafruit_BME280 bme;
// DHT dht(DHT_PIN, DHT_TYPE);

// cron schedule
bool data_sent = false;

void setup() {
  
  // PIN Init
  pinMode(LED_BLUE, OUTPUT);
  
  Serial.begin(115200);

  LEDTrigger(LED_BLUE, true);

  // dht.begin();
  bool status = bme.begin(0x76);
  if (!status) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
  }
  else {
    Serial.println("BMP280 connected!");
  }

  LEDTrigger(LED_BLUE, false);

  SetupWifi();
  SetupTime();

  // Begin DNS lookup
  /* 
  // Not working with WiFi
  dns_client.begin(Ethernet.dnsServerIP());
  dns_client.getHostByName(db_hostname, server_ip);
  */
  // manual setup of IP globally
  Serial.print("Server IP : ");
  Serial.println(server_ip);

  
  Serial.println("Ready!");
  for (int i = 0; i < 5; i++) {
    LEDTrigger(LED_BLUE, true);
    delay(200);
    LEDTrigger(LED_BLUE, false);
    delay(200);
  }


}

void loop(){

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  char minutes[5];
  strftime(minutes, 80, "%M", &timeinfo);

  if (strcmp(minutes, "00") == 0 || strcmp(minutes, "15") == 0 || strcmp(minutes, "30") == 0 || strcmp(minutes, "45") == 0 ) {
    PrintTime();
    GetMeteoChartsData();
    delay(1000*(60+1));
  }
}

void GetMeteoChartsData() {
  // get sensor values
  temp = bme.readTemperature();
  hum = bme.readHumidity();
  hpa = bme.readPressure()/100;
  // temp = dht.readTemperature();
  // hum = dht.readHumidity();
 
  LEDTrigger(LED_BLUE, true);
  delay(200);
  LEDTrigger(LED_BLUE, false);

  if (isnan(hum) || isnan(temp) || isnan(hpa)) {
      Serial.println("Failed to read data from sensor!");
      return;
  }
  
  if (conn.connect(server_ip, db_port, db_user, db_password)) {
    delay(1000);
    Serial.println("Connection success!");
  }
  else
    Serial.println("Connection failed.");

  char query[128];    
  /*
    * GET ID 
    */
  char SELECT_POP[] = "SELECT id FROM %s.rpi WHERE location = '%s';";

  // Here we use the QUERY_POP as the format string and query as the
  // destination. This uses twice the memory so another option would be
  // to allocate one buffer for all formatted queries or allocate the
  // memory as needed (just make sure you allocate enough memory and
  // free it when you're done!).
  sprintf(query, SELECT_POP, database, loc);

  meteocharts_client_id = QuerySelect(query);

  Serial.print("ID : ");
  Serial.print(meteocharts_client_id);
  Serial.print(" - ");
  Serial.print(temp);
  Serial.print("°C");
  Serial.print(" - ");
  Serial.print(hum);
  Serial.print("%");
  Serial.print(" - ");
  Serial.print(hpa);
  Serial.println(" hPa");
  
  /*
   * Insert Temperature
   */
  char TEMP_INS[] = "INSERT INTO %s.temperature(rpi_id, degree) VALUES ('%i', '%f');";
  sprintf(query, TEMP_INS, database, meteocharts_client_id, temp);
  QueryInsert(query);

  /*
   * Insert Humidity
   */
  char HUM_INS[] = "INSERT INTO %s.humidity(rpi_id, percentage) VALUES ('%i', '%f');";
  sprintf(query, HUM_INS, database, meteocharts_client_id, hum);
  QueryInsert(query);

  /*
   * Insert Pressure
   */  
  char HPA_INS[] = "INSERT INTO %s.pressure(rpi_id, hpa) VALUES ('%i', '%i');";
  sprintf(query, HPA_INS, database, meteocharts_client_id, hpa);
  QueryInsert(query);
    
  /*
   * Insert Light
   * 
  const char LUX_INS[] = "INSERT INTO %s.light(rpi_id, lux) VALUES ('%i', '%f');";
  sprintf(query, LUX_INS, database, meteocharts_client_id, lux);
  QueryInsert(query);
  
  */

  conn.close();
}


void QueryInsert(char query[128]) {
  LEDTrigger(LED_BLUE, true);
  int result;

  row_values *row = NULL;

  // Initiate the query class instance
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
  
  // Execute the query
  cur_mem->execute(query);
  delete cur_mem;
  LEDTrigger(LED_BLUE, false);
}

int QuerySelect(char query[128]) {
  int result;

  row_values *row = NULL;

  // Initiate the query class instance
  MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

  // Execute the query
  cur_mem->execute(query);
  // Fetch the columns (required) but we don't use them.
  column_names *columns = cur_mem->get_columns();

  // Read the row (we are only expecting the one)
  do {
    row = cur_mem->get_next_row();
    if (row != NULL) {
      result = atol(row->values[0]);
    }
  } while (row != NULL);
  // Deleting the cursor also frees up memory used
  delete cur_mem;

  return result;
}

void LEDTrigger(int color, bool on){
  if (on)
    digitalWrite(color, LOW);
  else
    digitalWrite(color, HIGH);
}

void SetupTime() {
  LEDTrigger(LED_BLUE, true);
  configTime(0 /* tz */, 0 /* dst */, TIME_ZONE_1, TIME_ZONE_2, TIME_ZONE_3);
  while (time(nullptr) < 1535920965) {
    LEDTrigger(LED_BLUE, false);
    delay(10);
    LEDTrigger(LED_BLUE, true);
  }
  setenv("TZ", TIME_ZONE_FORMAT,1); 
  tzset();  
  PrintTime();
  LEDTrigger(LED_BLUE, false);
}

void SetupWifi() {
  // Connect to Wi-Fi network with SSID and password
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  Serial.print("Connecting to : ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    LEDTrigger(LED_BLUE, true);
    delay(500);
    Serial.print("-");
    LEDTrigger(LED_BLUE, false);
    delay(500);
  }  

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.print("IP address : ");
  Serial.println(WiFi.localIP());

  Serial.println("Pinging SQL Server");
  bool ret = Ping.ping(DB_HOSTNAME);
  float avg_time_ms = Ping.averageTime();
  Serial.print("Ping : ");
  Serial.print(ret);
  Serial.print(" in ");
  Serial.println(avg_time_ms);

  LEDTrigger(LED_BLUE, false);
}

void PrintTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  char output[80];
  strftime(output, 80, "%d-%b-%y, %H:%M:%S", &timeinfo);
  Serial.print("Time : ");
  Serial.println(String(output));
}