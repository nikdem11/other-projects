#include<OneWire.h>
#include<DallasTemperature.h>
#include<LiquidCrystal.h>
#include<ezButton.h>
#include<WiFi.h>
#include<HTTPClient.h>
#include<Arduino_JSON.h>
#include<WiFiClientSecure.h>


#define DEBOUNCE_TIME 40  // ustawienie wartości Debounce Time'u 
#define SENSOR_PIN 26  // pin czujnika temperatury


ezButton button(33); // (pin przycisku)
int buttonCnt = 0;  // licznik, wykorzystywany do zmiany widoku 

OneWire oneWire(SENSOR_PIN); 
DallasTemperature DS18B20(&oneWire);

// wyświetlacz - piny [lcd(rs, rw, enable, d4, d5, d6, d7)] 
LiquidCrystal lcd(16, 21, 18, 19, 22, 23); 

// ssid i hasło do sieci, do której chcemy się podłączyć
const char* ssid = "Galaxy M22832F";
const char* password = "XXXXXXXX";

// flaga, do sprawdzania konieczności wyświetlania tekstu o łączeniu z OWM
bool introduce = true;


void setup() {
  // put your setup code here, to run once:
  
  DS18B20.begin(); // inicjalizacja czujnika

  lcd.begin(16, 2);  // inicjalizacja wyświetlacza (columns, rows)

  Serial.begin(115200);  // inicjalizacja Serial

  button.setDebounceTime(DEBOUNCE_TIME);  // ustawienie Debounce Time'u (czas trwania drgania styków)

  WiFi.begin(ssid, password);  // inicjalizacja połączenia z siecią WiFi

  lcd.print("WiFi connecting");
  
  delay(5000);
  
  // czas na połączenie - 5s
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.print("Failed to connect");
  } 
  else { // jeśli połączenie zakończy się sukcem
    lcd.clear();
    lcd.print("WiFi connected");
    delay(2000);  
  }
}


unsigned long previousTempMillis = 0;  // Przechowuje ostatni czas, kiedy temperatura została wyświetlona
const long tempInterval = 2000;      // Przerwa w wyświetlaniu temperatury (w millisekundach)

// Funkcja odpowiadająca za wyświetlanie bieżącej temperatury, pobieranej przez czujnik. 
void showTemp() {
  unsigned long currentMillis = millis();  
  
  // Sprawdzenie, czy czas ostatniego wyświetlenia jest większy od ustalonego interwału
  if (currentMillis - previousTempMillis >= tempInterval) {
    // Zapisanie ostatniego czasu wyświetlania temperatury
    previousTempMillis = currentMillis;

    float temp; // zmienna do zapisu bieżącej temperatury

    // Pobranie i wyświetlenie bieżącej temperatury
    DS18B20.requestTemperatures();
    temp = DS18B20.getTempCByIndex(0);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temp);
    lcd.print((char)223);
    lcd.print("C");
  }
}

// Funkcja GET, zwracająca String z objektem JSON 
String httpGETRequest(const char* serverName) {
  
  // Sprawdź, czy jesteś połączony z Wi-Fi przed wysłaniem żądania
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.println("WiFi is not connected!");
    return "{}";
  }

  WiFiClient client;
  HTTPClient http;

  // Rozpocznij połączenie z podanym URL
  http.begin(client, serverName);
  
  // Wysłanie żądania HTTP GET
  int httpResponseCode = http.GET();
  
  String payload = "{}";  // Domyślna wartość payload, jeśli nie uda się pobrać danych
  
  // Sprawdź, czy żądanie zostało wykonane pomyślnie
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();  // Pobierz treść odpowiedzi
  } else {
    // W przypadku błędu wydrukuj kod błędu oraz jego opis
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode));
  }
  
  // Zwolnienie zasobów
  http.end();

  return payload;
}


unsigned long lastForecastMillis = 0;
unsigned long lastMessageMillis = 0;
const long forecastInterval = 10000;  // interwał między pobieraniem informacji o pogodzie - 10s
const long messageInterval = 5000;  // interwał między wiadomościami - 5s
bool displayFirstMessage = true;  // Flaga, do przechodzenia między "dolnymi" informacjami

