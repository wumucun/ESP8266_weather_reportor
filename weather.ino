#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <U8g2lib.h>
#include <Arduino.h>
#include "DHT.h"
#define DHTPIN D4
#define DHTTYPE DHT11

WiFiClient client;
PubSubClient pub_client(client);

//mqtt服务器
const char* mqttServer = "test.ranye-iot.net";

// 天气服务器
const char* host = "api.seniverse.com";
const int httpPort = 80;
         
// 心知天气HTTP请求所需信息
//私钥
String reqUserKey = "SNEZ-20MJzl6BvQ5o";
// 城市
String reqLocation = "bijie";     
 // 摄氏||华氏       
String reqUnit = "c";   
     
//这里使用的屏幕为64*48像素，U8G2_MIRROR用于镜像显示             
U8G2_SSD1306_64X48_ER_F_HW_I2C u8g2(U8G2_R0, 2, 1, U8X8_PIN_NONE);

DHT dht(DHTPIN, DHTTYPE);
HTTPClient http;

//温度传感器取得的温湿度
int temp, hum;

void setup() {
  Serial.begin(115200);
  //从D4这个接口读取温湿度的数据
  pinMode(D4, INPUT);
  
  //设置ESP8266工作模式为无线终端模式
  WiFi.mode(WIFI_STA);

  dht.begin();
  //因为DHT串行传输数据，我们需要用DHT里的函数来读取温度和湿度。
  temp = (int)(dht.readTemperature());
  hum = (int)(dht.readHumidity());

  //u8g2为屏幕绘制的库
  u8g2.begin();
  //u8g2_font_ncenB08_tr：先设置显示屏显示什么类型的文字，中文需要设置成另外的参数。
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.enableUTF8Print();

  //清除屏幕缓存
  u8g2.clearBuffer();
  u8g2.drawStr(5, 30, "Waiting");
  u8g2.sendBuffer();
  
  //连接WiFi，连接不成功时屏幕保持显示“waiting”
  while (!connectWiFi());
  
  // 设置MQTT服务器和端口号
  pub_client.setServer(mqttServer, 1883);
  // 连接MQTT服务器
  connectMQTTServer();
  
  //获取时间的URL
  String GetUrl = "http://quan.suning.com/getSysTime.do";
  http.setTimeout(5000);
  http.begin(client, GetUrl);
  delay(1000);
  
  u8g2.clearBuffer();
  u8g2.drawStr(5, 20, "Welcome！");
  u8g2.setCursor(2, 43);
  
  u8g2.print("Makers Go!");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  close();
  u8g2.sendBuffer();
  delay(1000);
}
int i = 0;//用于交叉播放温湿度和天气预报
void loop() {
  u8g2.firstPage();
  do {
    if (i > 30000)
      i = 0;
    if (i % 2 == 0) {
      //显示环境温湿度
      envirTemp();
      //向订阅者发送当前的温湿度
      pubMqttMag();
    }
    else {
      // 建立心知天气API当前天气请求资源地址
      String reqRes = "/v3/weather/now.json?key=" + reqUserKey +
                      + "&location=" + reqLocation +
                      "&language=en&unit=" + reqUnit;

      // 向心知天气服务器服务器请求信息并对信息进行解析
      httpRequest(reqRes);
    }
  } while ( u8g2.nextPage() );
  i++;
  delay(10000);
}

