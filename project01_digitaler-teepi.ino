
/* wiring
   Servo
   yellow -> see constructor
   black  -> GND
   red    -> +5V

   display (i2c Address 0x3C; verified with i2c scanner tool)
   GND -> GND
   VCC -> 5V
   SCL -> A5
   SDA -> A4

   buttons
   pin1 -> GND
   pin2 -> see constructor

   music "switch"
   black -> GND
   red -> 3.3V
   brown (signal) -> see constructor
*/

// https://www.arduino.cc/en/Reference/Servo
#include <Servo.h>

// https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x64_i2c/ssd1306_128x64_i2c.ino
// https://joy-it.net/files/files/Produkte/SBC-OLED01/Anleitung-SBC-OLED01.pdf
#include <Adafruit_SSD1306.h>


enum class DipMode : int
{
  SIMPLY_DOWN = 0,
  JIGGLE,
  NO_DIPPING,
  NUMBER_OF_DIP_MODES,
};


struct LcdDisplay {
  Adafruit_SSD1306 display_driver;
  char print_time_buffer[15];  // sufficient space for time + "\neinst"
  char print_dip_mode_buffer[50];
  int start_text_current_line = 0;
  static const int number_of_start_text_lines = 3;
  const char* start_text_lines[number_of_start_text_lines] = {
    "1.\nTeebeutel\nbefestigen",
    "2.\nZeit\neinstellen",
    "3.\nTimer\nstarten"
  };
  const char* dip_mode_description[static_cast<int>(DipMode::NUMBER_OF_DIP_MODES)] = {
    "Eintauchen",
    "Schwenken",
    "Nicht!",
  };
  bool is_display_inverted = false;

  LcdDisplay()
    : display_driver(128, 64)
  {}

  void init() {
    display_driver.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  }

  void demo() {
  }

  void show_line(const char * line, int text_size = 2)
  {
    is_display_inverted = false;
    display_driver.clearDisplay();
    display_driver.setTextSize(text_size);
    display_driver.setTextColor(WHITE);
    display_driver.setCursor(0, 0);
    display_driver.println(line);
    display_driver.display();
  }

  void show_start_text_line() {
    if ((start_text_current_line < 0) || (start_text_current_line >= number_of_start_text_lines)) {
      start_text_current_line = 0;
    }

    show_line(start_text_lines[start_text_current_line]);
    start_text_current_line += 1;
  }

  void show_dip_mode_line(DipMode mode)
  {
    if (mode == DipMode::NUMBER_OF_DIP_MODES)
    {
      show_line("Internal Error");
    }
    else
    {
      sprintf(print_dip_mode_buffer,
              "Eintauch-\nModus:\n%s",
              dip_mode_description[static_cast<int>(mode)]);
    }

    show_line(print_dip_mode_buffer);
  }

  void show_done_text_line() {
    show_line(" Tee\n fertig", 3);
  }

  void toggle_invert_display() {
    is_display_inverted = not is_display_inverted;
    display_driver.invertDisplay(is_display_inverted);
    display_driver.display();
  }

  void set_invert_display(bool value) {
    display_driver.invertDisplay(value);
  }


  //! \param is_time_set true if the time is currently being set
  void show_time(int time_in_seconds, bool is_time_set = false) {
    int minutes = time_in_seconds / 60;
    int seconds = time_in_seconds % 60;

    display_driver.clearDisplay();
    display_driver.setTextSize(4);
    display_driver.setTextColor(WHITE);
    display_driver.setCursor(0, 0);

    if (minutes < 100) {
      sprintf(print_time_buffer, "%2d:%02d", minutes, seconds);
    }
    else {
      sprintf(print_time_buffer, "%2d", minutes);
    }
    if (is_time_set) {
      strcat(print_time_buffer, "\neinst");
    }
    display_driver.println(print_time_buffer);

    display_driver.display();
  }

};


/**
   Operate the penguin's beak.
   Use up() and down() to indicate that the beak should be moved up/down.
   Use tick() repeatedly to actually perform the movement.
   Note: On initialization of the Arduino, it looks like the PWM
         will drive the Servo to DOWN once whatever you do here...
*/
struct Beak {

  // parameters
  const int min_angle = 100;  //!< Angle when the beak is at the lowest position
  const int max_angle = 25;   //!< Angle when the beak is at the highest position
  const int servo_direction_for_beak_up = -1;  //!< Mapping from change in physical position to change in angle

  const int lower_jiggle_angle = min_angle;
  const int upper_jiggle_angle = 40;

  const int pin_no;
  const int ticks_per_degree_down;
  const int ticks_per_degree_up;

