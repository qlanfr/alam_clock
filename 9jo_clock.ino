#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <RTClib.h>
#include <TM1637Display.h>
//202008059_B_노용희
// DHT11 센서 설정
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// RTC 설정
RTC_DS3231 rtc;

// LCD 설정 (I2C 주소 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// TM1637 설정
#define CLK 3
#define DIO 4
TM1637Display display(CLK, DIO);

// 버튼 및 부저 핀 설정
const int button1Pin = 6; // 설정 버튼
const int button2Pin = 7; // 알람/부저 버튼
const int button3Pin = 8; // LCD 밝기 조절 및 온도 단위 변경 버튼
const int buzzerPin = 9;

bool alarmOn = false; // 알람 상태
bool alarmTriggered = false; // 알람 작동 상태
DateTime alarmTime; // 알람 시간
unsigned long button2PressTime = 0; // 버튼2 누른 시간
bool button2LongPress = false; // 버튼2 장기 누름 상태
unsigned long button3PressTime = 0; // 버튼3 누른 시간
bool button3LongPress = false; // 버튼3 장기 누름 상태
bool celsius = true; // 온도 단위 (기본: 섭씨)
int lcdBrightness = 5; // LCD 밝기 (기본값: 5, 범위: 0-10)
bool alarmDays[7] = {false, false, false, false, false, false, false}; // 일~토 알람 설정

void setup() {
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);
  pinMode(button3Pin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  lcd.begin();
  lcd.backlight();
  dht.begin();
  if (!rtc.begin()) {
    lcd.print("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  alarmTime = rtc.now(); // 기본 알람 시간 설정

  display.setBrightness(0x0f); // 밝기 설정 

  // 초기 화면 표시
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Made by nyh");
  lcd.setCursor(0, 1);
  lcd.print("202008059");
  delay(3000);
  lcd.clear();
}

void loop() {
  DateTime now = rtc.now();

  // 모든 버튼을 동시에 누르면 홈으로 나가기
  if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) {
    lcd.clear();
    lcd.print("Returning Home");
    delay(2000);
    lcd.clear();
  }

  if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW) {
    setDayOfWeek();
  }

  if (digitalRead(button1Pin) == LOW) {
    enterSettings();
  }

  if (digitalRead(button2Pin) == LOW) {
    if (!button2LongPress) {
      button2PressTime = millis();
      button2LongPress = true;
    }

    if (millis() - button2PressTime > 2000) { // 2초 이상 누르면 알람 on/off 토글
      alarmOn = !alarmOn;
      lcd.clear();
      lcd.print(alarmOn ? "Alarm On" : "Alarm Off");
      delay(2000);
      lcd.clear();
      button2LongPress = false;
    }
  } else {
    if (button2LongPress && millis() - button2PressTime < 2000) {
      if (alarmTriggered) {
        alarmTriggered = false;
        digitalWrite(buzzerPin, LOW); // 알람 끄기
        lcd.clear();
        lcd.print("Alarm Stopped");
        delay(2000);
        lcd.clear();
      }
      button2LongPress = false;
    }
  }

  if (digitalRead(button3Pin) == LOW) {
    if (!button3LongPress) {
      button3PressTime = millis();
      button3LongPress = true;
    }

    if (millis() - button3PressTime > 2000) { // 2초 이상 누르면 LCD 밝기 조절 메뉴로 진입
      adjustLCDBrightness();
      button3LongPress = false;
    }
  } else {
    if (button3LongPress && millis() - button3PressTime < 2000) {
      celsius = !celsius;
      lcd.clear();
      lcd.print(celsius ? "Celsius" : "Fahrenheit");
      delay(2000);
      lcd.clear();
      button3LongPress = false;
    }
  }

  showDateTime(now);
  showTempHumidity();

  checkAlarm(now);
}

