// include Library yang diperlukan
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Time.h>
#include <Bounce2.h>
#include <Buzzer.h>
#include <RTClib.h>
#include <DFRobotDFPlayerMini.h>

// konfigurasi
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define btn1 1
#define btn2 2
#define btn3 3
#define SLEEP_TIMEOUT 30000
#define RTC_SYNC_INTERVAL 3600000
#define DF_RX 20
#define DF_TX 21

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobotDFPlayerMini myDFPlayer;

// wifi credentials
const char* ssid = "the wonder of you";
const char* password = "akhadika akan kurus 2024";

// NTP time sync
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200, 60000);

// time keeping & hardware
ESP32Time rtc;
RTC_DS1307 ds1307;
bool rtcAvailable = false;
bool useHardwareRTC = false;

// stopwatch
unsigned long swStart = 0;
unsigned long swElapsed = 0;
bool swRunning = false;

// alarm
int alarmH = 6, alarmM = 0;
bool alarmActive = false;
bool alarmTriggered = false;

// Transkripsi Full: Dog of Flanders (Yoake no Michi)
int alarmNotes[] = {
  NOTE_E, NOTE_D, NOTE_C4, NOTE_F, NOTE_E, NOTE_D, NOTE_G, NOTE_A, NOTE_B, NOTE_C5, NOTE_E, NOTE_F, NOTE_E, NOTE_E, NOTE_D, NOTE_E, NOTE_D, NOTE_C4, NOTE_F, NOTE_E, NOTE_D, NOTE_G, NOTE_A, NOTE_B, NOTE_C5, NOTE_E, NOTE_D, NOTE_C4, NOTE_E, NOTE_D, NOTE_C4, NOTE_F, NOTE_E, NOTE_D, NOTE_G, NOTE_A, NOTE_B, NOTE_C5, NOTE_E, NOTE_F, NOTE_E, NOTE_E, NOTE_D, NOTE_E, NOTE_D, NOTE_C4, NOTE_F, NOTE_E, NOTE_D, NOTE_G, NOTE_A, NOTE_B, NOTE_C5, NOTE_E, NOTE_D, NOTE_C4, NOTE_E, NOTE_E, NOTE_E, NOTE_E, NOTE_E, NOTE_F, NOTE_G, NOTE_G, NOTE_F, NOTE_D, NOTE_D, NOTE_D, NOTE_D, NOTE_D, NOTE_D, NOTE_E, NOTE_F, NOTE_F, NOTE_E, NOTE_E, NOTE_C4, NOTE_C4, NOTE_A, NOTE_A, NOTE_B, NOTE_C5, NOTE_B, NOTE_A, NOTE_A, NOTE_G, NOTE_G, NOTE_F, NOTE_G, NOTE_A, NOTE_G, NOTE_F, NOTE_D, NOTE_B, NOTE_D, NOTE_A, NOTE_G, NOTE_G, NOTE_G, NOTE_A, NOTE_A, NOTE_A, NOTE_B, NOTE_C4, NOTE_D, NOTE_C4, NOTE_C4, NOTE_D, NOTE_E, NOTE_F 
};

// Durasi presisi (dalam milidetik) yang disesuaikan dengan tempo aslinya
int alarmDurations[] = {
  80, 80, 120, 80, 80, 120, 80, 50, 20, 110, 80, 80, 80, 80, 80, 80, 80, 120, 80, 80, 120, 80, 50, 20, 110, 80, 120, 120, 80, 80, 120, 80, 80, 120, 80, 50, 20, 110, 80, 80, 80, 80, 80, 80, 80, 120, 80, 80, 120, 80, 50, 20, 110, 80, 120, 120, 80, 80, 80, 80, 80, 100, 80, 80, 100, 150, 80, 80, 80, 80, 80, 100, 80, 80, 100, 80, 120, 80, 100, 120, 80, 80, 100, 80, 80, 100, 80, 100, 80, 80, 80, 80, 80, 80, 80, 800, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80, 80
};

// variabel musik
int musicVolume = 9;
bool isPlaying = false;
bool isLooping = false;
bool isRandom = false;

// battery monitoring
int adcValue = analogRead(0);
float voltage = (adcValue * 3.3 / 4095.0) * 3.0;
int pct = map(constrain(voltage, 3.3, 4.2), 3.3, 4.2, 0, 100);

