#define BLYNK_TEMPLATE_ID "TMPL6KeapFrSP"
#define BLYNK_TEMPLATE_NAME "Cat Feeder"
#define BLYNK_AUTH_TOKEN "y8AAr6DmP_S3Ky_TTT99eAgVm9F-ojfc"

#include <BlynkSimpleEsp32.h> //Menambahkan library blynk
#include <WiFiClient.h> // Menambahkan library wifi
#include <WiFi.h> // Menambahkan library wifi (juga)
#include <HX711.h> // Menambahkan library load cell
#include "time.h" // Menambahkan library waktu

#define HOUR_SLIDER V0 //Virtual pin BLynk buat nentuin jam makan pertama
#define INTERVAL_SLIDER V1 // Virtual pin Blynk buat nentuin interval jam makan
#define FREQUENCY_SLIDER V2 // Virtual pin Blynk buat nentuin frekuensi makan
#define PORTION_SLIDER V3 // Virtual pin Blynk buat nentuin porsi per 1x makan
#define SCALE_LABEL V4 // Virtual pin Blynk buat ngasih liat hasil timbangan


char ssid[] = "Infinix note 40"; // Nama Wifi
char pass[] = "ESP32TEST"; // Password Wifi

const char* ntpServer = "pool.ntp.org"; // Website untuk ngambil jam saat ini
const long  gmtOffset_sec = 25200; // GMT +7
const int   daylightOffset_sec = 3600; // ini gatau apa wkwk


/*________________________________________________
|              CARA KERJA LOAD CELL              |
|================================================|       
| Mengkonversi  perbedaan  gaya  tekanan  antara |
| ujung batang besi dengan ujung satunya menjadi |
| sinyal listrik, kemudian modul HX711 berfungsi |
| untuk mengamplifikasi sinyal listrik agar dapat|
| dibaca oleh Microcontroller ESP32.             |
==================================================
Referensi: https://www.800loadcel.com/load-cell-and-strain-gauge-basics.html 
*/
const int LOADCELL_DOUT_PIN = 16; // Pin DOUT load cell
const int LOADCELL_SCK_PIN = 4; // Pin SCK load cell

const int RED_LED_PIN = 25; // Pin LED merah
const int YELLOW_LED_PIN = 26; // Pin LED kuning
const int GREEN_LED_PIN = 27; // Pin LED hijau

/*_________________________________________________
|            CARA KERJA SENSOR JARAK               |
|==================================================|       
| Pin Trigger  akan memancarkan  sonar  ultrasonik |
| yang kemudian akan memantul pada permukaan benda,|
| lalu pin Echo akan menangkap  pantulan sonarnya. |
| jarak akan dihitung dengan cara menghitung waktu |
| tempuh sonar dikali kecepatan suara dibagi dua.  |
===================================================
Referensi: https://www.geeksforgeeks.org/arduino-ultrasonic-sensor/
*/
const int TRIG_PIN = 32; // Pin Trigger sensor jarak
const int ECHO_PIN = 33; // Pin Echo sensor jarak

const int MOTOR_PIN1 = 5; // Pin motor 1
const int MOTOR_PIN2 = 17; // Pin motor 2

int eatingStart = 0; // Variabel yang menyimpan jam makan pertama
int eatingInterval = 0; // Variabel yang menyimpan interval jam makan
int eatingFrequency = 0; // Variabel yang menyimpan frekuensi makan
int eatingPortion = 0; // Variabel yang menyimpan porsi per 1x makan
long scaleReading = 0; // Variabel yang menyimpan pembacaan sensor berat
float duration_us, distance_cm; // Variabel untuk perhitungan sensor jarak
bool isPouring = false; // Variabel untuk menentukan motor bergerak atau tidak

HX711 scale; // Variabel objek sensor berat


// Fungsi yang akan dipanggil apabila slider jam makan pada aplikasi digerakkan
BLYNK_WRITE(HOUR_SLIDER){
  eatingStart = param.asInt(); //Menyimpan perubahan slider ke variabel di MCU
  Serial.print("New eating start: "); //Mencetak perubahan variabel di Serial Monitor
  Serial.println(eatingStart);
}

// Fungsi yang akan dipanggil apabila slider interval jam makan pada aplikasi digerakkan
BLYNK_WRITE(INTERVAL_SLIDER){
  eatingInterval = param.asInt(); //Menyimpan perubahan slider ke variabel di MCU
  Serial.print("New interval: "); //Mencetak perubahan variabel di Serial Monitor
  Serial.println(eatingInterval);
}

// Fungsi yang akan dipanggil apabila slider frekuensi makan pada aplikasi digerakkan
BLYNK_WRITE(FREQUENCY_SLIDER){
  eatingFrequency = param.asInt(); //Menyimpan perubahan slider ke variabel di MCU
  Serial.print("New Frequency: "); //Mencetak perubahan variabel di Serial Monitor
  Serial.println(eatingFrequency);
}

// Fungsi yang akan dipanggil apabila slider porsi makan pada aplikasi digerakkan
BLYNK_WRITE(PORTION_SLIDER){
  eatingPortion = param.asInt(); //Menyimpan perubahan slider ke variabel di MCU
  Serial.print("New Portion: "); //Mencetak perubahan variabel di Serial Monitor
  Serial.println(eatingPortion);
}

