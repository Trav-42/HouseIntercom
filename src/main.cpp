// // // Server code


/*
  ESP32 mDNS responder sample

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
#include "WiFi.h"
#include "AsyncUDP.h"
#include <M5StickCPlus2.h>

static constexpr const size_t record_number     = 200;
static constexpr const size_t record_length     = 240;
static constexpr const size_t record_size       = record_number * record_length;
static constexpr const size_t record_samplerate = 10000;

static uint8_t prev_y[record_length];
static uint8_t prev_h[record_length];
static uint8_t rec_record_idx  = 2; // changed from size_t to uint8_t so it can be transmitted via UDP
static size_t draw_record_idx = 0;
static uint8_t received_rec_idx  = 2;
static uint8_t *rec_data;

AsyncUDP udp;
#define PIN_CLK  0
#define PIN_DATA 34
const char* ssid = "********";
const char* password = "********";





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

    rec_data = (typeof(rec_data))heap_caps_malloc(record_size *
                                                    sizeof(uint8_t),
                                                MALLOC_CAP_8BIT);
    memset(rec_data, 0, record_size * sizeof(uint8_t));
    StickCP2.Speaker.setVolume(255);

    /// Since the microphone and speaker cannot be used at the same time,
    // turn off the speaker here.
    StickCP2.Speaker.end();
    StickCP2.Mic.begin();

    StickCP2.Display.fillCircle(70, 15, 8, RED);
    StickCP2.Display.drawString("Server", 120, 3);

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

    // // // Server stuff (to play received audio)
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
            Serial.print(", Data: ");
            Serial.write(packet.data(), packet.length());
            Serial.println();
            //reply to the client
            packet.printf("Got %u bytes of data", packet.length());

            if (packet.length() == 1) {
                received_rec_idx = *packet.data(); // If the size is 1 assume the data is the new rec_idx
                Serial.printf("received_rec_idx = %o", received_rec_idx);
            }
            else {
                Serial.println("Received data array elements: ");
                for (int i = 0; i < packet.length(); i++) {
                    Serial.print(packet.data()[i] + " ");
                }
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
                int start_pos = received_rec_idx * record_length;
                if (start_pos < record_size) {
                    StickCP2.Speaker.playRaw(&packet.data()[start_pos],
                                            record_size - start_pos,
                                            record_samplerate, false, 1, 0);
                }
                if (start_pos > 0) {
                    StickCP2.Speaker.playRaw(packet.data(), start_pos, record_samplerate,
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
        });
    }
}











void loop()
{
    
    // // // Audio section
    StickCP2.update();

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
               
            // Commented out this section on 3/28 - try just overwriting the audio every time you try to record a message
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
    if (StickCP2.BtnB.wasHold()) {
        auto cfg               = StickCP2.Mic.config();
        cfg.noise_filter_level = (cfg.noise_filter_level + 8) & 255;
        StickCP2.Mic.config(cfg);
        StickCP2.Display.clear();
        StickCP2.Display.fillCircle(70, 15, 8, GREEN);
        StickCP2.Display.drawString("NF:" + String(cfg.noise_filter_level), 120,
                                    3);
        StickCP2.Display.drawString("Server", 120, 25);
    }
}
