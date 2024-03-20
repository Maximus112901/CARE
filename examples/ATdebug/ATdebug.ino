// SGP30 ===========================================================================================================
#include "Arduino.h"
#include <Wire.h>
#include "Adafruit_SGP30.h"

Adafruit_SGP30 sgp;

// SPS30 ===========================================================================================================
#include <sps30.h>
#define SP30_COMMS Wire
#define DEBUG_ 0
// function prototypes (sometimes the pre-processor does not create prototypes themself on ESPxx)
void serialTrigger(char *mess);
void ErrtoMess(char *mess, uint8_t r);
void Errorloop(char *mess, uint8_t r);
void GetDeviceInfo();
// create constructor
SPS30 sps30;

void GetDeviceInfo()
{
    char buf[32];
    uint8_t ret;
    SPS30_version v;

    // try to read serial number
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == SPS30_ERR_OK)
    {
        Serial.print(F("Serial number : "));
        if (strlen(buf) > 0)
            Serial.println(buf);
        else
            Serial.println(F("not available"));
    }
    else
        ErrtoMess((char *)"could not get serial number. ", ret);

    // try to get product name
    ret = sps30.GetProductName(buf, 32);
    if (ret == SPS30_ERR_OK)
    {
        Serial.print(F("Product name  : "));

        if (strlen(buf) > 0)
            Serial.println(buf);
        else
            Serial.println(F("not available"));
    }
    else
        ErrtoMess((char *)"could not get product name. ", ret);

    // try to get version info
    ret = sps30.GetVersion(&v);
    if (ret != SPS30_ERR_OK)
    {
        Serial.println(F("Can not read version info."));
        return;
    }

    Serial.print(F("Firmware level: "));
    Serial.print(v.major);
    Serial.print(".");
    Serial.println(v.minor);

    Serial.print(F("Library level : "));
    Serial.print(v.DRV_major);
    Serial.print(".");
    Serial.println(v.DRV_minor);
}

/**
 * @brief : read and display all values
 */
bool read_all()
{
    static bool header = true;
    uint8_t ret, error_cnt = 0;
    struct sps_values val;

    // loop to get data
    do
    {

#ifdef USE_50K_SPEED                // update 1.4.3
        SP30_COMMS.setClock(50000); // set to 50K
        ret = sps30.GetValues(&val);
        SP30_COMMS.setClock(100000); // reset to 100K in case other sensors are on the same I2C-channel
#else
        ret = sps30.GetValues(&val);
#endif

        // data might not have been ready
        if (ret == SPS30_ERR_DATALENGTH)
        {

            if (error_cnt++ > 3)
            {
                ErrtoMess((char *)"Error during reading values: ", ret);
                return (false);
            }
            delay(1000);
        }

        // if other error
        else if (ret != SPS30_ERR_OK)
        {
            ErrtoMess((char *)"Error during reading values: ", ret);
            return (false);
        }

    } while (ret != SPS30_ERR_OK);

    // only print header first time
    if (header)
    {
        Serial.println(F("-------------Mass -----------    ------------- Number --------------   -Average-"));
        Serial.println(F("     Concentration [μg/m3]             Concentration [#/cm3]             [μm]"));
        Serial.println(F("P1.0\tP2.5\tP4.0\tP10\tP0.5\tP1.0\tP2.5\tP4.0\tP10\tPartSize\n"));
        header = false;
    }

    Serial.print(val.MassPM1);
    Serial.print(F("\t"));
    Serial.print(val.MassPM2);
    Serial.print(F("\t"));
    Serial.print(val.MassPM4);
    Serial.print(F("\t"));
    Serial.print(val.MassPM10);
    Serial.print(F("\t"));
    Serial.print(val.NumPM0);
    Serial.print(F("\t"));
    Serial.print(val.NumPM1);
    Serial.print(F("\t"));
    Serial.print(val.NumPM2);
    Serial.print(F("\t"));
    Serial.print(val.NumPM4);
    Serial.print(F("\t"));
    Serial.print(val.NumPM10);
    Serial.print(F("\t"));
    Serial.print(val.PartSize);
    Serial.print(F("\n"));

    return (true);
}

/**
 *  @brief : continued loop after fatal error
 *  @param mess : message to display
 *  @param r : error code
 *
 *  if r is zero, it will only display the message
 */
void Errorloop(char *mess, uint8_t r)
{
    if (r)
        ErrtoMess(mess, r);
    else
        Serial.println(mess);
    Serial.println(F("Program on hold"));
    for (;;)
        delay(100000);
}

/**
 *  @brief : display error message
 *  @param mess : message to display
 *  @param r : error code
 *
 */
void ErrtoMess(char *mess, uint8_t r)
{
    char buf[80];

    Serial.print(mess);

    sps30.GetErrDescription(r, buf, 80);
    Serial.println(buf);
}