// Fungsi yang akan berjalan satu kali, pada saat ESP pertama kali menyala
void setup(){
  Serial.begin(115200); // Mengatur baudrate serial monitor

  //Menentukan mode setiap pin, antara INPUT atau OUTPUT
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(MOTOR_PIN1, OUTPUT);
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  //Memulai koneksi aplikasi Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); 
  //Mengambil waktu dari website
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  //Memulai instance objek sensor berat
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  //Menentukan scale factor (referensi: https://randomnerdtutorials.com/esp32-load-cell-hx711/)
  scale.set_scale(383.3157);

  //Tare kayak timbangan pada umumnya
  scale.tare();
  
  //Memulai fungsi yang menentukan kapan makan
  xTaskCreatePinnedToCore(vTaskEat, "TaskEat", 10000, NULL, 1, NULL, 0);

  //Memulai fungsi yang menggerakkan motor ketika waktu makan
  xTaskCreatePinnedToCore(vTaskMotor, "TaskMotor", 10000, NULL, 1, NULL, 0);

  //Memulai fungsi yang membaca sensor jarak dan menampilkan outputnya menggunakan LED
  xTaskCreatePinnedToCore(vTaskSensor, "TaskSensor", 10000, NULL, 1, NULL, 1 );

  ///Memulai fungsi yang membaca load cell dan menampilkan outputnya di aplikasi
  xTaskCreatePinnedToCore(vTaskScale, "TaskScale", 10000, NULL, 1, NULL, 1);


}

//Fungsi yang gerakkin motor
void vTaskMotor(void *pvParam){
  // While(1) artinya fungsi ini bakal diulang terus-terusan
  while(1){

    // Kalo variabel isPouring bernilai TRUE, motor gerak
    if(isPouring){
      Serial.println("Pouring Food..."); 
      digitalWrite(MOTOR_PIN1, HIGH);
      digitalWrite(MOTOR_PIN2, LOW);
    }else{
      //Kalo FALSE, motor berhenti
      digitalWrite(MOTOR_PIN1, LOW);
      digitalWrite(MOTOR_PIN2, LOW);
    }

    //delay 500ms
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Fungsi yang baca sensor jarak sama nyalain indikator LED 
void vTaskSensor(void *pvParam){
  while(1){
    digitalWrite(TRIG_PIN, HIGH); // Mancarin sonar
    delayMicroseconds(10); // tunggu 10ms
    digitalWrite(TRIG_PIN, LOW); // Matiin pancaran sonar

    duration_us = pulseIn(ECHO_PIN, HIGH); //Ngitung durasi tempuh sonar
    distance_cm = 0.017 * duration_us; //Ngitung jarak berdasarkan durasi tempuh 

    
    if(distance_cm <= 15 && distance_cm > 10){ //Kalo jarak antara 10-15 cm, nyalain LED merah
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(YELLOW_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }else if(distance_cm <= 10 && distance_cm > 5){ //Kalo jarak antara 5-10 cm, nyalain LED kuning
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
    }else if(distance_cm <= 5){ //Kalo jarak antara <= 5 cm, nyalain LED hijau
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
    }else{ //Kalo jarak > 15 cm, matiin semua LED
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
    }

    //jeda 500ms
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// Fungsi yang baca timbangan
void vTaskScale(void *pvParam){
  while(1){
    scaleReading = scale.get_units(10); // baca timbangan 10x trus diambil rata-rata
    scale.power_down(); //Matiin timbangan
    Blynk.virtualWrite(SCALE_LABEL, scaleReading); //Kirim hasil timbangan ke aplikasi
    vTaskDelay(pdMS_TO_TICKS(500)); //Jeda 500ms
    scale.power_up(); //Nyalain timbangan lagi
  }
}

//Fungsi yang ngitung jam makan
void vTaskEat(void *pvParam){

  struct tm timeinfo; //variabel yang nyimpen waktu
  int freq; //variabel yang dipake pas ngitung jam makan
  int i; //variabel buat increment
  int nextSchedule; //variabel yang nyimpen jadwal makan berikutnya
  bool bowlFull = false; //variabel yang nyimpen status mangkok penuh apa nggak

  while(1){
    if(!getLocalTime(&timeinfo)){ //Ngambil waktu dari website
      Serial.println("Failed to obtain time");
      return; //Kalo gagal program setelahnya ga dijalanin
    }

    //Loop yang ngitung jam makan, diitung dari frekuensi makan pertama sampai terakhir
    for(freq = 1; freq <= eatingFrequency; freq++){
      
      //Nyimpen jadwal makan berikutnya
      nextSchedule = eatingStart + (eatingInterval * freq);

      //Kalo jamnya sama kayak jam makan yang diatur, waktunya makan :D
      if(timeinfo.tm_hour == eatingStart || timeinfo.tm_hour == nextSchedule){
        Serial.println("Eating Time!");
        //Kalo mangkoknya belum penuh, dan timbangan belum setara porsi yang diatur
        if(scaleReading < eatingPortion && !bowlFull){
          //Ngubah variabel isPouring jadi TRUE biar motornya gerak
          isPouring = true;   
        }else{ //Kalo mangkok penuh
          Serial.println("Bowl is Full!");                                   
          isPouring = false; //Ngubah variabel isPouring jadi FALSE biar motornya berhenti
          bowlFull = true; //Ngubah variabel bowlFull jadi TRUE
        }
        break;
      }else{ //Kalo jam sekarang beda sama jam makan
        bowlFull = false; //Ngubah variabel bowlFull jadi FALSE

        //Error handling 
        if((timeinfo.tm_hour - nextSchedule) % eatingInterval != 0){
          isPouring = false;
        }else{
          isPouring = true;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// Fungsi buat nge-run aplikasinya berulang2
void loop(){
  Blynk.run(); 
}