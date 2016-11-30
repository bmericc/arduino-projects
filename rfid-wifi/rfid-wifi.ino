/*
 * Typical pin layout used:
 * ------------------------------------------------------------
 *             MFRC522      Arduino       Arduino   Arduino
 *             Reader/PCD   Uno           Mega      Nano v3
 * Signal      Pin          Pin           Pin       Pin
 * ------------------------------------------------------------
 * RST/Reset   RST          9             5         D9
 * SPI SS      SDA(SS)      10            53        D10
 * SPI MOSI    MOSI         11 / ICSP-4   51        D11
 * SPI MISO    MISO         12 / ICSP-1   50        D12
 * SPI SCK     SCK          13 / ICSP-3   52        D13
 */


#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"
#include <MFRC522.h>

// These are the interrupt and control pins
#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
// These can be any two pins
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIVIDER); // you can change this clock speed

#define WLAN_SSID       "kalehost"           // cannot be longer than 32 characters!
#define WLAN_PASS       "62davsan"
// Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data 
                                   // received before closing the connection.  If you know the server
                                   // you're accessing is quick to respond, you can reduce this value.

// What page to grab!
#define WEBSITE      "192.168.1.80"

#define RST_PIN    9   // 
#define SS_PIN    8    //

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

/**************************************************************************/
/*!
    @brief  Sets up the HW and the CC3000 module (called automatically
            on startup)
*/
/**************************************************************************/

uint32_t ip = cc3000.IP2U32(192,168,1,80);

int loopCounter = 0;  

void setup(void)
{
  Serial.begin(115200);

  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  ShowReaderDetails();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Wifi RF Reader!\n")); 

  Serial.print("Free RAM: "); Serial.println(getFreeRam(), DEC);
  
  /* Initialise the module */
  Serial.println(F("\nInitializing..."));
  if (!cc3000.begin())
  {
    Serial.println(F("Couldn't begin()! Check your wiring?"));
    while(1);
  }
  
  // Optional SSID scan
  // listSSIDResults();
  
  Serial.print(F("\nAttempting to connect to ")); Serial.println(WLAN_SSID);
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed!"));
    while(1);
  }
   
  Serial.println(F("Connected!"));
  
  /* Wait for DHCP to complete */
  Serial.println(F("Request DHCP"));
  while (!cc3000.checkDHCP())
  {
    delay(100); // ToDo: Insert a DHCP timeout!
  }  

  /* Display the IP address DNS, Gateway, etc. */  
  while (! displayConnectionDetails()) {
    delay(1000);
  }


  Serial.println("Scan PICC to see UID, type, and data blocks...");
  
}



void loop() {

    bool progress = false;

    if( loopCounter == 0 ) {
     delay(1000);
    }

    // Prepare key - all keys are set to FFFFFFFFFFFFh at chip delivery from the factory.
    MFRC522::MIFARE_Key key;
    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;  
    
    // Look for new cards
    if ( ! mfrc522.PICC_IsNewCardPresent())  {    Serial.println("Waiting");    return;  }

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())   {    Serial.println("ReadCardSerial");    return;  }
    
    String uid =  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    
    Serial.print("Card UID:");
    Serial.println(uid);
  
    progress = sendCardId(uid);
    Serial.print("Validating user: ");
    if(!progress){
        Serial.println("OK ");
    }else{
        Serial.println("Fail");          
    } 
    
    Serial.println();        

}

String dump_byte_array(byte *buffer, byte bufferSize) {
    String uid;
    
    for (byte i = 0; i < bufferSize; i++) {
       /* Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);      */        
        uid += buffer[i] < 0x10 ? "-0" : "-";
        uid += buffer[i];
    }
    
    return uid;
}

bool sendCardId(String uid) {

  Adafruit_CC3000_Client client = cc3000.connectTCP(ip, 80);
  if (client.connected()) {
    
    client.fastrprint(F("POST /rfid/index.php HTTP/1.1\r\n"));    
    client.fastrprint(F("Host: ")); 
    client.fastrprint(WEBSITE); 
    client.fastrprint(F("\r\n"));
    client.fastrprint(F("Content-Type: application/x-www-form-urlencoded\r\n"));
    String body = "uid=" + uid;
    client.fastrprint(F("Content-Length: ")); 
    client.print(body.length());      
    client.fastrprint(F("\r\n"));
    client.fastrprint(F("Connection: close\r\n"));
    client.println();
    client.println(body); 
    client.fastrprint(F("\r\n"));
    client.println();
    
  } else {
    Serial.println(F("Connection failed"));    
    return false;
  }

  Serial.println(F("-------------------------------------"));
  Serial.println(F("Awaiting response..."));
  
  unsigned long lastRead = millis();
  while (client.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
    while (client.available()) {
      char c = client.read();
      if(c == '{')   {   client.close(); return true; }
      lastRead = millis();
    }
  }
  client.close();
  return false;
}


/**************************************************************************/
/*!
    @brief  Begins an SSID scan and prints out all the visible networks
*/
/**************************************************************************/

/*
void listSSIDResults(void)
{
  uint32_t index;
  uint8_t valid, rssi, sec;
  char ssidname[33]; 

  if (!cc3000.startSSIDscan(&index)) {
    Serial.println(F("SSID scan failed!"));
    return;
  }

  Serial.print(F("Networks found: ")); Serial.println(index);
  Serial.println(F("================================================"));

  while (index) {
    index--;

    valid = cc3000.getNextSSID(&rssi, &sec, ssidname);
    
    Serial.print(F("SSID Name    : ")); Serial.print(ssidname);
    Serial.println();
    Serial.print(F("RSSI         : "));
    Serial.println(rssi);
    Serial.print(F("Security Mode: "));
    Serial.println(sec);
    Serial.println();
  }
  Serial.println(F("================================================"));

  cc3000.stopSSIDscan();
}
*/

/**************************************************************************/
/*!
    @brief  Tries to read the IP address and other connection details
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print("MFRC522 Software Version: 0x");
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(" = v1.0");
  else if (v == 0x92)
    Serial.print(" = v2.0");
  else
    Serial.print(" (unknown)");
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println("WARNING: Communication failure, is the MFRC522 properly connected?");
  }
}