// 连接WiFi
bool connectWiFi() {
  // 建立WiFiManager对象
  WiFiManager wifiManager;
  //建立热点，其他终端连接到8266之后跳转到指定页面进行网络配置
  wifiManager.autoConnect("AutoConnectAP");
  // WiFi连接成功后将通过串口监视器输出连接成功信息
  Serial.println("");
  Serial.print("ESP8266 Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());
  return true;
}

// 向心知天气服务器服务器请求信息并对信息进行解析
void httpRequest(String reqRes) {
  // 建立http请求信息
  String httpRequest = String("GET ") + reqRes + " HTTP/1.1\r\n" +
                       "Host: " + host + "\r\n" +
                       "Connection: close\r\n\r\n";
  Serial.println("");
  Serial.print("Connecting to "); Serial.print(host);

  // 尝试连接服务器
  if (client.connect(host, 80)) {
    Serial.println(" Success!");

    // 向服务器发送http请求信息
    client.print(httpRequest);
    Serial.println("Sending request: ");
    Serial.println(httpRequest);

    // 获取并显示服务器响应状态行
    String status_response = client.readStringUntil('\n');
    Serial.print("status_response: ");
    Serial.println(status_response);

    // 使用find跳过HTTP响应头
    if (client.find("\r\n\r\n")) {
      Serial.println("Found Header End. Start Parsing.");
    }

    // 利用ArduinoJson库解析心知天气响应信息
    parseInfo(client);
  } else {
    Serial.println(" connection failed!");
  }
  //断开客户端与服务器连接工作
  client.stop();
}


// 利用ArduinoJson库解析心知天气响应信息
void parseInfo(WiFiClient client) {
  const size_t capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(6) + 230;
  DynamicJsonDocument doc(capacity);

  deserializeJson(doc, client);

  JsonObject obj1 = doc["results"][0];
  String cityName = obj1["location"]["name"].as<String>();
  String weather = obj1["now"]["text"].as<String>();
  String temperature = obj1["now"]["temperature"].as<String>();
  Serial.println(cityName);
  Serial.println(weather);
  Serial.println(temperature);
  u8g2.clearBuffer();
  u8g2.drawStr(5, 30, "OutSide");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  
  //64*48,横坐标为64，纵坐标为48，原点在屏幕的左上角。在坐标为（6,13）的地方插入“Tem”。
  u8g2.setCursor(6, 13);
  u8g2.print(cityName);
  u8g2.setCursor(6, 28);
  u8g2.print(weather);
  u8g2.setCursor(6, 43);
  u8g2.print("Tem:");
  u8g2.setCursor(38, 43);
  u8g2.print(temperature);
  
  //摄氏度的圈
  u8g2.drawCircle(51, 33, 1, U8G2_DRAW_ALL);
  u8g2.drawStr(53, 43, "C");
  u8g2.sendBuffer();
  delay(5000);
  
  //读取时间
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      //读取响应内容
      String response = http.getString();
      Serial.println(response);
      //显示日期和时间
      u8g2.clearBuffer();
      u8g2.setCursor(5, 40);
      u8g2.print(response.substring(13, 23));
      u8g2.setCursor(18, 20);
      u8g2.print(response.substring(24, 29));
      u8g2.sendBuffer();
      delay(500);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  delay(3000);
}


//传感器温湿度显示
void envirTemp() {
  Serial.println(temp);
  Serial.println(hum);
  u8g2.clearBuffer();
  u8g2.drawStr(5, 30, "InSide");
  u8g2.sendBuffer();
  delay(1000);
  u8g2.clearBuffer();
  u8g2.drawStr(6, 13, "Tem:");//64*48,横坐标为64，纵坐标为48，原点在屏幕的左上角。在坐标为（6,13）的地方插入“Tem”。
  u8g2.setCursor(39, 13);
  u8g2.print(temp);
  u8g2.drawCircle(52, 3, 1, U8G2_DRAW_ALL);
  u8g2.drawStr(53, 13, "C");
  u8g2.drawStr(6, 38, "Hum:");
  u8g2.drawStr(52, 38, "%");
  u8g2.setCursor(39, 38);
  u8g2.print(hum);
  u8g2.sendBuffer();
}

//开机时连接到WiFi后的一个动画
void close()
{
  u8g2.clear();
  u8g2.drawBox(0, 32 / 2 - 3, 64, 6);
  u8g2.drawBox(64 / 2 - 3, 0, 6, 32);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clear();
  u8g2.drawBox(0, 32 / 2 - 2, 64, 4);
  u8g2.drawBox(64 / 2 - 2, 0, 4, 32);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clear();
  u8g2.drawBox(0, 32 / 2 - 1, 64, 2);
  u8g2.drawBox(64 / 2 - 1, 0, 2, 32);
  u8g2.sendBuffer();
  delay(10);
  u8g2.clear();
}

//连接mqtt服务器
void connectMQTTServer() {
  // 根据ESP8266的MAC地址生成客户端ID（避免与其它ESP8266的客户端ID重名）
  String clientId = "esp8266-" + WiFi.macAddress();
  // 连接MQTT服务器
  if (pub_client.connect(clientId.c_str())) {
    Serial.println("MQTT Server Connected");
    Serial.println("Server Address: ");
    Serial.println(mqttServer);
    Serial.println("ClientId:");
    Serial.println(clientId);
  } else {
    Serial.print("MQTT Server Connect Failed. Client State:");
    Serial.println(pub_client.state());
    delay(3000);
  }
}

//向订阅者发送消息
void pubMqttMag() {
  DynamicJsonDocument data(256);
  data [ "group"] = 20;
  data [ "temp"] = temp;
  data [ "hum" ] = hum;
  //publish temperature and humidity
  char json_string[256];
  serializeJson (data, json_string);
  Serial.println(json_string) ;
  //主题
  String topicString = "scu/building_base_b/323/temp";
  char publishTopic[topicString.length() + 1];
  strcpy(publishTopic, topicString.c_str());
  if (!pub_client.publish (publishTopic, json_string, false)) {
    Serial.println("Message Publish Failed.");
  }
}
