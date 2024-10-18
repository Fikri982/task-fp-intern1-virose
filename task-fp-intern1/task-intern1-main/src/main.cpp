#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <RTClib.h>
#include <Wire.h>

// Function prototypes
void displayTimeWithMenu();
void handleDHTMode();
void handleLDRMode();
void handleAlarmMode();
void drawAnimation(int frame);
void handleCountdown();
void handleStopwatch();
void setAlarm();

// Constants and Variables
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int servoPin = 4;
Servo servo;
const int DHT_PIN = 15;
DHTesp dhtSensor;
#define LDR_PIN 13
RTC_DS1307 rtc;

#define GREEN_BUTTON_PIN 19
#define RED_BUTTON_PIN 18
#define BLUE_BUTTON_PIN 5

#define GREEN_LED_PIN 16
#define RED_LED_PIN 17

int green_button_state;
int red_button_state;
int blue_button_state;
int previous_green_button_state = HIGH;
bool previous_blue_button_state = HIGH;
bool previous_red_button_state = HIGH;

unsigned long lastInputTime = 0;
unsigned long pressStartTime = 0;
int mode = 0;  // 0: Time, 1: DHT, 2: LDR, 3: Alarm
int selectedMode = 0;  // Used for menu selection
const unsigned long TIMEOUT_DURATION = 30000;
const unsigned long LONG_PRESS_DURATION = 2000;

const char* modes[] = {"Time", "DHT", "LDR", "Alarm"};
bool isDHTModeInitialized = false;
bool isLDRModeInitialized = false;
bool isInAlarmMode = false;  // Flag untuk mode alarm
bool isInSubMode = false;
int alarmSubMode = 0;  // 0: Alarm, 1: Countdown, 2: Stopwatch

// Variabel untuk alarm
int alarmHour = 0;
int alarmMinute = 0;
int alarmSecond = 0;

// Variabel untuk countdown
int countdownHour = 0;
int countdownMinute = 0;
int countdownSecond = 0;
bool isCountdownActive = false;
unsigned long countdownStart = 0;

// Variabel untuk stopwatch
unsigned long stopwatchStart = 0;
unsigned long stopwatchElapsed = 0;
bool isStopwatchRunning = false;

void setup() {
    Serial.begin(9600);

    if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("Failed to start SSD1306 OLED"));
    }
    oled.clearDisplay();

    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 2);
    oled.println("VI-ROSE");
    oled.display();

    servo.attach(servoPin, 500, 2400);
    pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BLUE_BUTTON_PIN, INPUT_PULLUP);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);

    if (!rtc.begin()) {
        Serial.println("RTC not found");
    }
    if (!rtc.isrunning()) {
        Serial.println("RTC is NOT running!");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
    pinMode(LDR_PIN, INPUT);

    lastInputTime = millis();
}