// Funkcja odpowiadająca za wyświetlanie bieżącej temperatury, prędkości wiatru
// indeksu jakości powietrza oraz prognozy pogody dla przekazywanej miejscowości.  
void showForecast(String city, String lat, String lon) {
  unsigned long currentMillis = millis();

  String jsonBuffer;  // bufor w którym będą zapisywane dane pobierane z GET requesta

  if (introduce) {  // wyświetlenie widoku połączenia z API
  displayFirstMessage = true;  // włączenie flagi informującej o wyświetleniu informacji o łączeniu
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Lacze z OWM...");
    lcd.setCursor(0,1);
    lcd.print("Miejsce: " + city);
    introduce = false;
  }

  // Sprawdź, czy upłynęło wystarczająco dużo czasu od ostatniego wywołania
  if (currentMillis - lastForecastMillis >= forecastInterval) {
    lastForecastMillis = currentMillis;  // Aktualizuj czas ostatniego wywołania
  
    // pobieranie informacji o bieżącej pogodzie
    String serverPath1 = "http://api.openweathermap.org/data/2.5/weather?lat=" + lat + "&lon=" + lon + "&appid=f7e7fab922e3fb87b10f0c8258f30061&units=metric";
    jsonBuffer = httpGETRequest(serverPath1.c_str());
    JSONVar myObj1 = JSON.parse(jsonBuffer);

    // pobieranie informacji o bieżącej jakości powietrza
    String serverPath2 = "http://api.openweathermap.org/data/2.5/air_pollution?lat=" + lat + "&lon=" + lon + "&appid=f7e7fab922e3fb87b10f0c8258f30061";
    jsonBuffer = httpGETRequest(serverPath2.c_str());
    JSONVar myObj2 = JSON.parse(jsonBuffer);

    // pobieranie informacji o prognozie pogody (3 dni do przodu, wartości co 3h)
    String serverPath3 = "http://api.openweathermap.org/data/2.5/forecast?lat=" + lat + "&lon=" + lon + "&appid=f7e7fab922e3fb87b10f0c8258f30061&units=metric";
    jsonBuffer = httpGETRequest(serverPath3.c_str());
    JSONVar myObj3 = JSON.parse(jsonBuffer);
    
    if (currentMillis - lastMessageMillis >= messageInterval) {
      lastMessageMillis = currentMillis;  // Aktualizuj czas ostatniego wyświetlenia wiadomości

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(city); // wyświetlenie lokalizacji 
      lcd.print("|");
      int tempInt = int(myObj1["main"]["temp"]);
      lcd.print(tempInt); // wyświetlenie bieżącej temperatury (stopnie Celsjusza)
      lcd.print((char)223);
      lcd.print("C");
      lcd.print("|");
      int windInt = int(myObj1["wind"]["speed"]);
      lcd.print(windInt); // wyświetlenie bieżącej prędkości wiatru (m/s)
      lcd.print("ms|");
      lcd.print(myObj2["list"][0]["main"]["aqi"]); // wyświetlenie bieżącego indeksu jakości powietrza (1->dobrze, 5->bardzo źle)
      
      String message = {};  // wyświetlanie 2 pomiarów najbliższego i +6h wraz z pierwszą literą rodzaju pogody (np. Rain->R)
      for (int i = 0; i < 3; i+=2) { 
          String dt_txt = String((const char*)myObj3["list"][i]["dt_txt"]); // MOŻE? = JSON.stringify(myObj3["list"][i]["dt_txt"]);
          String time = dt_txt.substring(11, 13);
          message += time + " "; 
          int tempInt = int(myObj3["list"][i]["main"]["temp"]);
          message += String(tempInt) + " ";
          String weather = String((const char*)myObj3["list"][i]["weather"][0]["main"]);
          String weatherone = weather.substring(0, 1);
          message += weatherone + "|";

      }
      
      String message2 = {};  // wyświetlenie pomiarów +12h, +18h i +24h
      for (int i = 4; i < 9; i+=2) {
          String dt_txt = String((const char*)myObj3["list"][i]["dt_txt"]);
          String time = dt_txt.substring(11, 13);
          message2 += time + " "; 
          int tempInt = int(myObj3["list"][i]["main"]["temp"]);
          message2 += String(tempInt) + " ";
      }

    // Wybór wiadomości do wyświetlenia
      if (displayFirstMessage) { 
        lcd.setCursor(0,1);
        lcd.print(message);
      } 
      else {  
        lcd.setCursor(0,1);
        lcd.print(message2);
      }

      // Przełączqnie flagi na wyświetlenie drugiej wiadomości przy następnym wywołaniu
      displayFirstMessage = !displayFirstMessage;
    }
  }  
}


void loop() {
  // put your main code here, to run repeatedly:
  button.loop(); 
  int prevButtonCnt = buttonCnt;
    if (button.isPressed()) {
        buttonCnt = (buttonCnt + 1) % 3; // ograniczenie buttonCnt do 3 wartości (0,1,2)
        if (prevButtonCnt != buttonCnt) {
            introduce = true; // Zmiana flagi wyświetlania tylko gdy zmienia się widok
        }
    }

  switch(buttonCnt) {
    case 0:
      showForecast("Tychy", "50.128250", "18.988600"); 
      break;
    case 1:
      showTemp();
      break;
    case 2:
      showForecast("AEI", "50.288589", "18.677483"); 
      break;
  }
}