/**
 * serialTrigger prints repeated message, then waits for enter
 * to come in from the serial port.
 */
void serialTrigger(char *mess)
{
    Serial.println();

    while (!Serial.available())
    {
        Serial.println(mess);
        delay(2000);
    }

    while (Serial.available())
        Serial.read();
}

// GPS =============================================================================================================
#include "utilities.h"

#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG SerialMon

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// SD Card Definitions and Functions =================================================================================================================================
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "utilities.h"

#if defined(LILYGO_T_CALL_A7670_V1_0) || defined(LILYGO_T_A7608X_DC_S3)
#error "This board not SD slot"
#endif

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.path(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char *path)
{
    Serial.printf("Removing Dir: %s\n", path);
    if (fs.rmdir(path))
    {
        Serial.println("Dir removed");
    }
    else
    {
        Serial.println("rmdir failed");
    }
}

void readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    while (file.available())
    {
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("File written");
    }
    else
    {
        Serial.println("Write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file)
    {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message))
    {
        Serial.println("Message appended");
    }
    else
    {
        Serial.println("Append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if (fs.rename(path1, path2))
    {
        Serial.println("File renamed");
    }
    else
    {
        Serial.println("Rename failed");
    }
}

void deleteFile(fs::FS &fs, const char *path)
{
    Serial.printf("Deleting file: %s\n", path);
    if (fs.remove(path))
    {
        Serial.println("File deleted");
    }
    else
    {
        Serial.println("Delete failed");
    }
}

void testFileIO(fs::FS &fs, const char *path)
{
    File file = fs.open(path);
    static uint8_t buf[512];
    size_t len = 0;
    uint32_t start = millis();
    uint32_t end = start;
    if (file)
    {
        len = file.size();
        size_t flen = len;
        start = millis();
        while (len)
        {
            size_t toRead = len;
            if (toRead > 512)
            {
                toRead = 512;
            }
            file.read(buf, toRead);
            len -= toRead;
        }
        end = millis() - start;
        Serial.printf("%u bytes read for %u ms\n", flen, end);
        file.close();
    }
    else
    {
        Serial.println("Failed to open file for reading");
    }

    file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("Failed to open file for writing");
        return;
    }

    size_t i;
    start = millis();
    for (i = 0; i < 2048; i++)
    {
        file.write(buf, 512);
    }
    end = millis() - start;
    Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
    file.close();
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(10);
    } // Wait for serial console to open!

    // SPS30 ========================================================================================================
    // set driver debug level
    sps30.EnableDebugging(DEBUG_);

    // Begin communication channel
    SP30_COMMS.begin();

    if (sps30.begin(&SP30_COMMS) == false)
    {
        Errorloop((char *)"SPS30 could not set I2C communication channel.", 0);
    }

    // check for SPS30 connection
    if (!sps30.probe())
        Errorloop((char *)"SPS30 NOT detected!", 0);
    else
        Serial.println(F("SPS30 connected successfully!"));

    // reset SPS30 connection
    if (!sps30.reset())
        Errorloop((char *)"SPS30 could not reset.", 0);

    // read device info
    GetDeviceInfo();

    // start measurement
    if (sps30.start())
        Serial.println(F("SPS30 woke up successfully!"));
    else
        Errorloop((char *)"SPS30 could not start measurements.", 0);

    // serialTrigger((char *)"Hit <enter> to continue reading.");

    if (sps30.I2C_expect() == 4)
        Serial.println(F(" !!! Due to I2C buffersize only the SPS30 MASS concentration is available!\n"));

    // SGP30 ========================================================================================================
    Serial.println("SGP30 test");

    if (!sgp.begin())
    {
        Serial.println("Sensor not found :(");
        while (1)
            ;
    }
    Serial.print("Found SGP30 serial #");
    Serial.print(sgp.serialnumber[0], HEX);
    Serial.print(sgp.serialnumber[1], HEX);
    Serial.println(sgp.serialnumber[2], HEX);

    // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
    // sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!

    // GPS ==========================================================================================================
    // Turn on DC boost to power on the modem
#ifdef BOARD_POWERON_PIN
    pinMode(BOARD_POWERON_PIN, OUTPUT);
    digitalWrite(BOARD_POWERON_PIN, HIGH);
#endif

    // Set modem reset pin ,reset modem
    pinMode(MODEM_RESET_PIN, OUTPUT);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);
    delay(100);
    digitalWrite(MODEM_RESET_PIN, MODEM_RESET_LEVEL);
    delay(2600);
    digitalWrite(MODEM_RESET_PIN, !MODEM_RESET_LEVEL);

    // Turn on modem
    pinMode(BOARD_PWRKEY_PIN, OUTPUT);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(BOARD_PWRKEY_PIN, HIGH);
    delay(1000);
    digitalWrite(BOARD_PWRKEY_PIN, LOW);

    // Set modem baud
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
    Serial.println("Enabling GPS/GNSS/GLONASS");
    while (!modem.enableGPS(MODEM_GPS_ENABLE_GPIO))
    {
        Serial.print(".");
    }
    Serial.println();
    Serial.println("GPS Enabled");

    // SD Card ======================================================================================================
    SPI.begin(BOARD_SCK_PIN, BOARD_MISO_PIN, BOARD_MOSI_PIN);
    if (!SD.begin(BOARD_SD_CS_PIN))
    {
        Serial.println("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    }
    else
    {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD Card Size: %lluMB\n", cardSize);

    // set init to true if you want a new set of measurements
    bool init = false;
    if (init)
    {
        // Initialize Contents
        deleteFile(SD, "/test.txt");
        writeFile(SD, "/test.txt", "Hello \n");
        // delay(1000000);
        // disconnect board and then set init to false
    }
    readFile(SD, "/test.txt");
}

void loop()
{
    String packet = "";

    // SGP30 TVOC ppb, eCO2 ppm
    if (!sgp.IAQmeasure())
    {
        Serial.println("Measurement failed");
        return;
    }
    Serial.print("TVOC ");
    Serial.print(sgp.TVOC);
    Serial.print(" ppb\t");
    Serial.print("eCO2 ");
    Serial.print(sgp.eCO2);
    Serial.print(" ppm\t");

    packet += String(sgp.TVOC);
    packet += ",";
    packet += String(sgp.eCO2);
    packet += ",";

    // SPS30 PM2.5 ug/m^3
    Serial.print("PM2.5 ");
    float pm25 = sps30.GetMassPM2();
    Serial.print(String(pm25));
    Serial.print(" ug/m^3\t");
    packet += String(pm25);

    // GPS
    float lat2 = 0;
    float lon2 = 0;
    float speed2 = 0;
    float alt2 = 0;
    int vsat2 = 0;
    int usat2 = 0;
    float accuracy2 = 0;
    int year2 = 0;
    int month2 = 0;
    int day2 = 0;
    int hour2 = 0;
    int min2 = 0;
    int sec2 = 0;

    uint8_t fixMode = 0;
    // Serial.println("Requesting current GPS/GNSS/GLONASS location");
    if (modem.getGPS(&fixMode, &lat2, &lon2, &speed2, &alt2, &vsat2, &usat2, &accuracy2,
                     &year2, &month2, &day2, &hour2, &min2, &sec2))
    {
        // Serial.print("FixMode:");
        // Serial.println(fixMode);
        packet += String(fixMode);
        packet += ",";

        // Serial.print("Latitude: ");
        // Serial.print(lat2, 6);
        packet += String(lat2);
        packet += ",";

        // Serial.print("\tLongitude:");
        // Serial.println(lon2, 6);
        packet += String(lon2);
        packet += ",";

        // Serial.print("Speed:");
        // Serial.print(speed2);
        packet += String(speed2);
        packet += ",";

        // Serial.print("\tAltitude:");
        Serial.println(alt2);
        packet += String(alt2);
        packet += ",";

        // Serial.print("Visible Satellites:");
        // Serial.print(vsat2);
        packet += String(vsat2);
        packet += ",";

        // Serial.print("\tUsed Satellites:");
        // Serial.println(usat2);
        packet += String(usat2);
        packet += ",";

        // Serial.print("Accuracy:");
        // Serial.println(accuracy2);
        packet += String(accuracy2);
        packet += ",";

        // Serial.print("Year:");
        // Serial.print(year2);
        packet += String(year2);
        packet += ",";

        // Serial.print("\tMonth:");
        // Serial.print(month2);
        packet += String(month2);
        packet += ",";

        // Serial.print("\tDay:");
        // Serial.println(day2);
        packet += String(day2);
        packet += ",";

        // Serial.print("Hour:");
        // Serial.print(hour2);
        packet += String(hour2);
        packet += ",";

        // Serial.print("\tMinute:");
        // Serial.print(min2);
        packet += String(min2);
        packet += ",";

        // Serial.print("\tSecond:");
        // Serial.println(sec2);
        packet += String(sec2);
        packet += "\n";
    }
    else
    {
        // Serial.print(" Couldn't get GPS/GNSS/GLONASS location, retrying again.");
        // Serial.println();
        packet += ",N/A\n";
    }

    Serial.println();
    Serial.println("Packet = " + packet);
    const char *charPacket = packet.c_str();
    appendFile(SD, "/test.txt", charPacket);
    Serial.println();
    // Serial.println("Contents of SD Card: ");
    // readFile(SD, "/test.txt");
    // Serial.println();
    delay(5000);
}