void loop() {
    // Check for button presses
    green_button_state = digitalRead(GREEN_BUTTON_PIN);
    red_button_state = digitalRead(RED_BUTTON_PIN);
    blue_button_state = digitalRead(BLUE_BUTTON_PIN);

    // Navigasi di mode default
    if (!isInAlarmMode) {
        if (red_button_state == LOW) {
            selectedMode = (selectedMode - 1 + 4) % 4;
            lastInputTime = millis();
            delay(200);
        } else if (blue_button_state == LOW) {
            selectedMode = (selectedMode + 1) % 4;
            lastInputTime = millis();
            delay(200);
        }

        if (green_button_state == LOW && previous_green_button_state == HIGH) {
            pressStartTime = millis();
        } else if (green_button_state == HIGH && previous_green_button_state == LOW) {
            unsigned long pressDuration = millis() - pressStartTime;
            if (pressDuration >= LONG_PRESS_DURATION) {
                mode = 0; // Reset ke mode default
                lastInputTime = millis();
            } else {
                mode = selectedMode; // Pilih mode
            }
            lastInputTime = millis();
            delay(200);
        }
    } else {  // Di dalam mode alarm
        // Handle mode alarm
        if (red_button_state == LOW && previous_red_button_state == HIGH) {
            alarmSubMode = (alarmSubMode - 1 + 3) % 3;  // Pindah ke opsi sebelumnya
            lastInputTime = millis();
            delay(200);
        } else if (blue_button_state == LOW && previous_blue_button_state == HIGH) {
            alarmSubMode = (alarmSubMode + 1) % 3;  // Pindah ke opsi berikutnya
            lastInputTime = millis();
            delay(200);
        }
        if (green_button_state == LOW && previous_green_button_state == HIGH) {
            lastInputTime = millis();
            switch (alarmSubMode) {
                case 0:
                    setAlarm();
                    break;
                case 1:
                    handleCountdown();
                    break;
                case 2:
                    handleStopwatch();
                    break;
            }
        }
    }

    previous_green_button_state = green_button_state;
    previous_red_button_state = red_button_state;
    previous_blue_button_state = blue_button_state;

    // Timeout untuk kembali ke mode default
    if (millis() - lastInputTime > TIMEOUT_DURATION) {
        mode = 0;
    }

    // Menjalankan mode yang dipilih
    switch (mode) {
        case 0:
            displayTimeWithMenu();
            break;
        case 1:
            handleDHTMode();
            break;
        case 2:
            handleLDRMode();
            break;
        case 3:
            handleAlarmMode();
            break;
    }
}

// Function to display the current time, date, and menu with mode selection
void displayTimeWithMenu() {
    DateTime now = rtc.now();
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.printf("%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    oled.setCursor(0, 14);
    oled.printf("%02d/%02d/%04d", now.day(), now.month(), now.year());

    oled.setCursor(0, 25);
    oled.setTextSize(1);
    for (int i = 1; i < 4; i++) {
        oled.setCursor(0, 25 + (i - 1) * 12);
        if (i == selectedMode) {
            oled.print("> ");
        } else {
            oled.print("  ");
        }
        oled.print(modes[i]);
    }
    oled.display();
}

// Mode 1: Handling DHT Sensor
void handleDHTMode() {
    if (!isDHTModeInitialized) {
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        for (int i = 0; i < 128; i += 32) {
            oled.clearDisplay();
            oled.fillRect(i, 0, 32, 64, WHITE);
            oled.display();
            delay(100);
        }
        oled.clearDisplay();
        isDHTModeInitialized = true;
    }

    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    float temperature = data.temperature;
    float humidity = data.humidity;

    String status;
    if (temperature < 20.0) {
        status = "Dingin";
    } else if (temperature < 25.0) {
        status = "Netral";
    } else if (temperature < 30.0) {
        status = "Hangat";
    } else {
        status = "Panas";
    }

    static int drawFrame = 0;

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.println("DHT22 Data:");
    oled.printf("Temp: %.2f C\n", temperature);
    oled.printf("Humidity: %.1f%%\n", humidity);
    oled.printf("Status: %s\n", status.c_str());
    oled.display();

    drawAnimation(drawFrame);
    oled.display();

    drawFrame = (drawFrame + 1) % 10;

    float minTemperature = 15.0;
    float maxTemperature = 35.0;
    float middleTemperature = (minTemperature + maxTemperature) / 2;

    int servoPos = map(temperature, minTemperature, maxTemperature, 0, 180);
    servo.write(servoPos);

    if (temperature < middleTemperature) {
        digitalWrite(RED_LED_PIN, HIGH);
        digitalWrite(GREEN_LED_PIN, LOW);
    } else {
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, HIGH);
    }
    delay(100);
}

void drawAnimation(int frame) {
    int baseY = 54;
    for (int i = 0; i < 128; i += 12) {
        int flameHeight = random(3, 8);
        int offset = (frame + i) % 12;
        oled.drawLine(i + offset, baseY, i + offset, baseY - flameHeight, WHITE);
    }
}