  enum class RequestedMovement
  {
    NONE,
    UP,
    DOWN,
    JIGGLE_UP,
    JIGGLE_DOWN,
  };
  RequestedMovement requested_movement = RequestedMovement::NONE;
  int current_angle = min_angle;
  int targeted_angle = max_angle;
  int ticks_remaining_until_going_to_next_angle;

  Servo servo_controller;

  /** Note: Only pins 9 and 10 are supported!
      See https://www.arduino.cc/en/Reference/ServoAttach
  */
  Beak(int pin_number
       , int delay_moving_down_in_number_of_ticks_per_degree
       , int delay_moving_up_in_number_of_ticks_per_degree
      )
    : pin_no(pin_number)
    , ticks_per_degree_down(delay_moving_down_in_number_of_ticks_per_degree)
    , ticks_per_degree_up(delay_moving_up_in_number_of_ticks_per_degree)
    , ticks_remaining_until_going_to_next_angle(delay_moving_up_in_number_of_ticks_per_degree)
  {
  }

  void init() {
    servo_controller.attach(pin_no);
  }

  void up() {
    requested_movement = RequestedMovement::UP;
    ticks_remaining_until_going_to_next_angle = ticks_per_degree_up;
    targeted_angle = max_angle;
  }

  void down() {
    requested_movement = RequestedMovement::DOWN;
    ticks_remaining_until_going_to_next_angle = ticks_per_degree_down;
    targeted_angle = min_angle;
  }

  void jiggle() {
    requested_movement = RequestedMovement::JIGGLE_DOWN;
    ticks_remaining_until_going_to_next_angle = ticks_per_degree_down;
    targeted_angle = lower_jiggle_angle;
  }

  void tick() {

    if (requested_movement == RequestedMovement::NONE)
    {
      return;
    }

    if (ticks_remaining_until_going_to_next_angle > 0) {
      ticks_remaining_until_going_to_next_angle -= 1;
      return;
    }

    servo_controller.write(current_angle);

    if (current_angle == targeted_angle) {
      switch (requested_movement)
      {
        case RequestedMovement::UP:
        case RequestedMovement::DOWN:
          {
            requested_movement = RequestedMovement::NONE;
            return;
          }

        case RequestedMovement::JIGGLE_UP:
          {
            requested_movement = RequestedMovement::JIGGLE_DOWN;
            ticks_remaining_until_going_to_next_angle = ticks_per_degree_down;
            targeted_angle = lower_jiggle_angle;
          }
          break;

        case RequestedMovement::JIGGLE_DOWN:
          {
            requested_movement = RequestedMovement::JIGGLE_UP;
            ticks_remaining_until_going_to_next_angle = ticks_per_degree_up;
            targeted_angle = upper_jiggle_angle;
          }
          break;

        case RequestedMovement::NONE:
        default:
          return;
      }
    }
    else {
      switch (requested_movement)
      {
        case RequestedMovement::JIGGLE_UP:
        case RequestedMovement::UP:
          {
            current_angle += servo_direction_for_beak_up;
            ticks_remaining_until_going_to_next_angle = ticks_per_degree_up;
          }
          break;
        case RequestedMovement::JIGGLE_DOWN:
        case RequestedMovement::DOWN:
          {
            current_angle -= servo_direction_for_beak_up;
            ticks_remaining_until_going_to_next_angle = ticks_per_degree_down;
          }
          break;
        case RequestedMovement::NONE:
        default:
          return;
      }
    }
  }

  bool done() {
    return requested_movement == RequestedMovement::NONE;
  }

};


/** Button needs to short given pin to GND when pressed. */
struct Button
{
  const int pin_number;
  bool was_previously_high = false;
  int button_press_count = 0;

  Button(int the_pin_number)
    : pin_number(the_pin_number)
  {
  }

  void init() {
    pinMode(pin_number, INPUT_PULLUP);
  }

  bool is_pressed()
  {
    int value = digitalRead(pin_number);
    return value == LOW;
  }

  int pop_button_press_count()
  {
    int count = button_press_count;
    button_press_count = 0;
    return count;
  }

  void tick()
  {
    bool pressed_now = is_pressed();
    if (pressed_now)
    {
      was_previously_high = true;
    }
    else
    {
      if (was_previously_high)
      {
        button_press_count += 1;
        was_previously_high = false;
      }
    }
  }

};


/*
   When turned on, the switch performs a periodic on/off cycle on a digital output.
   This means, it is turned on, then off after T_on, then again on after T_period.
   So it is on for T_on/T_period.
*/
struct PeriodicSwitch
{
    const int pin_number;
    const unsigned long total_period_duration_ms;
    const unsigned long on_during_period_ms;

