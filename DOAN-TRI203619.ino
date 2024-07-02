#include <DHTesp.h>
#include <WiFi.h>
#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HardwareSerial.h>
#include <Firebase_ESP_Client.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHT11_PIN 4
DHTesp dhtSensor;

#define GAS_SENSOR_PIN 36
#define BUZZER_PIN 2
#define FIRE_PIN 34
#define QUAT_PIN 5

#define WIFI_SSID "Tang 4"
#define WIFI_PASSWORD "98654321"

#define TB_SERVER "thingsboard.cloud"
#define TOKEN "1Qjs35LE55KpDM3Oh0G0"

#define FIREBASE_HOST "https://esp32-eddf1-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "JWNyZO0Qyh1gt8u7L804Pw2fKugsJ0prn0qQYWAG"

constexpr uint16_t MAX_MESSAGE_SIZE = 128U;

WiFiClient espClient;
Arduino_MQTT_Client mqttClient(espClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);
FirebaseData fbdo; // Đối tượng FirebaseData để sử dụng cho các hoạt động Firebase

FirebaseConfig config;
FirebaseAuth auth;

HardwareSerial sim900(1);  // Sử dụng UART1 để giao tiếp với module SIM900A

const char* phoneNumber = "0585740670";

const float latitude = 21.007523;
const float longitude = 105.853279;

void connectToWiFi() {
  Serial.println("Đang kết nối đến WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nKết nối WiFi thành công");
  } else {
    Serial.println("\nKhông thể kết nối đến WiFi.");
  }
}

void connectToThingsBoard() {
  if (!tb.connected()) {
    Serial.println("Đang kết nối đến máy chủ ThingsBoard");
    
    if (!tb.connect(TB_SERVER, TOKEN)) {
      Serial.println("Không thể kết nối đến ThingsBoard");
    } else {
      Serial.println("Kết nối ThingsBoard thành công");
    }
  }
}

void connectToFirebase() {
  Serial.println("Connecting to Firebase...");
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  
  Serial.println("Connected to Firebase");
}

void sendDataToThingsBoard(float temp, int hum, int gas) {
  String jsonData = "{\"temperature\":" + String(temp) + ", \"humidity\":" + String(hum) + ", \"gas\":" + String(gas) + ", \"latitude\":" + String(latitude, 6) + ", \"longitude\":" + String(longitude, 6) + "}";
  tb.sendTelemetryJson(jsonData.c_str());
  Serial.println("Đã gửi dữ liệu lên ThingsBoard");
}

void callPhone(const char* number) {
  sim900.print("ATD");
  sim900.print(number);
  sim900.println(";");
  delay(20000);  // Chờ 20 giây trước khi kết thúc cuộc gọi
  sim900.println("ATH");  // Kết thúc cuộc gọi
}

void setup() {
  Serial.begin(115200);
  sim900.begin(9600, SERIAL_8N1, 16, 17);  // Khởi tạo UART1 với baud rate 9600, TX là GPIO16, RX là GPIO17
  dhtSensor.setup(DHT11_PIN, DHTesp::DHT11);
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Không thể khởi tạo SSD1306"));
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(QUAT_PIN, OUTPUT);
  pinMode(FIRE_PIN, INPUT);

  connectToWiFi();
  connectToThingsBoard();
  connectToFirebase();  // Kết nối tới Firebase

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("System");
  display.setCursor(0, 16);
  display.print("Loading");
  display.display();

  for (int a = 0; a < 8; a++) {
    display.setCursor(a * 16, 32); // Điều chỉnh khoảng cách ở đây nếu cần
    display.print(".");
    display.display();
    delay(200);
  }
  display.clearDisplay();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!tb.connected()) {
    connectToThingsBoard();
  }

  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float temp = data.temperature;
  int hum = data.humidity;

  int gasValue = analogRead(GAS_SENSOR_PIN);
  int gas = map(gasValue, 0, 4095, 0, 100);

  Serial.print("Temperature: ");
  Serial.println(temp);
  Serial.print("Humidity: ");
  Serial.println(hum);
  Serial.print("Gas: ");
  Serial.println(gas);
  Serial.print("Gas: ");
  Serial.println(gasValue);

  Firebase.RTDB.setFloat(&fbdo, "/Nhiet do", temp);
  Firebase.RTDB.setFloat(&fbdo, "/Do am", hum);
  Firebase.RTDB.setFloat(&fbdo, "/Gas", gas);
  
  sendDataToThingsBoard(temp, hum, gas);  // Gửi dữ liệu lên ThingsBoard

  display.clearDisplay();
  display.setTextSize(2);

  if (isnan(temp) || isnan(hum)) {
    display.setCursor(0, 0);
    display.print("Failed");
  } else {
    display.setCursor(0, 0);
    display.print("T:");
    display.print(temp);
    display.print("C");

    display.setCursor(0, 16);
    display.print("H:");
    display.print(hum);
    display.print("%");

    display.setCursor(0, 32);
    display.print("GAS:");
    display.print(gas);
    display.print(" ");
  }
  display.display();
  if (gas >= 10) {
    // Bật còi báo động và quạt khi phát hiện mức gas cao
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(QUAT_PIN, HIGH); 
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("GAS:");
    display.print(gas);
    display.setCursor(0, 16);
    display.print("WARNING!");
    display.display();
    delay(8000); // Hiển thị cảnh báo gas trong 8 giây
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(QUAT_PIN, LOW);
  }

  int fire = analogRead(FIRE_PIN);
  Serial.print("FIRE analog: ");
  Serial.println(fire);
  if (fire != 4095) {
    // Bật còi báo động khi phát hiện lửa
    digitalWrite(BUZZER_PIN, HIGH);
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.print("CANH BAO");
    display.setCursor(0, 16);
    display.print("CO CHAY!");
    display.display();
    Serial.println("Đang thực hiện cuộc gọi");
    // Gọi điện thoại khi phát hiện cháy
    callPhone(phoneNumber);
    Serial.println("CANH BAO CO CHAY! Kiem tra ngay!");

    delay(10000); // Hiển thị cảnh báo cháy trong 10 giây
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(1000);
}