// Mode 2: Handling LDR sensor
void handleLDRMode() {
    if (!isLDRModeInitialized) {
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(WHITE);
        for (int i = 0; i < 128; i += 32) {
            oled.clearDisplay();
            oled.fillRect(i, 0, 32, 64, WHITE);
            oled.display();
            delay(100);
        }
        oled.clearDisplay();
        isLDRModeInitialized = true;
    }

    int ldrValue = analogRead(LDR_PIN);
    
    // Mengonversi nilai LDR menjadi lux
    int lux = map(ldrValue, 0, 4095, 0, 1000);
    String status = (lux < 500) ? "Gelap" : "Terang";

    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.printf("Lux: %d\n", lux);
    oled.printf("Status: %s\n", status.c_str());

    // Animasi matahari/bulan berdasarkan status "gelap" atau "terang"
    if (lux < 500) {
        // Gambar bulan
        oled.fillCircle(108, 20, 10, WHITE);
        oled.fillCircle(104, 16, 8, BLACK); // Bentuk bulan sabit

        // Gambar bintang
        for (int i = 0; i < 5; i++) {
            int x = random(128); // Posisi acak bintang
            int y = random(64);
            oled.drawPixel(x, y, WHITE);
        }

        static int flameFrame = 0;
        // Tambahkan animasi gelombang
        for (int x = 0; x < 128; x++) {
            int y = 32 + (sin((x + flameFrame) * 0.1) * 10); // Gelombang sinus
            oled.drawPixel(x, y, WHITE);
        }
        flameFrame = (flameFrame + 1) % 360; // Update frame animasi

    } else {
        // Gambar matahari
        oled.fillCircle(108, 20, 10, WHITE); // Matahari
        oled.display();

        // Tambahkan animasi awan
        oled.fillCircle(50, 20, 8, WHITE); // Tengah awan
        oled.fillCircle(62, 20, 6, WHITE); // Kanan awan
        oled.fillCircle(38, 20, 6, WHITE); // Kiri awan
    }

    // Tambahkan progress bar untuk menggambarkan lux
    int barLength = map(lux, 0, 1000, 0, 128);  // Bar sepanjang 128 pixel
    oled.fillRect(0, 50, barLength, 10, WHITE); // Bar horizontal di bagian bawah layar

    oled.display();

    // Kontrol servo berdasarkan nilai lux
    int servoPos = map(lux, 0, 1000, 0, 180);
    servo.write(servoPos);

    // Kontrol LED berdasarkan lux
    if (lux < 500) {
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, HIGH);
    } else {
        digitalWrite(GREEN_LED_PIN, HIGH);
        digitalWrite(RED_LED_PIN, LOW);
    }

    delay(100);
}

// Mode 3: Handling Alarm Mode with Countdown and Stopwatch
void handleAlarmMode() {
    if (!isInAlarmMode) {
        alarmSubMode = 0;  // Reset ke pilihan pertama
        isInAlarmMode = true;
    }
    
    // Tampilan mode alarm dengan kursor
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.println("Alarm Mode:");

     // Navigasi submode
    for (int i = 0; i < 3; i++) {
        oled.setCursor(0, 10 + (i * 10));
        if (i == alarmSubMode) {
            oled.print("> ");
        } else {
            oled.print("  ");
        }
        if (i == 0) {
            oled.print("Alarm");
        } else if (i == 1) {
            oled.print("Countdown");
        } else if (i == 2) {
            oled.print("Stopwatch");
        }
    }
    oled.display();

    // Navigasi menu alarm
    if (red_button_state == LOW && previous_red_button_state == HIGH) {
        alarmSubMode = (alarmSubMode - 1 + 3) % 3;  // Pindah ke opsi sebelumnya
        lastInputTime = millis();
        delay(200);  // Debounce delay
    } else if (blue_button_state == LOW && previous_blue_button_state == HIGH) {
        alarmSubMode = (alarmSubMode + 1) % 3;  // Pindah ke opsi berikutnya
        lastInputTime = millis();
        delay(200);  // Debounce delay
    }

    // Pilih submode alarm
    if (green_button_state == LOW && previous_green_button_state == HIGH) {
        lastInputTime = millis();
        isInSubMode = true;
        switch (alarmSubMode) {
            case 0:
                setAlarm();
                break;
            case 1:
                handleCountdown();
                break;
            case 2:
                handleStopwatch();
                break;
        }
        isInSubMode = false;
    }

    // Cek timeout untuk keluar dari alarm mode
    if (millis() - lastInputTime > TIMEOUT_DURATION) {
        isInAlarmMode = false;  // Kembali ke mode default jika tidak ada input
    }
}