    unsigned long activation_time_stamp;
    int current_pin_value = LOW;
    bool is_on = false;

    PeriodicSwitch(int the_pin, unsigned long period_duration_ms, unsigned long period_on_time_ms)
      : pin_number(the_pin)
      , total_period_duration_ms(period_duration_ms)
      , on_during_period_ms(period_on_time_ms)
    {
    }

    void init()
    {
      pinMode(pin_number, OUTPUT);
    }

    void on()
    {
      activation_time_stamp = millis();
      is_on = true;
    }

    void off()
    {
      is_on = false;
    }

    void tick()
    {
      if (is_on)
      {
        unsigned long now = millis();

        if (is_within_duty_cycle(now))
        {
          current_pin_value = HIGH;
        }
        else
        {
          current_pin_value = LOW;
        }

      }
      else
      {
        current_pin_value = LOW;
      }

      digitalWrite(pin_number, current_pin_value);
    }

  private:

    bool is_within_duty_cycle(unsigned long now)
    {
      auto time_within_cycle = (now - activation_time_stamp) % total_period_duration_ms;
      return time_within_cycle < on_during_period_ms;
    }

};


struct DigitalTeePi
{

    // members for control
    Beak& beak;
    LcdDisplay& lcd;
    Button& increase_time_button;
    Button& decrease_time_button;
    Button& start_button;
    PeriodicSwitch& alarm;

    // parameters
    const unsigned long DURATION_SPLASH_PAGE_MS = 2000;
    const int TIME_CHANGE_ON_BUTTON_PRESS_S = 15;
    const unsigned long DURATION_DONE_PAGE_MS = 2000;
    const unsigned long DURATION_ALARM_BLINK_MS = 500;

    // state of the application
    enum class TeePiState
    {
      INIT,
      SPLASH_SCREEN,
      SET_TIME,
      SET_DIP_MODE,
      TIME_RUNNING,
      TIME_UP,
    };
    DipMode dip_mode = DipMode::SIMPLY_DOWN;
    TeePiState state = TeePiState::INIT;
    unsigned long last_time = 0;
    unsigned long last_time_inverted = 0;

    /** Time set with buttons -- will be used for timer */
    int duration_in_seconds = 120;
    unsigned long timestamp_timer_started = 0;

    DigitalTeePi(
      Beak& the_beak,
      LcdDisplay& the_display,
      Button& the_increase_time_button,
      Button& the_decrease_time_button,
      Button& the_start_button,
      PeriodicSwitch& the_alarm_switch
    )
      : beak(the_beak)
      , lcd(the_display)
      , increase_time_button(the_increase_time_button)
      , decrease_time_button(the_decrease_time_button)
      , start_button(the_start_button)
      , alarm(the_alarm_switch)
    {
    }

    void init()
    {
      beak.init();
      lcd.init();
      increase_time_button.init();
      decrease_time_button.init();
      start_button.init();
      alarm.init();
    }

    void tick()
    {
      increase_time_button.tick();
      decrease_time_button.tick();
      start_button.tick();
      beak.tick();
      alarm.tick();

      handle_state();
    }

  private:

