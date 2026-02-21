# 3D_Scanner_ESP8266

<div align="center">

![Arduino](https://img.shields.io/badge/Arduino_IDE-00979D?style=for-the-badge&logo=arduino&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![HTML5](https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)

**Arduino Uno + ESP8266 + VL53L0X lazer sensörü ile WiFi üzerinden gerçek zamanlı 3D nokta bulutu tarayıcı.**

</div>

---

## Donanım

| Bileşen | Adet |
|---|---|
| Arduino Uno | 1 |
| ESP8266 WiFi Modülü | 1 |
| VL53L0X Lazer Mesafe Sensörü | 1 |
| Servo Motor (Pan) | 1 |
| Servo Motor (Tilt) | 1 |

---

## Bağlantı Şeması

| Bileşen | Arduino Pin | Not |
|---|---|---|
| Servo Pan | D9 | PWM |
| Servo Tilt | D10 | PWM |
| VL53L0X SDA | A4 | I2C Data |
| VL53L0X SCL | A5 | I2C Clock |
| ESP8266 RX | D3 (SoftSerial TX) | ⚠️ Voltaj bölücü kullan! |
| ESP8266 TX | D2 (SoftSerial RX) | Doğrudan bağlanabilir |

> ⚠️ Arduino Uno 5V, ESP8266 3.3V mantık seviyesiyle çalışır. D3 → ESP8266 RX hattına mutlaka voltaj bölücü ekle.

---

## Kurulum

### 1. Kütüphaneler

Arduino IDE'de aşağıdaki kütüphaneleri yükle:

- `VL53L0X` — Pololu
- `Servo` — Arduino Built-in
- `SoftwareSerial` — Arduino Built-in
- `ESP8266WiFi` — ESP8266 Community
- `ESP8266WebServer` — ESP8266 Community
- `LittleFS` — ESP8266 Community

### 2. ESP8266 Board Desteği

Arduino IDE → **Dosya > Tercihler > Ek Kart URL'leri** kısmına ekle:

```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Ardından **Araçlar > Kart Yöneticisi**'nden `esp8266` paketini yükle.

### 3. Flash Ayarı

ESP8266'yı derlemeden önce:

**Araçlar > Flash Size > `4MB (FS:2MB OTA:~1MB)`** seçeneğini ayarla.

LittleFS dosya sistemi bu alan olmadan çalışmaz.

### 4. Kodları Yükle

Önce `ESP8266_Server.ino` dosyasını ESP8266'ya yükle, ardından `Uno_Controller.ino` dosyasını Arduino Uno'ya yükle.

---

## Kullanım

1. Sistemi besle — ESP8266 **`3D_Scanner_Project`** adında bir WiFi ağı oluşturur.
2. Telefon veya bilgisayardan bu ağa bağlan. Şifre: **`12345678`**
3. Tarayıcıdan **`192.168.4.1`** adresine git.
4. **`192.168.4.1/scan`** adresine giderek taramayı başlat.
5. Tarama tamamlandıktan sonra ana sayfaya dön — nokta bulutu WebGL ile görselleştirilir.

### Diğer Endpoint'ler

| Adres | İşlev |
|---|---|
| `/scan` | Taramayı başlatır |
| `/status` | JSON formatında anlık durum |
| `/reset` | Önceki tarama verisini siler |
| `/format` | LittleFS dosya sistemini formatlar |

---

## Pan-Tilt Açılarını Ayarlama

`Uno_Controller.ino` içindeki bu bloktan tarama alanını özelleştirebilirsin:

```cpp
const int YAW_MIN  = 0;   // Pan başlangıç açısı (°)
const int YAW_MAX  = 80;  // Pan bitiş açısı     (°)
const int PITCH_MIN = 0;  // Tilt başlangıç açısı (°)
const int PITCH_MAX = 90; // Tilt bitiş açısı     (°)

const int STEP_YAW   = 2; // Adım büyüklüğü — küçük = daha fazla nokta, yavaş tarama
const int STEP_PITCH = 2;

const float VALID_MIN = 0.04f; // Geçerli mesafe aralığı (metre)
const float VALID_MAX = 0.30f;
```

---

## 3D Baskı Parçaları

`3D Printable Parts/` klasöründe sistemin montajı için gerekli parçalar bulunmaktadır.

Tripod için dış kaynak kullanılmıştır:
**[Folding Tripod — MakerWorld](https://makerworld.com/tr/models/671280-folding-tripod-two-sizes?from=search#profileId-599018)**

---

## İlham

Bu proje **[bitluni](https://bitluni.net/3d-scanner)** tarafından geliştirilen orijinal ESP8266 3D tarayıcıdan ilham alınarak yeniden tasarlanmıştır.

---

## Lisans

Bu proje [MIT Lisansı](LICENSE) ile lisanslanmıştır.