// 현재 날짜 및 시간을 LCD와 FND에 표시
void showDateTime(DateTime now) {
  // FND
  int displayTime = now.hour() * 100 + now.minute();
  display.showNumberDecEx(displayTime, 0b01000000, true); 

  // LCD에 날짜, 요일 표시
  lcd.setCursor(0, 0);
  lcd.print(now.year(), DEC);
  lcd.print('/');
  lcd.print(now.month(), DEC);
  lcd.print('/');
  lcd.print(now.day(), DEC);

  lcd.print(" ");
  lcd.print(alarmOn ? "@" : " ");
  lcd.setCursor(13, 0);
  switch (now.dayOfTheWeek()) {
    case 0: lcd.print("Sun "); break;
    case 1: lcd.print("Mon "); break;
    case 2: lcd.print("Tue "); break;
    case 3: lcd.print("Wed "); break;
    case 4: lcd.print("Thu "); break;
    case 5: lcd.print("Fri "); break;
    case 6: lcd.print("Sat "); break;
  }
}

// 현재 온도 및 습도를 LCD에 표시
void showTempHumidity() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!celsius) {
    t = t * 9.0 / 5.0 + 32.0; // 섭씨에서 화씨로 변환
  }
  
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(t, 1);
  lcd.print(celsius ? "C " : "F ");
  lcd.print("H:");
  lcd.print(h, 1);
}

// 설정 메뉴 진입 함수
void enterSettings() {
  int settingIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1:Clock Adj");
  lcd.setCursor(0, 1);
  lcd.print("2:Alarm Adj");
  delay(2000);

  while (true) {
    if (digitalRead(button1Pin) == LOW) { // 설정 선택
      settingIndex++;
      if (settingIndex > 2) settingIndex = 0;
      delay(200);
    }

    if (digitalRead(button2Pin) == LOW) { // 설정 진입
      if (settingIndex == 0) {
        adjustClock();
      } else if (settingIndex == 1) {
        adjustAlarm();
      } else {
        break;
      }
    }

    if (digitalRead(button3Pin) == LOW) { // 홈으로 돌아가기
      break;
    }

    lcd.setCursor(0, 0);
    if (settingIndex == 0) lcd.print("1:Clock Adj");
    if (settingIndex == 1) lcd.print("2:Alarm Adj");
    if (settingIndex == 2) lcd.print("3:Home");
    lcd.setCursor(0, 1);
    lcd.print("Select & Enter");
    delay(200);
  }

  lcd.clear();
}

// 시계 설정 함수
void adjustClock() {
  int clockIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1:Date Adj");
  lcd.setCursor(0, 1);
  lcd.print("2:Time Adj");
  delay(2000);

  while (true) {
    if (digitalRead(button1Pin) == LOW) { // 설정 선택
      clockIndex++;
      if (clockIndex > 2) clockIndex = 0;
      delay(200);
    }

    if (digitalRead(button2Pin) == LOW) { // 설정 진입
      if (clockIndex == 0) {
        setDateTime();
      } else if (clockIndex == 1) {
        setTime();
      } else {
        break;
      }
    }

    if (digitalRead(button3Pin) == LOW) { // 홈으로 돌아가기
      break;
    }

    lcd.setCursor(0, 0);
    if (clockIndex == 0) lcd.print("1:Date Adj");
    if (clockIndex == 1) lcd.print("2:Time Adj");
    if (clockIndex == 2) lcd.print("3:Home ");
    lcd.setCursor(0, 1);
    lcd.print("Select & Enter");
    delay(200);
  }

  lcd.clear();
}

// 날짜 설정 함수
void setDateTime() {
  int values[3] = {2023, 1, 1}; // 초기값: 연, 월, 일
  int index = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Date");

  delay(2000); 

  // 날짜 설정
  while (index < 3) {
    lcd.setCursor(0, 1);
    lcd.print(values[0]); lcd.print('/');
    lcd.print(values[1]); lcd.print('/');
    lcd.print(values[2]);

    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      index++;
      delay(200);
    }
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      values[index]++;
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      values[index]--;
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }

    if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) { // 모든 버튼을 동시에 누르면 홈으로 나가기
      return;
    }
  }

  rtc.adjust(DateTime(values[0], values[1], values[2], rtc.now().hour(), rtc.now().minute(), rtc.now().second()));
  lcd.clear();
}

