#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <TelegramBot.h>
#include <ArduinoJson.h>
#include <FS.h>   //Include File System Headers
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <time.h>    
#include <SPI.h>
#include <MFRC522.h> // библиотека "RFID".   
#include <Servo.h>        
#define BUFFER_SIZE 100
#define pinHC_SR501 D1
#define pinLed D8
#define pinLight A0
#define SS_PIN D4
#define RST_PIN D2
Servo servo;
MFRC522 mfrc522(SS_PIN, RST_PIN);
unsigned long uidDec, uidDecTemp;  // для храниения номера метки в десятичном формате
const char* ssid = "TP-LINK_B250"; // Имя вайфай точки доступа
const char* pass = "47334373"; // Пароль от точки доступa
const char* mqtt_server = "m15.cloudmqtt.com"; // Имя сервера MQTT
const int mqtt_port = 12784; // Порт для подключения к серверу MQTT
const char* mqtt_user = "euneiisl"; // Логи от сервер
const char* mqtt_pass = "3LSLJ_Sk5mIP"; // Пароль от сервера
const char* filenameRFID = "/fileRFID.txt";
String CHAT_ID = "373416230";
String choose, led, signaling_on_off, control;
String choose_file;
unsigned long time_blink;
unsigned long time_info;
int count_on = 0;
int tm = 1000;
int tm_signaling = 100;
int pinHC_SR501_Value;
struct Key {
    int keyRFID;
    String openName;
};
Key key[10] = { {0,""}, {0,""}, {0,""}, {0,""}, {0,""},
                       {0,""}, {0,""}, {0,""}, {0,""}, {0,""}, };
//static key key_struct[4] = { {2524347162, "Tomas"}, {2270357276, "Tomas1"},
//{3108728064, "Tomas2"}, {1743025187, "Tomas3"} };
void callback(const MQTT::Publish& pub)
{
    if (pub.topic() == "house/led")
    {
        led = String(pub.payload_string());
        if (led == "0") { digitalWrite(pinLed, LOW); delay(10); }
        else if (led == "1") { digitalWrite(pinLed, HIGH); delay(10); }
        //else if(led == "2"){ autoLed(); delay(10); }
    }
    if (pub.topic() == "house/signaling")
    {
        signaling_on_off = String(pub.payload_string());
    }
    if (pub.topic() == "house/control")
    {
        control = String(pub.payload_string());
        if (control == "format") { formatRFID(); delay(10); }
    }

    if (pub.topic() == "house/file")
    {
        choose_file = String(pub.payload_string());
        if (choose_file == "format") { formatSPIFFS(); delay(10); }
        else if (choose_file == "rfid") { printFileRFID(); delay(10); }
    }
}
const char* BotToken = "798189881:AAGN19z8cn6dS6WAavTSF8DKrgdvpAOuPIk";
WiFiClientSecure net_ssl;
TelegramBot bot(BotToken, net_ssl);
ESP8266WebServer server(80);
WiFiClient wclient;
PubSubClient client(wclient, mqtt_server, mqtt_port);
void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println();
    Serial.println();
    pinMode(pinLed, OUTPUT);
    pinMode(pinHC_SR501, INPUT);
    pinMode(pinLight, INPUT);
    digitalWrite(pinLed, LOW);
    // подключаемся к wi-fi
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.print("Connected to WiFi. IP:");
    Serial.println(WiFi.localIP());
    bot.begin();
    SPI.begin();  //  инициализация SPI / Init SPI bus.
    mfrc522.PCD_Init();     // инициализация MFRC522 / Init MFRC522 card.
    servo.attach(D0);
    servo.write(0);  // устанавливаем серву в закрытое сосотояние
    if (SPIFFS.begin()) {
        Serial.println("SPIFFS Initialize....ok");
    }
    else {
        Serial.println("SPIFFS Initialization...failed");
    }
}
void loop() {
    // подключаемся к MQTT серверу
    if (WiFi.status() == WL_CONNECTED) {
        if (!client.connected()) {
            Serial.println("Connecting to MQTT server");
            if (client.connect(MQTT::Connect("Movement")
                .set_auth(mqtt_user, mqtt_pass))) {
                Serial.println("Connected to MQTT server");
                client.set_callback(callback);
                client.subscribe("test/choose");
                client.subscribe("house/led");
                client.subscribe("house/signaling");
                client.subscribe("house/control");
                client.subscribe("house/file");
            }
            else {
                Serial.println("Could not connect to MQTT server");
            }
        }
        if (client.connected()) {
            client.loop();
            // Проверка входящих сообщений с MQTT приложения
            if (led == "2") { autoLed(); delay(10); }
            else { infoMovement(); choose = ""; delay(10); }
            if (signaling_on_off == "on") { infoSignaling(); delay(10); }
            else if (signaling_on_off == "off") { signaling_on_off = ""; }
            if (control == "addkeyon") { addKeyTelegram(); delay(10); }
            else if (control == "format") { formatRFID(); delay(10); }
            else if (control == "addkeyoff") { control = ""; }
            else { rfidKey(); }
        }
    }
    delay(100);
}

