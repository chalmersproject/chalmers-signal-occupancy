void show_wifi_connected()
{
  //show connected on display!
  tft.fillScreen();
  tft.setCursor(10, 50);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("Connected to Internet!");
  tft.println("  to");
  tft.println("  Internet!");
}
void show_chalmers_start()
{
  tft.fillScreen();
  tft.setCursor(16, 50);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.println("Chalmers ");
  tft.println("  Signal!");
}


void update_tft_occupancy(int occupancy, int capacity)
{
  tft.fillScreen();
  tft.setCursor(35, 10);
  tft.setTextColor(WHITE);
  tft.setTextSize(5);
  tft.println(occupancy);
  tft.setCursor(8, 42);
  tft.println("----");
  tft.setCursor(20, 75);
  tft.println(capacity);
}



void update_led_occupancy(int occupancy, int capacity)
{
  // Add entropy to random number generator; we use a lot of it.
  random16_add_entropy( random());
  Fire2012WithPalette(); // run simulation frame, using palette colors

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);
}