    void handle_state()
    {
      unsigned long now = millis();

      switch (state) {
        case TeePiState::INIT:
          {
            beak.up();
            alarm.off();
            lcd.set_invert_display(false);
            lcd.show_start_text_line();
            last_time = now;
            state = TeePiState::SPLASH_SCREEN;
          }
          break;
        case TeePiState::SPLASH_SCREEN:
          {
            if ((now - last_time) >= DURATION_SPLASH_PAGE_MS) {
              lcd.show_start_text_line();
              last_time = now;
            }
            if (any_button_pressed()) {
              state = TeePiState::SET_TIME;
            }
          }
          break;
        case TeePiState::SET_TIME:
          {
            int increase_count = increase_time_button.pop_button_press_count();
            int decrease_count = decrease_time_button.pop_button_press_count();
            int change_count = increase_count - decrease_count;
            duration_in_seconds += TIME_CHANGE_ON_BUTTON_PRESS_S * change_count;
            if (duration_in_seconds < 0) {
              duration_in_seconds = 0;
            }
            lcd.show_time(duration_in_seconds, true);
            int start_count = start_button.pop_button_press_count();
            if (start_count > 0)
            {
              state = TeePiState::SET_DIP_MODE;
              lcd.show_dip_mode_line(dip_mode);
            }
          }
          break;
        case TeePiState::SET_DIP_MODE:
          {
            int increase_count = increase_time_button.pop_button_press_count();
            int decrease_count = decrease_time_button.pop_button_press_count();
            int change_count = increase_count - decrease_count;

            int new_dip_mode = (static_cast<int>(dip_mode) + change_count) % static_cast<int>(DipMode::NUMBER_OF_DIP_MODES);
            if (new_dip_mode < 0)
            {
              new_dip_mode += static_cast<int>(DipMode::NUMBER_OF_DIP_MODES);
            }
            if (new_dip_mode != static_cast<int>(dip_mode))
            {
              dip_mode = static_cast<DipMode>(new_dip_mode);
              lcd.show_dip_mode_line(dip_mode);
            }

            int start_count = start_button.pop_button_press_count();
            if (start_count > 0)
            {
              timestamp_timer_started = now;
              state = TeePiState::TIME_RUNNING;

              switch (dip_mode)
              {
                case DipMode::SIMPLY_DOWN:
                  beak.down();
                  break;

                case DipMode::JIGGLE:
                  beak.jiggle();
                  break;

                case DipMode::NO_DIPPING:
                  break;

                default:
                  lcd.show_line("Internal Error");
                  break;
              }
            }
          }
          break;
        case TeePiState::TIME_RUNNING:
          {
            unsigned long time_expired_ms = now - timestamp_timer_started;
            int time_expired_s = time_expired_ms / 1000;
            int time_remaining = duration_in_seconds - time_expired_s;
            lcd.show_time(time_remaining);

            if (any_button_pressed())
            {
              state = TeePiState::SET_TIME;
            }

            if (time_remaining <= 0) {
              state = TeePiState::TIME_UP;
              beak.up();
              lcd.show_done_text_line();
              alarm.on();
              last_time = now;
              last_time_inverted = now;
            }
          }
          break;
        case TeePiState::TIME_UP:
          {
            if ((now - last_time) >= DURATION_DONE_PAGE_MS) {
              lcd.show_done_text_line();
              last_time = now;
            }
            if ((now - last_time_inverted) >= DURATION_ALARM_BLINK_MS) {
              lcd.toggle_invert_display();
              last_time_inverted = now;
            }
            if (any_button_pressed())
            {
              state = TeePiState::INIT;
            }
          }
          break;
      }
    }

    bool any_button_pressed()
    {
      int increase_count = increase_time_button.pop_button_press_count();
      int decrease_count = decrease_time_button.pop_button_press_count();
      int start_count = start_button.pop_button_press_count();
      bool any_pressed = increase_count > 0 or decrease_count > 0 or start_count > 0;
      return any_pressed;
    }

};


Beak beak(9, 2, 3);
LcdDisplay lcd;
Button increase_time(12);
Button decrease_time(11);
Button start(10);
PeriodicSwitch alarm(5, 25000, 20000);

DigitalTeePi teepi(beak, lcd, increase_time, decrease_time, start, alarm);

void setup() {
  // activate the init() calls to run the tests
  /* test_beak
    beak.init();
    pinMode(LED_BUILTIN, OUTPUT);
  */

  /* test_lcd
    //lcd.init();
  */

  /* test_button
    increase_time.init();
    pinMode(LED_BUILTIN, OUTPUT);
  */

  /* test alarm
    alarm.init();
  */

  /* teepi_application -- activate the init() call for teepi for the main function

  */
  teepi.init();
}

void test_beak()
{
  digitalWrite(LED_BUILTIN, HIGH);
  beak.up();
  while (not beak.done()) {
    beak.tick();
    delay(5);
  }

  delay(1000);

  digitalWrite(LED_BUILTIN, LOW);
  beak.down();
  delay(1000);
  while (not beak.done()) {
    beak.tick();
    delay(5);
  }
  delay(5000);
}

void test_lcd()
{
  for (int i = 0; i < 6; ++i) {
    lcd.show_start_text_line();
    delay(1000);
  }

  for (int i = 61; i != 0; i -= 1) {
    lcd.show_time(i);
    delay(20);
  }

  lcd.show_done_text_line();
  for (int i = 0; i < 10; ++i) {
    lcd.toggle_invert_display();
    delay(500);
  }

  delay(1000);
}

void test_button()
{
  for (int i = 0; i < 1000; ++i) {
    increase_time.tick();
    delay(5);
  }
  int count = increase_time.pop_button_press_count();
  for (int i = 0; i < count; ++i) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

void test_alarm()
{
  alarm.on();
  while (true) {
    alarm.tick();
  }
}

void teepi_application()
{
  teepi.tick();
  delay(5);
}


void loop() {
  //test_beak();
  //test_beak();
  //test_alarm();
  teepi_application();
}