void rfidKey() {
    // Поиск новой метки
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    // Выбор метки
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    uidDec = 0;
    // Выдача серийного номера метки.
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    Serial.println("Card UID: ");
    Serial.println(uidDec); // Выводим UID метки в консоль.
    for (int i = 0; i < 10; i++) {
        if (uidDec == key[i].keyRFID) {
            Serial.println("HAHAHAHA -_- ");
            Serial.println(key[i].openName);
            timeSynch(3);
            File f = SPIFFS.open(filenameRFID, "a");
            f.print("Door open at : ");
            f.print(GetTime());
            f.print(" ");
            f.print(GetDate());
            f.print("Opener : ");
            f.print(key[i].openName);
            f.print("\n");
            f.close();  //Close file
            servo.write(90); // Поворациваем серву на угол 90 градусов(Отпираем какой либо механизм: задвижку, поворациваем ключ и т.д.)
            delay(3000); // пауза 3 сек и механизм запирается.
        }
        servo.write(0);  // устанавливаем серву в закрытое сосотояние
    }
}
void addKeyTelegram() {
    // Поиск новой метки
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }
    // Выбор метки
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }
    uidDec = 0;
    // Выдача серийного номера метки.
    for (byte i = 0; i < mfrc522.uid.size; i++)
    {
        uidDecTemp = mfrc522.uid.uidByte[i];
        uidDec = uidDec * 256 + uidDecTemp;
    }
    Serial.println("Card UID: ");
    Serial.println(uidDec); // Выводим UID метки в консоль.
    for (int i = 0; i < 10; i++) {
        if (key[i].keyRFID == 0) {
            bot.sendMessage(CHAT_ID, "Enter the name of the owner of the key");
            for (;;) {
                message m = bot.getUpdates();
                if (m.text.equals("") == false) {
                    key[i].keyRFID = uidDec;
                    key[i].openName = m.text;
                    break;
                }
            }
            break;
            bot.sendMessage(CHAT_ID, "Successfully added");
        }
    }
}
void formatRFID() {
    for (int i = 0; i < 10; i++) {
        key[i].keyRFID = 0;
        key[i].openName = "";
    }
}
void formatSPIFFS() {
    if (SPIFFS.format()) {
        Serial.println("File System Formated");
        client.publish("file", "File System Formated");
    }
    else {
        Serial.println("File System Formatting Error");
    }
}
void printFileRFID() {
    File f = SPIFFS.open(filenameRFID, "r");
    if (!f) {
        Serial.println("file open failed");
    }
    else {
        String data = f.readString();
        client.publish("file", data);
        f.close();  //Close file
    }
    delay(1000);
}
void autoLed() {
    if (analogRead(pinLight) > 800) {
        if (digitalRead(pinHC_SR501) == HIGH) {
            digitalWrite(pinLed, HIGH);
        }
        else {
            digitalWrite(pinLed, LOW);
        }
        delay(100);
    }
    delay(10);
}
void infoMovement() {
    String value1 = " The movement is not seen ";
    String value2 = " The movement is seen ";
    if (millis() - time_info > 10000) {
        pinHC_SR501_Value = digitalRead(pinHC_SR501);
        if (pinHC_SR501_Value == LOW) {
            client.publish("test/movement", value1);
            Serial.println(value1);
        }
        else {
            client.publish("test/movement", value2);
            Serial.println(value2);
        }
        time_info = millis();
    }
    delay(10);
}
void infoSignaling() {
    //if (WiFi.status() == WL_CONNECTED) {
    pinHC_SR501_Value = digitalRead(pinHC_SR501);
    if (pinHC_SR501_Value == HIGH) {
        for (;;) {
            message m = bot.getUpdates();
            if (m.text.equals("offsignaling") == true) {
                break;
            }
            else {
                bot.sendMessage(CHAT_ID, "The movement is seen");
                delay(1000);
            }
        }
        choose = "";
        signaling_on_off = "";
    }
    else if (millis() - time_blink > 5000) {
        digitalWrite(pinLed, HIGH);
        delay(100);
        digitalWrite(pinLed, LOW);
        time_blink = millis();
    }
    delay(10);
    // }
}
void timeSynch(int zone) {
    if (WiFi.status() == WL_CONNECTED) {
        configTime(zone * 3600, 0, "pool.ntp.org", "ru.pool.ntp.org");
        int i = 0;
        Serial.println("\nWaiting for time");
        while (!time(nullptr) && i < 10) {
            Serial.print(".");
            i++;
            delay(1000);
        }
        Serial.println("");
        Serial.println("ITime Ready!");
        Serial.println(GetTime());
        Serial.println(GetDate());
    }
}

String GetTime() {
    time_t now = time(nullptr);
    String Time = "";
    Time += ctime(&now);
    int i = Time.indexOf(":");
    Time = Time.substring(i - 2, i + 6);
    return Time;
}

String GetDate() {
    time_t now = time(nullptr);
    String Data = "";
    Data += ctime(&now);
    int i = Data.lastIndexOf(" ");
    String Time = Data.substring(i - 8, i + 1);
    Data.replace(Time, "");
    return Data;
}