// 시간 설정 함수
void setTime() {
  int values[2] = {0, 0}; // 초기값: 시, 분
  int index = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Time");

  delay(2000); 

  // 시간 설정
  while (index < 2) {
    lcd.setCursor(0, 1);
    lcd.print(values[0]); lcd.print(':');
    lcd.print(values[1]);

    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      index++;
      delay(200);
    }
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      values[index]++;
      if (index == 0 && values[index] >= 24) values[index] = 0; // 시 최대값 24
      if (index == 1 && values[index] >= 60) values[index] = 0; // 분 최대값 60
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      values[index]--;
      if (index == 0 && values[index] < 0) values[index] = 23; // 시 최소값 0
      if (index == 1 && values[index] < 0) values[index] = 59; // 분 최소값 0
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }

    if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) { // 모든 버튼을 동시에 누르면 홈으로 나가기
      return;
    }
  }

  rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), values[0], values[1], rtc.now().second()));
  lcd.clear();
}

// 요일 설정 함수
void setDayOfWeek() {
  int dayOfWeek = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Day of Week");

  delay(2000); 

  while (true) {
    lcd.setCursor(0, 1);
    switch (dayOfWeek) {
      case 0: lcd.print("Sun "); break;
      case 1: lcd.print("Mon "); break;
      case 2: lcd.print("Tue "); break;
      case 3: lcd.print("Wed "); break;
      case 4: lcd.print("Thu "); break;
      case 5: lcd.print("Fri "); break;
      case 6: lcd.print("Sat "); break;
    }

    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      DateTime now = rtc.now();
      rtc.adjust(DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second()));
      rtc.writeSqwPinMode(DS3231_OFF); // 요일 설정을 위해 추가
      rtc.writeSqwPinMode(DS3231_OFF); // 요일 설정을 위해 추가
      break;
    }
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      dayOfWeek++;
      if (dayOfWeek > 6) dayOfWeek = 0; // 요일 최대값 6
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      dayOfWeek--;
      if (dayOfWeek < 0) dayOfWeek = 6; // 요일 최소값 0
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }

    if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) { // 모든 버튼을 동시에 누르면 홈으로 나가기
      break;
    }
  }

  lcd.clear();
}

// 알람 설정 함수
void adjustAlarm() {
  int alarmIndex = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("1:Time Adj");
  lcd.setCursor(0, 1);
  lcd.print("2:Day Adj");
  delay(2000);

  while (true) {
    if (digitalRead(button1Pin) == LOW) { // 설정 선택
      alarmIndex++;
      if (alarmIndex > 2) alarmIndex = 0;
      delay(200);
    }

    if (digitalRead(button2Pin) == LOW) { // 설정 진입
      if (alarmIndex == 0) {
        setAlarmTime();
      } else if (alarmIndex == 1) {
        setAlarmDays();
      } else {
        break;
      }
    }

    if (digitalRead(button3Pin) == LOW) { // 홈으로 돌아가기
      break;
    }

    lcd.setCursor(0, 0);
    if (alarmIndex == 0) lcd.print("1:Time Adj");
    if (alarmIndex == 1) lcd.print("2:Day Adj");
    if (alarmIndex == 2) lcd.print("3:Home ");
    lcd.setCursor(0, 1);
    lcd.print("Select & Enter");
    delay(200);
  }

  lcd.clear();
}

