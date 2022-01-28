#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <Servo.h>
#include "RTClib.h"
#include <String.h>
#include "HX711.h"
//setup
// servo_1 : đóng mở nắp, servo_2 : trộn thức ăn
const char* devide_ID ="1223" ;
const char* sensor_servo_1 ="1111" ;
const char* sensor_servo_2 ="1110";
const char* ds3231_servo ="1121" ;
double weight_static = 0;
double weight_tmp = 0;
int time_hour=0;
int time_min=0;
//task_1 : "cho an thu cong";
//task_2 : "cho an tu dong";
//task_3 : "hen gio cho an";
boolean isTask1 = false;
boolean isTask2 = false;
boolean isTask3 = false;
//WIFI
const char *ssid = "HOAI 2"; // Enter your WiFi name
const char *password = "88889999";  // Enter WiFi password
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup_wifi() {
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  randomSeed(micros());
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
//TIME
RTC_DS3231 rtc;
//Servo
Servo myservo1; 
Servo myservo2; 

int servo1_pin = 15;
//Loadcell
HX711 scale;

const int LOADCELL_DOUT_PIN = 23;
const int LOADCELL_SCK_PIN = 18;


void setup_hx711(){
   scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
   Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");
  Serial.println(scale.read());      // print a raw reading from the ADC
   scale.set_scale(1901.f); 
     scale.tare(); 
}
//MQTT

#define MQTT_SERVER "broker.emqx.io"
#define MQTT_PORT 1883
#define MQTT_USER "Minh"
#define MQTT_PASSWORD "Minh"
#define MQTT_LED_TOPIC "nodeWiFi32/led"
#define MQTT_SCHEDULE_TOPIC "nodeWiFi32/schedule"
#define MQTT_COMPLETE_TOPIC "nodeWiFi32/complete"


//int servo2_pin = SERVO_2_PIN;



// weight_static : là lượng khi cho ăn tự động 
// weight_tmp : lượng hẹn giờ or cho ăn thủ công 

DynamicJsonDocument mess_publish(200);
DynamicJsonDocument mess_subcribe(200);

void connect_to_broker() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "nodeWiFi32";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      client.subscribe(MQTT_SCHEDULE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      delay(2000);
    }
  }
}
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.println("-------new message from broker-----");
  Serial.print("topic: ");
  Serial.println(topic);
  Serial.print("message: ");
  Serial.println();
  char *messageTemp;
  messageTemp = (char *)malloc((length + 1) * sizeof(char));
  memset(messageTemp, 0, length + 1);
  for (int i = 0; i < length; i++)
  {
    messageTemp[i] = (char)payload[i];
  }
  Serial.println("sub mess: ");
  Serial.println(messageTemp);
  // chuyển mess từ char[] sang đối tượng json
  deserializeJson(mess_subcribe, messageTemp);
  // --so sánh 2 id, nếu đúng thì cân bằng time giữa ds3231 và web--
  if (strcmp(devide_ID, mess_subcribe["DevideID"]) == 0)
  {
    double tmp = scale.get_units(10);
    weight_tmp = mess_subcribe["Weight"];
    weight_tmp += tmp;
    weight_static = mess_subcribe["Weight"];

    if (strcmp(mess_subcribe["Task"], "2") == 0)
    {
      isTask2 = true;
      isTask3 = false;
      isTask1 = false;
    }
    else if (strcmp(mess_subcribe["Task"], "3") == 0)
    {
      isTask3 = true;
      isTask2 = false;
     
      //    date_time_seconds += 25170;
      time_hour = mess_subcribe["DateTime"][0];
      time_min = mess_subcribe["DateTime"][1];
      Serial.print("test: ");
      Serial.println(time_min);
      Serial.println("w1: ");
      Serial.println(weight_tmp);
    }
    else if (strcmp(mess_subcribe["Task"], "1") == 0)
    {
      isTask1 = true;
      Serial.println("w1: ");
      Serial.println(weight_tmp);
    }
    else
    {
      Serial.println("Invalid task.");
    }
  }
}

// mở nắp 

void setup() {
  myservo1.attach(servo1_pin);
//  myservo2.attach(servo2_pin);

  Serial.begin(115200);
    setup_hx711();
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  connect_to_broker();
  while (!Serial) continue;
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  mess_publish["DevideID"] = devide_ID;
 // connect_to_broker();
}
}



boolean isFeed=false;
boolean isDone=false;
int current_weight=0;
void Feeding(){
 //lượng thức ăn hiện tại có trong bát
    //current_weight=scale.get_units(10);
    if(current_weight<weight_tmp){
      myservo1.write(270);  
    //double weight = scale.get_units(10);
      double weight = 10;
      Serial.print("weight: ");
      Serial.println(weight);
      current_weight+= weight;
      Serial.print("curr weight: ");
      Serial.println(current_weight);
      delay(100);
    }else{
      delay(100);
      myservo1.write(0);
      isFeed=false;
      isDone=true;
      Serial.println("Done");
    }
}
void Publish(boolean &Task){
   DateTime now = rtc.now();
    delay(1000);
    Serial.print("date_time : ");
    Serial.print(time_hour);
    Serial.print("/");
    Serial.println(time_min);
    JsonArray Datetime = mess_publish.createNestedArray("Datetime");
    Datetime.add(now.month());
    Datetime.add(now.day());
    Datetime.add(now.hour());
    Datetime.add(now.minute());
    char buffer[256];
    mess_publish["Weight"] = weight_static;
    serializeJson(mess_publish, buffer);
    client.publish(MQTT_COMPLETE_TOPIC, buffer);
    Task = false;
    weight_tmp = 0;
    isDone=false;
    Serial.println("published");
    delay(50000);
}
void loop() {
  client.loop();
    if (!client.connected()) {
    connect_to_broker();
  }
  DateTime now = rtc.now();

  Serial.print("check: ");
  Serial.print(now.hour());
      Serial.print("/");
    Serial.println(now.minute());
    delay(1000);
  // hàm xử lý cho ăn thủ công 
  if (time_hour==now.hour() &&(time_min==now.minute())&& isTask1 == true ) {
     isFeed=true;
      Feeding();
      if(isDone==true){
        Publish(isTask1);
        Serial.println("Hi task 1");
      }
   Serial.println("hi Minh");
  }
  else if(isTask2 == true){
    isFeed=true;
    Feeding();
    if(isDone==true){
       Publish(isTask2);
       Serial.println("Hi task 2");
       isTask2=true;
    }
  }
  else if(time_hour==now.hour() &&(time_min==now.minute())&& isTask3 == true &&isTask2==false){
      isFeed=true;
      Feeding();
      if(isDone==true){
        Publish(isTask3);
        Serial.println("Hi task 3");
      }
  }

  
  // còn 2 hàm nữa , 1 là khi đến h trong timeSchedule thì nhả, 2 là nếu isAuto = 1 và lượng thức ăn nhỏ hơn weight thì nhả 
  
  
}