// button 
Bounce modeBtn = Bounce();
Bounce setBtn = Bounce();
Bounce adjustBtn = Bounce();
unsigned long btn1PressTime = 0;
unsigned long btn2PressTime = 0;
unsigned long btn3PressTime = 0;

// state jam
enum Mode {CLOCK_MODE, STOPWATCH_MODE, SET_TIME_MODE,SET_ALARM_MODE, MUSIC_MODE};
Mode currentMode = CLOCK_MODE;
int settingField = 0;
unsigned long lastActive = millis();

Buzzer buzzer(4);

void setup(){
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, DF_RX, DF_TX);

  Wire.begin(8, 9);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Dislay init failed");
    while(true);
  }

  display.clearDisplay();
  display.setTextColor(WHITE);

  Serial.println(F("Initializing DFPlayer..."));
  if(!myDFPlayer.begin(Serial1)) {
    Serial.println(F("DFPlayer error/SD not found"));
  } else {
    myDFPlayer.setTimeOut(500);
    myDFPlayer.outputSetting(true, 31);
    myDFPlayer.enableDAC();
    myDFPlayer.volume(musicVolume);
    myDFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    Serial.println(F("DFPlayer stereo optimized online"));
  }

  deviceStartUp();
}

void deviceStartUp() {
  display.setTextSize(1);
  display.setCursor(20, 10);
  display.print("Heemin's watch");
  display.display();

  modeBtn.attach(btn1, INPUT_PULLUP);
  modeBtn.interval(25);
  setBtn.attach(btn2, INPUT_PULLUP);
  setBtn.interval(25);
  adjustBtn.attach(btn3, INPUT_PULLUP);
  adjustBtn.interval(25);

  if (ds1307.begin()) {
    rtcAvailable = true;
    if (!ds1307.isrunning()) {
      Serial.println("RTC lost power, setting time");
      ds1307.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    useHardwareRTC = true;
  } else {
    Serial.println("Couldn't find RTC");
    rtcAvailable = false;
  }

  buzzer.begin(100);
  int notes[] = {NOTE_A8, NOTE_B8, NOTE_C8, NOTE_D8, NOTE_E8, NOTE_F8, NOTE_G8, NOTE_AS8};
  for(int i=0; i<8; i++) buzzer.sound(notes[i], 80);
  buzzer.end(100);

  display.setCursor(0, 30);
  WiFi.begin(ssid, password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 10) {
    delay(500);
    display.print(".");
    display.display();
    counter++;
  }
  syncTime();

  esp_sleep_enable_gpio_wakeup();
  gpio_wakeup_enable((gpio_num_t)btn1, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)btn2, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)btn3, GPIO_INTR_LOW_LEVEL);
}

void syncTime() {
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
    rtc.setTime(timeClient.getEpochTime());
    if (rtcAvailable) ds1307.adjust(DateTime(timeClient.getEpochTime()));
  } else if (rtcAvailable) {
    DateTime now = ds1307.now();
    rtc.setTime(now.unixtime());
  } else {
    // Fallback to software RTC with default time
    rtc.setTime(0, 0, 0, 1, 1, 2026);
  }
}

DateTime getCurrentTime() {
  if (useHardwareRTC && rtcAvailable) {
    return ds1307.now();
  }
  // Fallback to software RTC
  return DateTime(rtc.getYear(), rtc.getMonth() + 1, rtc.getDay(), 
                 rtc.getHour(true), rtc.getMinute(), rtc.getSecond());
}

void loop() {
  updateInput();
  updateDisplay();

  static unsigned long lastSync = 0;
  if (millis()- lastSync > RTC_SYNC_INTERVAL) {
    syncTime();
    lastSync = millis();
  }

  checkAlarm();
  checkSleepCondition();
}