// 알람 시간 설정 함수
void setAlarmTime() {
  int values[2] = {0, 0}; // 초기값: 시, 분
  int index = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Alarm Time");

  delay(2000); // 설정 시작 전 2초 대기

  // 알람 시간 설정
  while (index < 2) {
    lcd.setCursor(0, 1);
    lcd.print(values[0]); lcd.print(':');
    lcd.print(values[1]);

    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      index++;
      delay(200);
    }
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      values[index]++;
      if (index == 0 && values[index] >= 24) values[index] = 0; // 시 최대값 24
      if (index == 1 && values[index] >= 60) values[index] = 0; // 분 최대값 60
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      values[index]--;
      if (index == 0 && values[index] < 0) values[index] = 23; // 시 최소값 0
      if (index == 1 && values[index] < 0) values[index] = 59; // 분 최소값 0
      tone(buzzerPin, 1000, 100); // 소리출력
      delay(200);
    }

    if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) { // 모든 버튼을 동시에 누르면 홈으로 나가기
      return;
    }
  }

  alarmTime = DateTime(2000, 1, 1, values[0], values[1]);
  alarmOn = true;
  lcd.clear();
}

// 알람 요일 설정 함수
void setAlarmDays() {
  int dayIndex = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Alarm Days");

  delay(2000); 

  while (true) {
    lcd.setCursor(0, 1);
    switch (dayIndex) {
      case 0: lcd.print("Sun "); break;
      case 1: lcd.print("Mon "); break;
      case 2: lcd.print("Tue "); break;
      case 3: lcd.print("Wed "); break;
      case 4: lcd.print("Thu "); break;
      case 5: lcd.print("Fri "); break;
      case 6: lcd.print("Sat "); break;
    }
    lcd.print(alarmDays[dayIndex] ? "On " : "Off");

    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      alarmDays[dayIndex] = !alarmDays[dayIndex];
      delay(200);
    }
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      dayIndex++;
      if (dayIndex > 6) dayIndex = 0; // 요일 최대값 6
      tone(buzzerPin, 1000, 100); // 
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      dayIndex--;
      if (dayIndex < 0) dayIndex = 6; // 요일 최소값 0
      tone(buzzerPin, 1000, 100); 
      delay(200);
    }

    if (digitalRead(button1Pin) == LOW && digitalRead(button2Pin) == LOW && digitalRead(button3Pin) == LOW) { // 모든 버튼을 동시에 누르면 홈으로 나가기
      break;
    }
  }

  lcd.clear();
}

// 알람 작동 확인 함수
void checkAlarm(DateTime now) {
  bool alarmDay = false;
  switch (now.dayOfTheWeek()) {
    case 0: alarmDay = alarmDays[0]; break;
    case 1: alarmDay = alarmDays[1]; break;
    case 2: alarmDay = alarmDays[2]; break;
    case 3: alarmDay = alarmDays[3]; break;
    case 4: alarmDay = alarmDays[4]; break;
    case 5: alarmDay = alarmDays[5]; break;
    case 6: alarmDay = alarmDays[6]; break;
  }

  if (alarmOn && alarmDay && now.hour() == alarmTime.hour() && now.minute() == alarmTime.minute() && !alarmTriggered) {
    digitalWrite(buzzerPin, HIGH);
    alarmTriggered = true;
  }
}

// LCD 밝기 조절 함수
void adjustLCDBrightness() {
  int brightnessIndex = lcdBrightness;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Light Control");
  lcd.setCursor(0, 1);
  lcd.print("Brightness: ");
  lcd.print(brightnessIndex);

  while (true) {
    if (digitalRead(button2Pin) == LOW) { // 업 버튼
      brightnessIndex = 1;
      tone(buzzerPin, 1000, 100); 
      delay(200);
    }
    if (digitalRead(button3Pin) == LOW) { // 다운 버튼
      brightnessIndex = 0;
      tone(buzzerPin, 1000, 100); 
      delay(200);
    }
    if (digitalRead(button1Pin) == LOW) { // 엔터 버튼
      lcdBrightness = brightnessIndex;
      lcd.setBacklight(lcdBrightness == 1 ? HIGH : LOW); // LCD 밝기 조절
      lcd.clear();
      lcd.print("Brightness Saved");
      delay(2000);
      lcd.clear();
      break;
    }

    lcd.setCursor(0, 1);
    lcd.print("Brightness: ");
    lcd.print(brightnessIndex);
    delay(200);
  }
}
