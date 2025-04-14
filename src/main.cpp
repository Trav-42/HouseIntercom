// // // Intercom all-in-one code



/*
  Recording/Microphone code based on ESP32 mDNS responder sample

  This is an example of an HTTP server that is accessible
  via http://esp32.local URL thanks to mDNS responder.

  Instructions:
  - Update WiFi SSID and password as necessary.
  - Flash the sketch to the ESP32 board
  - Install host software:
    - For Linux, install Avahi (http://avahi.org/).
    - For Windows, install Bonjour (http://www.apple.com/support/bonjour/).
    - For Mac OSX and iOS support is built in through Bonjour already.
  - Point your browser to http://esp32.local, you should see a response.

 */

 #include <M5StickCPlus2.h>
 static constexpr const size_t record_number     = 30;
 static constexpr const size_t record_length     = 60;
 static constexpr const size_t record_size       = record_number * record_length;
 static constexpr const size_t record_samplerate = 5000;
 static uint8_t prev_y[record_length];
 static uint8_t prev_h[record_length];
 static uint8_t rec_record_idx  = 2;
 static size_t draw_record_idx = 0;
 static uint8_t received_rec_idx  = 2;
 
 static uint8_t *transmitted_data;
 static uint8_t *rec_data;
 #define PIN_CLK  0
 #define PIN_DATA 34
 
 


#include "WiFi.h"
#include "AsyncUDP.h"

const char* ssid = "**********";
const char* password = "**********";

AsyncUDP udp;
AsyncUDPMessage asyncMessage;








void setup()
{

    // // // microphone and speaker section
    auto cfg = M5.config();

    // Code from workingSpeakerFile.cpp example from M5Unified library
    // If you want to play sound from HAT Speaker2, write this
    cfg.external_speaker.hat_spk2       = true;

    StickCP2.begin(cfg);
    StickCP2.Display.startWrite();
    StickCP2.Display.setRotation(1);
    StickCP2.Display.setTextDatum(top_center);
    StickCP2.Display.setTextColor(WHITE);
    StickCP2.Display.setFont(&fonts::FreeSansBoldOblique12pt7b);

    // Setup storage to record audio
    rec_data = (typeof(rec_data))heap_caps_malloc(record_size *
                                                    sizeof(uint8_t),
                                                MALLOC_CAP_8BIT);
    memset(rec_data, 0, record_size * sizeof(uint8_t));
    
    // Setup storage to receive data sent via WiFi
    transmitted_data = (typeof(transmitted_data))heap_caps_malloc(record_size *
        sizeof(uint8_t),
    MALLOC_CAP_8BIT);
    memset(transmitted_data, 0, record_size * sizeof(uint8_t));

    StickCP2.Speaker.setVolume(255);

    /// Since the microphone and speaker cannot be used at the same time,
    // turn off the speaker here.
    StickCP2.Speaker.end();
    StickCP2.Mic.begin();

    StickCP2.Display.drawString("Intercom", 120, 3);

    // // // mUDP section

    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("WiFi Failed");
        delay(1000);
    }
    Serial.println("WiFi connected");
    StickCP2.Display.drawString("WiFi connected", 120, 50);
}








