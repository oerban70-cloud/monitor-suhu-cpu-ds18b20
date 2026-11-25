# Monitor Suhu CPU Komputer — ESP32 + DS18B20 + OLED

Komputermu panas sampai mati sendiri tanpa peringatan? Sensor kecil ini
yang memantaunya: DS18B20 ditempel di heatsink CPU komputer (pakai pasta
termal supaya kontak panasnya rapat), ESP32 membaca suhunya lewat satu
kabel data, dan angkanya tampil di layar OLED — lengkap dengan buzzer
yang berbunyi begitu suhu menyentuh 75 derajat, supaya kamu sempat
menyimpan pekerjaanmu sebelum komputer mati mendadak.

Video penjelasan (60 detik): link menyusul setelah upload

## Peringatan jujur: yang diukur suhu heatsink, bukan suhu inti CPU

Sensor DS18B20 di sini menempel di HEATSINK, bukan di dalam chip CPU —
jadi angka yang tampil adalah suhu heatsink. Suhu heatsink selalu lebih
rendah beberapa derajat dari suhu inti CPU, karena panas harus merambat
dari die CPU ke heatsink. Artinya: kalau heatsink saja sudah 75 derajat,
inti CPU hampir pasti lebih panas lagi. Ambang alarm 75 derajat ini
sengaja konservatif — lebih baik berbunyi terlalu awal daripada terlambat.

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| DS18B20 | Sensor suhu digital (OneWire, resolusi 12 bit) |
| Resistor 4,7 kΩ | Pull-up wajib di kabel data DQ |
| OLED 128x64 SSD1306 (0x3C) | Tampil suhu heatsink + status alarm |
| Buzzer aktif | Bunyi saat GPIO2 HIGH |
| Pasta termal | Supaya sensor menempel rapat di heatsink |
| Catu USB 5 V | Adaptor USB / powerbank untuk ESP32 |

## Wiring

| Dari | Ke |
|---|---|
| DS18B20 DQ | ESP32 GPIO4 (OneWire) |
| DS18B20 DQ | Resistor 4,7 kΩ ke 3V3 (pull-up) |
| DS18B20 VCC | ESP32 3V3 |
| DS18B20 GND | GND |
| OLED SDA | ESP32 GPIO21 |
| OLED SCL | ESP32 GPIO22 |
| OLED VCC | ESP32 3V3 |
| OLED GND | GND |
| Buzzer + | ESP32 GPIO2 |
| Buzzer - | GND |
| ESP32 VIN 5V | Catu USB 5 V |
| Semua GND | Digabung |

Skema lengkap: [schematic.png](schematic.png)

## Cara kerja

DS18B20 adalah sensor suhu digital yang berkomunikasi lewat protokol
OneWire — cukup satu kabel data (DQ) untuk mengirim suhu. ESP32 meminta
pengukuran lewat `requestTemperatures()`, lalu membaca hasilnya dengan
`getTempCByIndex(0)`. Pembacaan dilakukan tiap 2 detik, tanpa `delay()`
supaya buzzer tetap bisa berbunyi di antara pembacaan.

Alarm memakai histeresis: buzzer mulai berbunyi saat suhu mencapai 75
derajat, dan baru berhenti setelah suhu turun ke 72 derajat — supaya
buzzer tidak hidup-mati berulang saat suhu berayun di sekitar ambang.
Selama alarm aktif, buzzer berbunyi putus-putus (bip tiap 0,5 detik),
bukan bunyi terus-menerus.

Kalau sensor tidak terbaca (mis. kabel DQ lepas atau pull-up tidak
terpasang), DS18B20 mengembalikan -127 derajat. Sketch mendeteksi angka
ini, menampilkan "SENSOR?" di OLED, dan mematikan alarm — jadi angka
-127 tidak pernah disalahartikan sebagai suhu.

Rumus yang dipakai:

```
suhu heatsink (C) = bacaan DS18B20, resolusi 12 bit (0,0625 C per langkah)
alarm nyala bila suhu >= 75 C, mati bila suhu <= 72 C
```

## Pemasangan sensor di heatsink

1. Olesi pasta termal tipis di permukaan heatsink CPU, dekat dengan
   pusat chip.
2. Tempelkan badan logam DS18B20 di atasnya, lalu ikat/jepit supaya
   tidak bergeser (mis. dengan kabel ties atau lakban tahan panas).
3. Jangan menempelkan sensor di kipas — yang diukur suhu logam
   heatsink, bukan aliran udara.

## Angka compile terverifikasi

Sketch `MonitorSuhuCpuDs18b20.ino` terverifikasi compile pada
25 Nov 2026 dengan arduino-cli 1.6.0, ESP32 core 4.0.0:

- Ukuran sketch: 332.573 byte (25% dari program storage)
- RAM (global variables): 24.840 byte (7% dari dynamic memory)

Library yang dipakai: OneWire 2.3.8, DallasTemperature 4.0.6,
Adafruit SSD1306 2.5.17, Adafruit GFX Library 1.13.0,
Adafruit BusIO 1.17.4 (tersedia di `libraries/`).

## Lisensi

MIT — lihat [LICENSE](LICENSE).
