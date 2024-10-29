#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include "Arduino.h"

// Konfiguracja sieci WiFi
const char* ssid = "siec99x";
const char* password = "password"; 

using namespace websockets;
WebsocketsClient client;

// Piny sterujące silnikami
int PWMA = 5;  // Sterowanie prędkością silnika prawego (PWM)
int PWMB = 4;  // Sterowanie prędkością silnika lewego (PWM)
int DA = 0;    // Kierunek silnika prawego
int DB = 2;    // Kierunek silnika lewego

// Aktualny poziom prędkości (1-5)
int speedLevel = 255; // Domyślnie maksymalna prędkość (poziom 5)

String serverIP = ""; // Zmienna przechowująca znaleziony adres IP serwera

// Flagi do sterowania ruchem
bool moveForwardFlag = false;
bool moveBackwardFlag = false;
bool turnLeftFlag = false;
bool turnRightFlag = false;

bool findServerIP() {
    // Przeszukiwanie adresów IP od 192.168.55.100 do 192.168.55.200
    for (int i = 100; i <= 200; i++) {
        String ip = "192.168.55." + String(i);
        Serial.print(F("Sprawdzam: "));
        Serial.println(ip);

        if (client.connect(ip.c_str(), 8000, "/check-ip")) {
            serverIP = ip;
            Serial.println(F("Połączenie nawiązane z: ") + serverIP); 
            client.close(); 
            return true; 
        } else {
            Serial.println(F("Brak odpowiedzi od: ") + ip); 
        }
        delay(10);
    }

    // Przeszukiwanie IP od 192.168.55.0 do 192.168.55.99
    for (int i = 0; i < 100; i++) {
        String ip = "192.168.55." + String(i);
        Serial.print(F("Sprawdzam: "));
        Serial.println(ip);

        if (client.connect(ip.c_str(), 8000, "/check-ip")) {
            serverIP = ip;
            Serial.println(F("Połączenie nawiązane z: ") + serverIP); 
            client.close(); 
            return true; 
        } else {
            Serial.println(F("Brak odpowiedzi od: ") + ip); 
        }
        delay(10);
    }

    // Przeszukiwanie IP od 192.168.55.201 do 192.168.55.254
    for (int i = 201; i <= 254; i++) {
        String ip = "192.168.55." + String(i);
        Serial.print(F("Sprawdzam: "));
        Serial.println(ip);

        if (client.connect(ip.c_str(), 8000, "/check-ip")) {
            serverIP = ip;
            Serial.println(F("Połączenie nawiązane z: ") + serverIP); 
            client.close(); 
            return true; 
        } else {
            Serial.println(F("Brak odpowiedzi od: ") + ip); 
        }
        delay(10);
    }

    return false; 
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);

    // Konfiguracja pinów dla silników
    pinMode(PWMA, OUTPUT);
    pinMode(PWMB, OUTPUT);
    pinMode(DA, OUTPUT);
    pinMode(DB, OUTPUT);

    // Inicjalizacja silników do zatrzymania
    stopMotors();

    // Połączenie z WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println(F("WiFi połączone"));

    // Znajdowanie IP serwera
    if (!findServerIP()) {
        Serial.println(F("Nie znaleziono serwera. Sprawdz IP serwera."));
        return;
    }

    // Inicjalizacja połączenia WebSocket
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    while (!client.connect(serverIP.c_str(), 8000, "/driver")) {
        delay(500);
    }
}

void loop() {
    client.poll(); // Obsługa zdarzeń WebSocket

    // Obsługa ruchu na podstawie flag ustawionych w funkcji onMessageCallback
    if (moveForwardFlag) {
        forward();
        moveForwardFlag = false;
    }
    if (moveBackwardFlag) {
        backward();
        moveBackwardFlag = false;
    }
    if (turnLeftFlag) {
        turnLeft();
        turnLeftFlag = false;
    }
    if (turnRightFlag) {
        turnRight();
        turnRightFlag = false;
    }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    switch (event) {
        case WebsocketsEvent::ConnectionOpened:
            Serial.println(F("Connection Opened"));
            break;
        case WebsocketsEvent::ConnectionClosed:
            Serial.println(F("Connection Closed"));
            delay(100);
            ESP.restart();
            break;
        case WebsocketsEvent::GotPing:
            Serial.println(F("Got a Ping!"));
            break;
        case WebsocketsEvent::GotPong:
            Serial.println(F("Got a Pong!"));
            break;
    }
}

void onMessageCallback(WebsocketsMessage message) {
    Serial.println(F("Odebrano wiadomość:"));
    String msg = message.data().substring(0, 50); // Ograniczenie długości wiadomości
    Serial.println(F("Odebrano wiadomość: ") + msg);

    if (msg.startsWith("speed:")) {
        int newSpeed = msg.substring(6).toInt();
        if (newSpeed >= 1 && newSpeed <= 5) {
            speedLevel = map(newSpeed, 1, 5, 51, 255);
            Serial.println(F("Ustawiono prędkość na: ") + String(speedLevel));
        } else {
            Serial.println(F("Nieprawidłowa wartość prędkości. Powinna być w zakresie 1-5."));
        }
    } else if (msg.equals("forward")) {
        moveForwardFlag = true;
    } else if (msg.equals("backward")) {
        moveBackwardFlag = true;
    } else if (msg.equals("left")) {
        turnLeftFlag = true;
    } else if (msg.equals("right")) {
        turnRightFlag = true;
    } else if (msg.startsWith("run_for:")) {
        int runTime = msg.substring(8).toInt(); // Pobierz czas działania w milisekundach
        if (runTime > 0) {
            runMotors(runTime);
        } else {
            Serial.println(F("Czas działania musi być większy od zera."));
        }
    }
}

void runMotors(int runTime) {
    Serial.println(F("Uruchamianie silników..."));
    digitalWrite(DA, LOW); // Kierunek silnika prawego
    digitalWrite(DB, LOW); // Kierunek silnika lewego
    analogWrite(PWMA, speedLevel);
    analogWrite(PWMB, speedLevel);
    
    delay(runTime); // Działaj przez określony czas (w ms)
    stopMotors();
}

void forward() {
    Serial.println(F("Moving forward..."));
    digitalWrite(DA, LOW);
    digitalWrite(DB, LOW);
    analogWrite(PWMA, speedLevel);
    analogWrite(PWMB, speedLevel);
    delay(40);
    stopMotors();
}

void backward() {
    Serial.println(F("Moving backward..."));
    digitalWrite(DA, HIGH);
    digitalWrite(DB, HIGH);
    analogWrite(PWMA, speedLevel);
    analogWrite(PWMB, speedLevel);
    delay(40);
    stopMotors();
}

void turnLeft() {
    Serial.println(F("Turning left..."));
    digitalWrite(DA, LOW);
    digitalWrite(DB, HIGH);
    analogWrite(PWMA, speedLevel);
    analogWrite(PWMB, speedLevel);
    delay(40);
    stopMotors();
}

void turnRight() {
    Serial.println(F("Turning right..."));
    digitalWrite(DA, HIGH);
    digitalWrite(DB, LOW);
    analogWrite(PWMA, speedLevel);
    analogWrite(PWMB, speedLevel);
    delay(40);
    stopMotors();
}

void stopMotors() {
    Serial.println(F("Stopping motors..."));
    digitalWrite(DA, LOW);
    digitalWrite(DB, LOW);
    analogWrite(PWMA, LOW);
    analogWrite(PWMB, LOW);
}
