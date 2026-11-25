/*
  Monitor Suhu CPU Komputer dengan DS18B20 + OLED — ESP32
  ========================================================
  Cara kerja:
  Sensor suhu DS18B20 ditempel di heatsink CPU komputer (pakai pasta
  termal supaya kontak panasnya rapat). ESP32 membaca suhu lewat
  protokol OneWire di satu kabel data, lalu menampilkannya di layar
  OLED SSD1306 128x64. Begitu suhu heatsink menyentuh 75 derajat
  Celsius, buzzer aktif berbunyi sebagai peringatan supaya pengguna
  segera menyimpan pekerjaan sebelum komputer mati mendadak.

  KEJUJURAN TEKNIS (baca dulu):
  Yang diukur alat ini adalah suhu HEATSINK, bukan suhu inti CPU.
  Suhu heatsink selalu lebih rendah beberapa derajat dari suhu inti
  karena panas harus merambat dari die CPU ke heatsink. Jadi angka
  75 derajat di sini adalah batas heatsink — kalau heatsink sudah
  75 derajat, inti CPU hampir pasti lebih panas lagi. Alarm ini
  sengaja konservatif: lebih baik berbunyi terlalu awal daripada
  terlambat.

  Wiring:
    - DS18B20: DQ -> GPIO4 (resistor pull-up 4,7 kOhm ke 3V3),
      VCC -> 3V3, GND -> GND.
    - OLED SSD1306 128x64 alamat I2C 0x3C: SDA -> GPIO21,
      SCL -> GPIO22, VCC -> 3V3, GND -> GND.
    - Buzzer aktif: kaki + -> GPIO2, kaki - -> GND.
      Buzzer aktif bunyi saat pin diberi HIGH (tidak butuh nada PWM).
    - ESP32 dicatu lewat USB 5 V; semua GND digabung.

  Library yang dibutuhkan:
    - OneWire 2.3.8
    - DallasTemperature 4.0.6
    - Adafruit SSD1306 2.5.17
    - Adafruit GFX Library 1.13.0

  Video penjelasan (60 detik): link menyusul setelah upload
  Repo: https://github.com/oerban70-cloud/monitor-suhu-cpu-ds18b20
*/

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Konfigurasi pin ----------
const uint8_t PIN_DS18B20_DATA = 4;   // DQ sensor DS18B20 (OneWire)
const uint8_t PIN_BUZZER       = 2;   // buzzer aktif, HIGH = bunyi
const uint8_t PIN_SDA_OLED     = 21;  // SDA bus I2C
const uint8_t PIN_SCL_OLED     = 22;  // SCL bus I2C

// ---------- Konfigurasi OLED ----------
const uint8_t ALAMAT_OLED      = 0x3C;  // alamat I2C OLED SSD1306
const uint8_t LEBAR_OLED       = 128;   // lebar layar (piksel)
const uint8_t TINGGI_OLED      = 64;    // tinggi layar (piksel)
const int8_t  PIN_RESET_OLED   = -1;    // OLED tanpa pin reset terpisah

// ---------- Konfigurasi suhu dan alarm ----------
const float   SUHU_ALARM_NYALA = 75.0;  // buzzer nyala bila suhu >= nilai ini
const float   SUHU_ALARM_MATI  = 72.0;  // buzzer mati bila suhu turun ke nilai ini
const float   SUHU_SENSOR_LEPAS = -127.0;  // kode DallasTemperature bila sensor tidak terbaca
const uint8_t RESOLUSI_SENSOR  = 12;    // resolusi DS18B20 (bit), 0,0625 derajat per langkah
const uint8_t INDEKS_SENSOR    = 0;     // sensor pertama di bus OneWire

// ---------- Konfigurasi waktu ----------
const unsigned long INTERVAL_BACA_MS = 2000;  // baca suhu tiap 2 detik
const unsigned long INTERVAL_BIP_MS  = 500;   // buzzer bip tiap 0,5 detik saat alarm

// ---------- Objek perangkat ----------
OneWire busOneWire(PIN_DS18B20_DATA);
DallasTemperature sensorSuhu(&busOneWire);
Adafruit_SSD1306 layarOled(LEBAR_OLED, TINGGI_OLED, &Wire, PIN_RESET_OLED);