void checkAlarm() {
  DateTime now = getCurrentTime();
  
  // Cek apakah waktu cocok dan alarm aktif
  if (alarmActive && now.hour() == alarmH && now.minute() == alarmM) {
    if (!alarmTriggered) {
      alarmTriggered = true;
    }
  }

  // Jika sedang terpicu, mainkan melodi secara berulang
  if (alarmTriggered) {
    static int noteIndex = 0;
    static unsigned long lastNoteTime = 0;

    display.ssd1306_command(SSD1306_DISPLAYON);
    lastActive = millis();
    Serial.println("Alarm Triggered: Waking up display...");

    // Mainkan nada satu per satu tanpa memblokir loop (non-blocking)
    if (millis() - lastNoteTime > alarmDurations[noteIndex]) {
      buzzer.sound(alarmNotes[noteIndex], alarmDurations         [noteIndex]);
      lastNoteTime = millis();
      noteIndex++;

      // Reset melodi jika sudah selesai (looping lagu)
      if (noteIndex >= (sizeof(alarmNotes) / sizeof(int))) {
        noteIndex = 0;
      }
    }

    // Matikan alarm jika ada tombol yang ditekan (Tombol 1, 2,atau 3)
    if (modeBtn.rose() || setBtn.rose() || adjustBtn.rose()) {
      alarmTriggered = false;
      alarmActive = false; // Matikan alarm setelah dihentikan
      noteIndex = 0;
      buzzer.end(10);
      Serial.println("Alarm stopped by user");
    }
    lastActive = millis();
  }
}

void updateInput() {
  modeBtn.update();
  setBtn.update();
  adjustBtn.update();

  if (modeBtn.fell()) btn1PressTime = millis();
  if (modeBtn.rose()) {
    unsigned long duration = millis() - btn1PressTime;
    if (currentMode == MUSIC_MODE && duration >= 500) {
      if (isPlaying) { myDFPlayer.disableLoopAll(); myDFPlayer.pause(); isPlaying = false;}
      else { myDFPlayer.enableLoopAll(); myDFPlayer.start(); isPlaying = true;} 
    } else {
      currentMode = (Mode)((currentMode + 1) % 5);
      settingField = 0;
    }
    playButtonSound();
    lastActive = millis();
  }

  if (setBtn.fell()) btn2PressTime = millis();
  if (setBtn.rose()) {
    unsigned long duration = millis() - btn2PressTime;
    if (currentMode == MUSIC_MODE) {
      if (duration > 900) {
        isLooping = !isLooping;
        if(isLooping) myDFPlayer.enableLoop(); else myDFPlayer.disableLoop();
      } else if (duration >500 ) {
        musicVolume = max(0, musicVolume - 2);
        myDFPlayer.volume(musicVolume);
      } else {
        myDFPlayer.previous();
        isPlaying = true;
      }
    } else {
      handleSetButton();
    }
    playButtonSound();
    lastActive = millis();
  }

  if (adjustBtn.fell()) btn3PressTime = millis();
  if (adjustBtn.rose()) {
    unsigned long duration = millis() - btn3PressTime;
    if (currentMode == MUSIC_MODE) {
      if (duration > 900) {
        isRandom = !isRandom;
        if(isRandom) myDFPlayer.randomAll(); else myDFPlayer.stop();
      } else if (duration > 500) {
        musicVolume = min(30, musicVolume + 2);
        myDFPlayer.volume(musicVolume);
      } else {
        myDFPlayer.next();
        isPlaying = true;
      }
    } else {
      handleAdjustButton();
    }
    playButtonSound();
    lastActive = millis();
  }
}

void handleSetButton() {
  switch(currentMode) {
    case STOPWATCH_MODE:
      swRunning = !swRunning;
      if(swRunning) swStart = millis() - swElapsed;
      break;
    case SET_TIME_MODE:
      settingField = (settingField + 1) % 5;
      break;
    case SET_ALARM_MODE:
      settingField = (settingField + 1) % 3;
      break;
  }
}

void handleAdjustButton() {
  switch(currentMode) {
    case STOPWATCH_MODE:
      if(!swRunning) swElapsed = 0;
      break;
    case SET_TIME_MODE:
      adjustTime();
      break;
    case SET_ALARM_MODE:
      adjustAlarm();
      break;
  }
}

void playButtonSound() {buzzer.sound(NOTE_E8, 80); buzzer.sound(NOTE_AS8, 80);}

void checkSleepCondition() {
  if (millis() - lastActive > SLEEP_TIMEOUT && !isPlaying && !swRunning && !alarmTriggered) {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    esp_light_sleep_start();
    display.ssd1306_command(SSD1306_DISPLAYON);
    lastActive = millis();
  }
}

