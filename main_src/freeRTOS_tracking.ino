// SIM card PIN empty normally
const char simPIN[]   = ""; 

// Definitions
#define SMS_TARGET  "+50230815818"
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb
#define I2C_SDA_2            21
#define I2C_SCL_2            22
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define DATA_UPLOAD          25

// Handlers
TaskHandle_t h_timestamp_begin= NULL;
TaskHandle_t h_LSM6DSOX_data = NULL;

// Libraries
#include <Wire.h>
#include <TinyGsmClient.h>
#include <Adafruit_LSM6DSOX.h>
#define SerialMon Serial

// Set serial for AT commands (to SIM800 module)
#define SerialAT  Serial1
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

// Cores on ESP32
static const BaseType_t pro_cpu = 0;
static const BaseType_t app_cpu = 1;

// Objects 
Adafruit_LSM6DSOX sox;

// Globals
char  clock_time_stamp[40];
String cclk;
String temp_cclk;

void t_timestamp_begin(void *parameter) {
  while(1) {
    get_time_stamp();
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    vTaskResume(h_LSM6DSOX_data);
  }
}

void t_LSM6DSOX_data_log(void *parameter) {
  vTaskSuspend(h_LSM6DSOX_data); 
  while (1) {
    //Get a new normalized sensor event 
    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    sox.getEvent(&accel, &gyro, &temp);
    Serial.print(temp_cclk);
    Serial.print("\t");
    Serial.print(temp.temperature);
    Serial.print("\t");
    Serial.print(accel.acceleration.x);
    Serial.print("\t");
    Serial.print(accel.acceleration.y);
    Serial.print("\t");
    Serial.println(accel.acceleration.z);
    vTaskDelay(5000 / portTICK_PERIOD_MS);  
  }
}

void get_time_stamp(){
  modem.sendSMS(SMS_TARGET, " ");
  modem.sendAT(GF("+CNTPCID=1"));
  modem.waitResponse();
  modem.sendAT(GF("+CNTP=\"200.160.7.186\",-12"));
  modem.waitResponse();
  modem.sendAT(GF("+CNTP"));
  modem.waitResponse(200, GF("+CNTP: 1" GSM_NL));
  modem.sendAT(GF("+CCLK?"));
  modem.waitResponse(GF(GSM_NL));
  cclk = modem.stream.readStringUntil('\n');
  temp_cclk = cclk.substring(8,28);
  modem.waitResponse();
}

void setup() {
  SerialMon.begin(115200);
  Wire.begin(I2C_SDA_2, I2C_SCL_2);
  if (!sox.begin_I2C()) {
    Serial.println("Failed to find LSM6DSOX chip");
  }
  pinMode(13, OUTPUT);
  sox.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  sox.setGyroRange(LSM6DS_GYRO_RANGE_250_DPS);
  sox.setAccelDataRate(LSM6DS_RATE_833_HZ);
  sox.setGyroDataRate(LSM6DS_RATE_12_5_HZ);
  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);
  pinMode(DATA_UPLOAD, INPUT);
  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  vTaskDelay(3000 / portTICK_PERIOD_MS);
  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  modem.init();
  // modem.restart();
  // use modem.init() if you don't need the complete restart
  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }
  // To send an SMS, call modem.sendSMS(SMS_TARGET, smsMessage)
  get_time_stamp();
  Serial.print("");
  Serial.print("timestamp");
  Serial.print("\t temp");
  Serial.print("\t x");
  Serial.print("\t y ");
  Serial.print("\t z ");
  Serial.println("\t sensor_id");
  // Tasks to run 
  xTaskCreatePinnedToCore(
  t_LSM6DSOX_data_log,     // Function to be called
  "Log the data of sensor",  // Name of task
  2048,      // Stack size (bytes in ESP32, words in FreeRTOS)
  NULL,      // Parameter to pass to function
  2,         // Task priority (0 to configMAX_PRIORITIES - 1)
  &h_LSM6DSOX_data,  // Task handle
  pro_cpu);  // Core to run
  xTaskCreatePinnedToCore(t_timestamp_begin, "begin operations with time present", 2048, NULL, 4, &h_timestamp_begin, app_cpu);
  vTaskDelete(NULL);
}

void loop() {
}