// ---------- Status berjalan ----------
float suhuTerakhir = 0.0;        // suhu terakhir yang berhasil dibaca
bool  sensorTerbaca = false;     // false bila sensor lepas / tidak merespons
bool  alarmAktif = false;        // true selama suhu di atas ambang alarm
unsigned long waktuBacaTerakhir = 0;  // millis() saat baca suhu terakhir
unsigned long waktuBipTerakhir = 0;   // millis() saat bip buzzer terakhir
bool  buzzerMenyala = false;     // status pin buzzer saat ini

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  // Bus I2C untuk OLED (pin SDA/SCL sesuai wiring di atas).
  Wire.begin(PIN_SDA_OLED, PIN_SCL_OLED);

  // Sensor suhu: resolusi tertinggi (12 bit) supaya langkahnya halus.
  sensorSuhu.begin();
  sensorSuhu.setResolution(RESOLUSI_SENSOR);

  // OLED: gagal init = tidak ada gunanya lanjut tanpa layar.
  if (!layarOled.begin(SSD1306_SWITCHCAPVCC, ALAMAT_OLED)) {
    Serial.println("OLED tidak ditemukan di alamat 0x3C, cek wiring I2C!");
    while (true) {
      delay(1000);
    }
  }
  layarOled.clearDisplay();
  layarOled.setTextColor(SSD1306_WHITE);

  // Baca sekali di awal supaya layar langsung berisi angka.
  bacaSuhu();
  tampilkanSuhu();
}

void loop() {
  unsigned long sekarang = millis();

  // Baca suhu tiap INTERVAL_BACA_MS, tanpa delay() supaya buzzer
  // tetap bisa berbunyi di antara pembacaan.
  if (sekarang - waktuBacaTerakhir >= INTERVAL_BACA_MS) {
    waktuBacaTerakhir = sekarang;
    bacaSuhu();
    perbaruiAlarm();
    tampilkanSuhu();
  }

  // Saat alarm aktif, buzzer berbunyi putus-putus (bip), bukan bunyi
  // terus-menerus, supaya tidak terlalu mengganggu.
  if (alarmAktif) {
    if (sekarang - waktuBipTerakhir >= INTERVAL_BIP_MS) {
      waktuBipTerakhir = sekarang;
      buzzerMenyala = !buzzerMenyala;
      digitalWrite(PIN_BUZZER, buzzerMenyala ? HIGH : LOW);
    }
  }
}

// Minta DS18B20 mengukur suhu, simpan hasilnya ke suhuTerakhir.
// Menandai sensorTerbaca = false bila sensor tidak merespons.
void bacaSuhu() {
  sensorSuhu.requestTemperatures();
  float suhu = sensorSuhu.getTempCByIndex(INDEKS_SENSOR);
  if (suhu == SUHU_SENSOR_LEPAS) {
    sensorTerbaca = false;
    Serial.println("DS18B20 tidak terbaca, cek wiring DQ dan pull-up 4,7 kOhm!");
  } else {
    suhuTerakhir = suhu;
    sensorTerbaca = true;
    Serial.print("Suhu heatsink: ");
    Serial.print(suhuTerakhir);
    Serial.println(" C");
  }
}

// Atur alarmAktif dengan histeresis: nyala di SUHU_ALARM_NYALA,
// baru mati setelah suhu turun ke SUHU_ALARM_MATI, supaya buzzer
// tidak hidup-mati berulang saat suhu berayun di sekitar ambang.
void perbaruiAlarm() {
  if (!sensorTerbaca) {
    matikanBuzzer();
    alarmAktif = false;
    return;
  }
  if (!alarmAktif && suhuTerakhir >= SUHU_ALARM_NYALA) {
    alarmAktif = true;
    Serial.println("ALARM: suhu heatsink 75 C atau lebih!");
  } else if (alarmAktif && suhuTerakhir <= SUHU_ALARM_MATI) {
    alarmAktif = false;
    matikanBuzzer();
    Serial.println("Alarm mati, suhu sudah turun.");
  }
}

// Matikan buzzer dan reset status bip-nya.
void matikanBuzzer() {
  buzzerMenyala = false;
  digitalWrite(PIN_BUZZER, LOW);
}

// Gambar suhu di OLED: angka besar, label, dan status alarm.
// Kalau sensor tidak terbaca, tampilkan pesan kesalahan.
void tampilkanSuhu() {
  layarOled.clearDisplay();
  layarOled.setTextSize(1);
  layarOled.setCursor(0, 0);
  layarOled.print("SUHU HEATSINK CPU");

  if (!sensorTerbaca) {
    layarOled.setTextSize(2);
    layarOled.setCursor(0, 24);
    layarOled.print("SENSOR?");
    layarOled.setTextSize(1);
    layarOled.setCursor(0, 52);
    layarOled.print("cek kabel DQ + 4,7k");
  } else {
    layarOled.setTextSize(3);
    layarOled.setCursor(0, 18);
    layarOled.print(suhuTerakhir, 1);
    layarOled.setTextSize(2);
    layarOled.print(" C");
    layarOled.setTextSize(1);
    layarOled.setCursor(0, 52);
    if (alarmAktif) {
      layarOled.print("PANAS! simpan kerjaanmu");
    } else {
      layarOled.print("normal - ambang 75 C");
    }
  }
  layarOled.display();
}