void adjustTime() {
  DateTime now = getCurrentTime();
  
  switch(settingField) {
    case 0: // Hour
      now = DateTime(now.year(), now.month(), now.day(), 
                    (now.hour() + 1) % 24, now.minute(), now.second());
      break;
    case 1: // Minute
      now = DateTime(now.year(), now.month(), now.day(), 
                    now.hour(), (now.minute() + 1) % 60, now.second());
      break;
    case 2: // Second
      now = DateTime(now.year(), now.month(), now.day(), 
                    now.hour(), now.minute(), (now.second() + 1) % 60);
      break;
    case 3: // Day
      now = DateTime(now.year(), now.month(), 
                    (now.day() % 31) + 1, now.hour(), now.minute(), now.second());
      break;
    case 4: // Month
      now = DateTime(now.year(), (now.month() % 12) + 1, 
                    now.day(), now.hour(), now.minute(), now.second());
      break;
  }
}

void adjustAlarm() {
  if (settingField == 0) alarmH = (alarmH + 1) % 24;
  else if (settingField == 1) alarmM = (alarmM +1) % 60;
  else if (settingField == 2) alarmActive = !alarmActive;
}

void updateDisplay() {
  display.clearDisplay();

  switch(currentMode) {
    case CLOCK_MODE:
      drawClock();
      break;
    case STOPWATCH_MODE:
      drawStopWatch();
      break;
    case SET_TIME_MODE:
      drawTimeSetting();
      break;
    case SET_ALARM_MODE:
      drawAlarmSetting();
      break;
    case MUSIC_MODE:
      drawMusicPlayer();
      break;
  }
  drawBatteryStatus();
  display.display();
}

void drawClock() {
  DateTime now = getCurrentTime();
  display.setTextSize(2);
  display.setCursor(17, 20);
  display.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.setTextSize(1);
  display.setCursor(1, 56);
  display.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());

  if(alarmActive) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("A:ON");
  }
}

void drawStopWatch() {
  if(swRunning) swElapsed = millis() - swStart;
  
  unsigned long cs = (swElapsed / 10) % 100;
  unsigned long s = (swElapsed / 1000) % 60;
  unsigned long m = (swElapsed / 60000) % 60;
  
  display.setTextSize(2);
  display.setCursor(17, 20);
  display.printf("%02d:%02d.%02d", m, s, cs);
  
  display.setTextSize(1);
  display.setCursor(47, 50);
  display.print(swRunning ? "RUNNING" : "PAUSED");
}

void drawTimeSetting() {
    DateTime now = getCurrentTime();
  
  display.setTextSize(2);
  display.setCursor(17, 20);
  switch(settingField) {
    case 0: // Hour
      display.print(">"); display.print(now.hour()); 
      display.print(" "); display.print(now.minute()); 
      display.print(" "); display.print(now.second()); 
      break;
    case 1: // Minute
      display.print(" "); display.print(now.hour()); 
      display.print(">"); display.print(now.minute()); 
      display.print(" "); display.print(now.second()); 
      break;
    case 2: // Second
      display.print(" "); display.print(now.hour()); 
      display.print(" "); display.print(now.minute()); 
      display.print(">"); display.print(now.second()); 
      break;
    case 3: // Day
      display.print(">"); display.print(now.day()); 
      display.print(" "); display.print(now.month()); 
      break;
    case 4: // Month
      display.print(" "); display.print(now.day()); 
      display.print(">"); display.print(now.month()); 
      break;
  }
}

void drawAlarmSetting() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SET ALARM");

  display.setTextSize(2);
  display.setCursor(10, 25);
  display.printf("%s%02d:%s%02d", (settingField==0?">":" "), alarmH, (settingField==1?">":" "), alarmM);

  display.setTextSize(1);
  display.setCursor(10, 50);
  display.printf("Status: %s %s", (settingField==2?">":" "), (alarmActive?"ON":"OFF"));
}

void drawMusicPlayer() {
  display.setTextSize(1);
  display.setCursor(30, 0);
  display.print("---MUSIC---");

  display.setTextSize(2);
  display.setCursor(20, 22);
  if (isPlaying) display.print("PLAYING");
  else display.print("PAUSED");

  display.setTextSize(1);
  display.setCursor(0, 50);
  display.printf("Vol:%d", musicVolume);

  display.setTextSize(1);
  display.setCursor(38, 50);
  display.printf("L:%s R:%s", (isLooping?"Y":"N"), (isRandom?"Y":"N"));

}

void drawBatteryStatus() {
  display.setTextSize(1);
  display.setCursor(100, 56);
  display.printf("%d%%", pct);
}