// Submode 1: Set and Handle Alarm
void setAlarm() {
    // Inisialisasi RTC dan variabel waktu
    DateTime now;
    // Pengaturan alarm
    while (true) {
        // Ambil waktu sekarang
        now = rtc.now();
        // Baca status tombol
        red_button_state = digitalRead(RED_BUTTON_PIN);
        blue_button_state = digitalRead(BLUE_BUTTON_PIN);
        green_button_state = digitalRead(GREEN_BUTTON_PIN);

        // Cek tombol merah dan biru untuk mengatur jam dan menit
        if (red_button_state == LOW && previous_red_button_state == HIGH) {
            alarmMinute = (alarmMinute + 1) % 60; // Tambah menit
            delay(200);  // Debounce delay
        }
        if (blue_button_state == LOW && previous_blue_button_state == HIGH) {
            alarmHour = (alarmHour + 1) % 24; // Tambah jam
            delay(200);  // Debounce delay
        }
        if (green_button_state == LOW && previous_green_button_state == HIGH) {
            // Simpan pengaturan alarm (tidak keluar dari mode)
            delay(200);
        }

        // Tampilkan waktu saat ini dan pengaturan alarm
        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.printf("Current Time:\n%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        oled.setCursor(0, 16); // Pindah ke baris berikutnya
        oled.printf("Set Alarm:\n%02d:%02d", alarmHour, alarmMinute);
        oled.display();
        
        // Periksa jika waktu sekarang sesuai dengan alarm
        if (now.hour() == alarmHour && now.minute() == alarmMinute) {
            oled.clearDisplay();
            oled.setCursor(0, 0);
            oled.setTextSize(2);
            oled.println("Alarm!");
            oled.display();
            delay(5000); // Tahan alarm selama 5 detik
            break;  // Keluar dari loop 
        }

        // Simpan status tombol sebelumnya
        previous_red_button_state = red_button_state;
        previous_blue_button_state = blue_button_state;
        previous_green_button_state = green_button_state;
        
        delay(100); // Delay untuk menghindari terlalu banyak pembaruan tampilan
    }
    displayTimeWithMenu(); // Return to the main menu
    isInAlarmMode = false; // Exit alarm mode
}

// Submode 2: Countdown Timer
void handleCountdown() {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.println("Set Countdown:");
    oled.setCursor(0, 10);
    oled.printf("Time: %02d:%02d", countdownHour, countdownMinute);
    oled.display();

    while (true) {
        // Cek tombol merah dan biru untuk mengatur jam dan menit
        red_button_state = digitalRead(RED_BUTTON_PIN);
        blue_button_state = digitalRead(BLUE_BUTTON_PIN);
        green_button_state = digitalRead(GREEN_BUTTON_PIN);
       
        if (red_button_state == LOW && previous_red_button_state == HIGH) {
            countdownMinute = (countdownMinute + 1) % 60; // Tambah menit
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 0);
            oled.println("Set Countdown:");
            oled.setCursor(0, 10);
            oled.printf("Time: %02d:%02d", countdownHour, countdownMinute);
            oled.display();
            delay(200);  // Debounce delay
        }
        if (blue_button_state == LOW && previous_blue_button_state == HIGH) {
            countdownHour = (countdownHour + 1) % 24; // Tambah jam
            oled.clearDisplay();
            oled.setTextSize(1);
            oled.setCursor(0, 0);
            oled.println("Set Countdown:");
            oled.setCursor(0, 10);
            oled.printf("Time: %02d:%02d", countdownHour, countdownMinute);
            oled.display();
            delay(200);  // Debounce delay
        }
        if (green_button_state == LOW && previous_green_button_state == HIGH) {
            // Simpan pengaturan countdown dan keluar
            countdownStart = millis();
            isCountdownActive = true;
            break;
        }
    }

    // Mulai countdown
    unsigned long countdownTime = countdownHour * 3600 + countdownMinute * 60; // Hitung total detik
    while (countdownTime > 0 && isCountdownActive) {
        countdownSecond = countdownTime % 60;
        countdownMinute = (countdownTime / 60) % 60;
        countdownHour = countdownTime / 3600;

        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setCursor(0, 0);
        oled.printf("Countdown:\n%02d:%02d:%02d", countdownHour, countdownMinute, countdownSecond);
        oled.display();

        delay(1000); // Delay 1 detik
        countdownTime--; // Kurangi satu detik

        // Cek jika countdown selesai
        if (countdownTime == 0) {
            oled.clearDisplay();
            oled.setTextSize(2); // Ukuran teks lebih besar
            oled.setCursor(0, 0);
            oled.println("Countdown");
            oled.setCursor(0, 20);
            oled.println("Finished!");
            oled.display();
            delay(2000); // Tampilkan teks 
            isCountdownActive = false; // Reset status
            break;  // Keluar setelah countdown selesai
            countdownMinute = 0;
            countdownSecond = 0;
        }
    }
    displayTimeWithMenu(); // Return to the main menu
    isInAlarmMode = false; // Exit alarm mode
}