void loop()
{
    // // // Server side - to play received audio
    if(udp.listenMulticast(IPAddress(239,1,2,3), 1234)) {
        udp.onPacket([](AsyncUDPPacket packet) {
            Serial.println("\nUDP Packet Type: ");
            Serial.print(packet.isBroadcast()?"Broadcast":packet.isMulticast()?"Multicast":"Unicast");
            Serial.print(", From: ");
            Serial.print(packet.remoteIP());
            Serial.print(":");
            Serial.print(packet.remotePort());
            Serial.print(", To: ");
            Serial.print(packet.localIP());
            Serial.print(":");
            Serial.print(packet.localPort());
            Serial.print(", Length: ");
            Serial.print(packet.length());
            Serial.println();

            // Receive rec_idx
            if (packet.length() == 1) {
                received_rec_idx = *packet.data(); // If the size is 1 assume the data is the new rec_idx
            }
            else {
                if (StickCP2.Speaker.isEnabled()) {

                    StickCP2.Display.clear();
                    while (StickCP2.Mic.isRecording()) {
                        delay(1);
                    }
                    /// Since the microphone and speaker cannot be used at the same
                    /// time, turn off the microphone here.
                    StickCP2.Mic.end();
                    StickCP2.Speaker.isEnabled();
                    StickCP2.Speaker.begin();

                    StickCP2.Display.fillTriangle(70 - 8, 15 - 8, 70 - 8, 15 + 8,
                                                70 + 8, 15, 0x1c9f);
                    StickCP2.Display.drawString("PLAY", 120, 3);
                    int start_pos = received_rec_idx * record_length;
                    transmitted_data = packet.data();
                    if (start_pos < record_size) {
                        StickCP2.Speaker.playRaw(&transmitted_data[start_pos],
                                                record_size - start_pos,
                                                record_samplerate, false, 1, 0);
                    }
                    if (start_pos > 0) {
                        StickCP2.Speaker.playRaw(transmitted_data, start_pos, record_samplerate,
                                                false, 1, 0);
                    }
                    do {
                        delay(1);
                        StickCP2.update();
                    } while (StickCP2.Speaker.isPlaying());

                    /// Since the microphone and speaker cannot be used at the same
                    /// time, turn off the speaker here.
                    StickCP2.Speaker.end();
                    StickCP2.Mic.begin();

                    StickCP2.Display.clear();
                    StickCP2.Display.fillCircle(70, 15, 8, RED);
                    StickCP2.Display.drawString("REC", 120, 3);
                }
            }
        });
    }




     
     // // // // // // Audio section
     StickCP2.update();
 

     // // // record audio if BtnA held
     if (StickCP2.BtnA.isHolding()) {// Clear the screen from the original text before adding the "REC" text and red dot
         StickCP2.Display.clear();
     }
     while (StickCP2.BtnA.isHolding()) { // Record while BtnA is pressed, then can send (or play) the message with button B
        if (StickCP2.Mic.isEnabled()) {
            static constexpr int shift = 6;
            auto data                  = &rec_data[rec_record_idx * record_length];
            if (StickCP2.Mic.record(data, record_length, record_samplerate)) {
                data = &rec_data[draw_record_idx * record_length];
 
                int32_t w = StickCP2.Display.width();
                if (w > record_length - 1) {
                    w = record_length - 1;
                }
                for (int32_t x = 0; x < w; ++x) {
                    StickCP2.Display.writeFastVLine(x, prev_y[x], prev_h[x],
                                                    TFT_BLACK);
                    int32_t y1 = (data[x] >> shift);
                    int32_t y2 = (data[x + 1] >> shift);
                    if (y1 > y2) {
                        int32_t tmp = y1;
                        y1          = y2;
                        y2          = tmp;
                    }
                    int32_t y = ((StickCP2.Display.height()) >> 1) + y1;
                    int32_t h = ((StickCP2.Display.height()) >> 1) + y2 + 1 - y;
                    prev_y[x] = y;
                    prev_h[x] = h;
                    StickCP2.Display.writeFastVLine(x, prev_y[x], prev_h[x], WHITE);
                }
 
                // Re-display the "REC" text and red dot (so the moving audio data line does not put holes into it)
                StickCP2.Display.display();
                StickCP2.Display.fillCircle(70, 15, 8, RED);
                StickCP2.Display.drawString("REC", 120, 3);
                
                if (++draw_record_idx >= record_number) {
                    draw_record_idx = 0;
                }
                if (++rec_record_idx >= record_number) {
                    rec_record_idx = 0;
                }
            }
        }
        StickCP2.update(); // update to see if BtnA is still held
    }


    // // // Play back recorded audio if BtnA clicked
    if (StickCP2.BtnA.wasClicked()) {
        if (StickCP2.Speaker.isEnabled()) {
            StickCP2.Display.clear();
            while (StickCP2.Mic.isRecording()) {
                delay(1);
            }
            /// Since the microphone and speaker cannot be used at the same
            /// time, turn off the microphone here.
            StickCP2.Mic.end();

            StickCP2.Speaker.begin();
            StickCP2.Display.fillTriangle(70 - 8, 15 - 8, 70 - 8, 15 + 8,
                                          70 + 8, 15, 0x1c9f);
            StickCP2.Display.drawString("PLAY", 120, 3);
            int start_pos = rec_record_idx * record_length;
            if (start_pos < record_size) {
                StickCP2.Speaker.playRaw(&rec_data[start_pos],
                                         record_size - start_pos,
                                         record_samplerate, false, 1, 0);
            }
            if (start_pos > 0) {
                StickCP2.Speaker.playRaw(rec_data, start_pos, record_samplerate,
                                         false, 1, 0);
            }
            do {
                delay(1);
                StickCP2.update();
            } while (StickCP2.Speaker.isPlaying());

            /// Since the microphone and speaker cannot be used at the same
            /// time, turn off the speaker here.
            StickCP2.Speaker.end();

            // turn the microphone back on
            StickCP2.Mic.begin();

            StickCP2.Display.clear();
            StickCP2.Display.drawString("Ready to Record", 120, 40);
        }
    }



    // // // change noise filter level if BtnB held
     if (StickCP2.BtnB.wasHold()) {
         auto cfg               = StickCP2.Mic.config();
         cfg.noise_filter_level = (cfg.noise_filter_level + 8) & 255;
         StickCP2.Mic.config(cfg);
         StickCP2.Display.clear();
         StickCP2.Display.fillCircle(70, 15, 8, GREEN);
         StickCP2.Display.drawString("NF:" + String(cfg.noise_filter_level), 120,
                                     3);
         StickCP2.Display.drawString("Client", 120, 25);
     }


     // // // Broadcast data if BtnB clicked
      else if (StickCP2.BtnB.wasClicked()) {
         if (StickCP2.Speaker.isEnabled()) {
            StickCP2.Display.clear();
            while (StickCP2.Mic.isRecording()) {
                delay(1);
            }
            /// Since the microphone and speaker cannot be used at the same
            /// time, turn off the microphone here.
            StickCP2.Mic.end();

            StickCP2.Speaker.begin();
            StickCP2.Display.drawString("Sending data", 120, 3);
            int start_pos = rec_record_idx * record_length;

            // // // Server stuff (to send received audio)
            // Broadcast the first 1436 elements of the audio array on port 1234
            udp.broadcastTo(&rec_record_idx, sizeof(rec_record_idx), 1234);
            udp.broadcastTo(rec_data, record_size, 1234);

            /// Since the microphone and speaker cannot be used at the same
            /// time, turn off the speaker here.
            StickCP2.Speaker.end();

            // turn the microphone back on
            StickCP2.Mic.begin();

            StickCP2.Display.clear();
            StickCP2.Display.drawString("Ready to Record", 120, 40);
         }
     }
 }