/*
(External Device) -- (Arduino Uno R3)
(Relay 2 - Unused) -- (D2 output)
(Relay 3 - Move) -- (D3 output)
(Relay 4 - Move) -- (D4 output)
(COGS ISO Input - Idol_Taken) -- (D5 output)
(COGS ISO Output - Raise_Pedestal) -- (D6 input)
(Relay 1 - Lights) -- (D8 output)
(RC522 RST) -- (D9)
(RC522 SDA) -- (D10)
(RC522 MOSI) -- (D11)
(RC522 MISO) -- (D12)
(RC522 SCK) -- (D13)
(RC-522 GND) -- (GND)
(RC-522 3.3v) -- (3.3v)

Idol_Taken Uno output to COGS ISO input
Raise_Pedestal Uno input from COGS ISO output


Start State:
  Pedestal Lowered (D3 LOW, D4 HIGH, lights off)
  RFID not reading
  Idol_Taken signal low

On Raise_Pedestal signal from COGS:
  Pedestal Raises (D3 HIGH, D4 LOW, lights on)
    Start reading for RFID tag
      When RFID fails to read tag for more than 2 seconds, set Idol_Taken signal high for .5 second, turn off lights
      Send controller into indefinite wait until external reset is performed
      
*There is no signal to lower the pedestal from COGS. Resetting the Uno makes the Pedestal start in a closed position. The idol must be placed inside first.
*The reset from COGS will be a COGS ISO output that loops the GND to the RESET pin on the Arduino itself
*If room runner fails to replace idol the next time the pedestal is raised it will automatically trigger the Idol_Taken signal to COGS
*The pedestal takes 21 seconds to raise or lower
*/


#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN); // create MFRC522 instance
bool idolPresent = false;
bool raisePedestalSignalDetected = false;
unsigned long raisePedestalStartTime = 0;
unsigned long lastRFIDDetectionTime = 0;

void setup()
{
  Serial.begin(9600); // initiate serial communication
  SPI.begin();        // initiate SPI bus

  pinMode(3, OUTPUT); // relay 3 - Move Control (LOW = relay is triggered state, board light ON)
  pinMode(4, OUTPUT); // relay 4 - Move Control (LOW = relay is triggered state, board light ON)
  pinMode(5, OUTPUT); // Idol_Taken signal (HIGH to COGS)
  pinMode(6, INPUT_PULLUP); // Raise_Pedestal signal from COGS
  pinMode(8, OUTPUT); // relay 1 - LIGHTS (LOW = relay is triggered state, board light ON)

  lowerPedestal(); // start with the pedestal down on reset
}

void loop()
{
  if (digitalRead(6) == LOW && !raisePedestalSignalDetected)
  {
    raisePedestalSignalDetected = true;
    raisePedestalStartTime = millis();
    raisePedestal(); // raise the pedestal
  }

  if (raisePedestalSignalDetected)
  {
    unsigned long elapsedTime = millis() - raisePedestalStartTime;

    if (elapsedTime >= 21000)
    {
      // Allow 21 seconds for the pedestal to fully raise
      raisePedestalSignalDetected = false; // Reset the flag after the delay
      lastRFIDDetectionTime = millis();    // Update the last RFID detection time

      while (true)
      {
        if (!lookingForIdolRFID())
        {
          handleTimeout(); // Call function to handle timeout
          break; // Exit the loop after handling timeout
        }

        if (idolPresent)
        {
          unsigned long tagCheckStartTime = millis();

          while ((millis() - tagCheckStartTime) <= 2000)
          {
            // Check for RFID tag every 0.5 seconds
            if (!lookingForIdolRFID())
            {
              handleTimeout(); // Call function to handle timeout
              break; // Exit the inner loop after handling timeout
            }

            if (idolPresent)
            {
              lastRFIDDetectionTime = millis(); // Update the last RFID detection time
              break; // Exit the inner loop if the tag is found
            }

            delay(500); // Wait for 0.5 seconds
          }
        }
      }
    }
  }
}

void raisePedestal()
{
  digitalWrite(8, LOW); // turn lights on
  digitalWrite(3, LOW); // part of raise move
  digitalWrite(4, HIGH); // part of raise move
  Serial.println("Pedestal raising.... Lights on.");
  delay(21000); //delay for pedestal to raise
  Serial.println("Pedestal raised and ready.");
}

void lowerPedestal()
{
  digitalWrite(8, HIGH); // turn lights off
  digitalWrite(3, HIGH); // part of lower move
  digitalWrite(4, LOW);   // part of lower move
  Serial.println("Pedestal lowering....");
  delay(21000); //delay for pedestal to lower
  Serial.println("Pedestal lowered and ready.");
}

bool lookingForIdolRFID()
{
  unsigned long startTime = millis();

  while (millis() - startTime <= 1500)
  {
    mfrc522.PCD_Init(); // Initiate MFRC522
    Serial.println("   READY TO READ RFID");

    // Look for new cards
    if (mfrc522.PICC_IsNewCardPresent())
    {
      // Select one of the cards
      if (mfrc522.PICC_ReadCardSerial())
      {
        // Show UID on the serial monitor
        Serial.print("UID tag :");
        String content = "";
        for (byte i = 0; i < mfrc522.uid.size; i++)
        {
          Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
          Serial.print(mfrc522.uid.uidByte[i], HEX);
          content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
          content.concat(String(mfrc522.uid.uidByte[i], HEX));
        }
        Serial.println();
        Serial.print("Message : ");
        content.toUpperCase();

        if (content.substring(1) == "33 98 1E 0E") // this is the correct RFID for the Idol, to add more than one use [if (content.substring(1) == "33 98 1E 0E" || content.substring(1) == "F3 BA DF 99")]
        {
          Serial.println("IDOL IS PRESENT");
          Serial.println();
          idolPresent = true;
          return true;
        }
      }
    }

    delay(500); // Wait for 0.5 seconds
  }

  return false; // Return false if no valid RFID reading is found within the timeout
}

void handleTimeout() // this is what executes whent the Idol is gone for more than 2 seconds
{
  digitalWrite(8, HIGH); // turn lights off
  digitalWrite(5, HIGH); // Set Idol_Taken signal high for .5 seconds
  delay(500);
  digitalWrite(5, LOW); // Reset Idol_Taken signal
  indefiniteWait();       // Go into an indefinite wait
}

void indefiniteWait()
{
  Serial.println("Indefinite wait...");
  while (true)
  {
    // Do nothing, just wait indefinitely
  }
}