// Submode 3: Stopwatch
void handleStopwatch() {
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setCursor(0,0);
    oled.println("Stopwatch:");

    while (true) {
        // Memeriksa status tombol
        red_button_state = digitalRead(RED_BUTTON_PIN);
        blue_button_state = digitalRead(BLUE_BUTTON_PIN);
        // Start stopwatch
        if (red_button_state == LOW && previous_red_button_state == HIGH) {
            if (!isStopwatchRunning) {
                stopwatchStart = millis() - stopwatchElapsed; // Reset waktu yang sudah berlalu
                isStopwatchRunning = true;
            }
            delay(200);  // Debounce delay
        }

        // Pause stopwatch
        if (blue_button_state == LOW && previous_blue_button_state == HIGH) {
            if (isStopwatchRunning) {
                stopwatchElapsed = millis() - stopwatchStart; // Hitung waktu yang sudah berlalu
                isStopwatchRunning = false;
            } else {
                stopwatchStart = millis() - stopwatchElapsed; // Lanjutkan dari waktu yang sudah berlalu
                isStopwatchRunning = true;
            }
            delay(200);  // Debounce delay
        }

        // Menampilkan waktu stopwatch
        if (isStopwatchRunning) {
            unsigned long elapsedTime = millis() - stopwatchStart;
            int seconds = (elapsedTime / 1000) % 60;
            int minutes = (elapsedTime / 60000) % 60;
            int hours = (elapsedTime / 3600000);
            oled.clearDisplay();
            oled.setCursor(0, 20);
            oled.printf("Time: %02d:%02d:%02d", hours, minutes, seconds);
        } else {
            oled.clearDisplay();
            oled.setCursor(0, 20);
            oled.printf("Paused: %02d:%02d:%02d", (stopwatchElapsed / 3600000), (stopwatchElapsed / 60000) % 60, (stopwatchElapsed / 1000) % 60);
        }

        oled.display();
    }
}

void handleDefaultMode() {
    // Return to default mode after alarm submodes
    isInAlarmMode = false;
}
