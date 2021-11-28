#include "zebus-display.h"

#include <TFT_eSPI.h>
#include <time.h>
#include "driver/ledc.h"

#include "zebus-events.h"
#include "zebus-messages.h"
#include "zebus-sprites.h"
#include "zebus-state.h"
#include "zebus-system-info.h"

#define GET_BYTE(INT32, I) ((INT32 >> 8 * I) & 0XFF)

// variables
EventGroupHandle_t event_group;

TFT_eSPI tft = TFT_eSPI(); // Invoke library

measurement_bool flame;
measurement_float flow;
uint8_t queue_size;


// prototypes
void display_handle_flame(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void display_handle_flow(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void display_handle_queue_size(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

void setupDisplay();
void disableDisplay();

// prototypes: update
void updateDisplay();

//prototypes: helpers
size_t print_time(struct tm * timeinfo, const char * format);
void print_ip_addr();

// state
typedef enum {
  DISPLAY_STATE_OFF = 0,
  DISPLAY_STATE_ACTIVE,
  DISPLAY_STATE_DISABLE,
  DISPLAY_STATE_STOPPING,
} display_state_t;

typedef display_state_t (*display_handler)(EventBits_t &);

display_state_t display_off(EventBits_t &uxBits) {
  if (uxBits & DISPLAY_ENABLED) {
    setupDisplay();
    updateDisplay();
    return DISPLAY_STATE_ACTIVE;
  }
  return DISPLAY_STATE_OFF;
}

display_state_t display_active(EventBits_t &uxBits) {
  xEventGroupClearBits(event_group, DISPLAY_DISABLED);
  updateDisplay();
  return uxBits & DISPLAY_ENABLED ? DISPLAY_STATE_ACTIVE : DISPLAY_STATE_DISABLE;
}

display_state_t display_disable(EventBits_t &uxBits) {
  disableDisplay();
  updateDisplay();
  return DISPLAY_STATE_STOPPING;
}

display_state_t display_stopping(EventBits_t &uxBits) {
  if (uxBits & DISPLAY_DISABLED) {
    return DISPLAY_STATE_OFF;
  }
  updateDisplay();
  return DISPLAY_STATE_STOPPING;
}

display_handler handlers[] = {
  [DISPLAY_STATE_OFF] = display_off,
  [DISPLAY_STATE_ACTIVE] = display_active,
  [DISPLAY_STATE_DISABLE] = display_disable,
  [DISPLAY_STATE_STOPPING] = display_stopping,
};

// public functions
void displayTask(void *pvParameter) {
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLAME, display_handle_flame, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_RECVD_FLOW, display_handle_flow, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(ZEBUS_EVENTS, zebus_events::EVNT_UPD_QUEUE_SIZE, display_handle_queue_size, NULL));
  event_group = (EventGroupHandle_t) pvParameter;
  display_state_t display_state = DISPLAY_STATE_OFF;
  for (;;) {
    display_handler handler = handlers[display_state];
    if (handler) {
      EventBits_t uxBits = xEventGroupGetBits(event_group);
      display_state = handler(uxBits);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void display_handle_flame(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  flame = *((measurement_bool*) event_data);
}

void display_handle_flow(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  flow = *((measurement_float*) event_data);
}

void display_handle_queue_size(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  queue_size = *((uint8_t*) event_data);
}

static void IRAM_ATTR display_fadeout_complete(void *param) {
  xEventGroupSetBits(event_group, DISPLAY_DISABLED);
}

void display_disable_backlight() {
  ledc_set_fade_with_time(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0, ZEBUS_BACKLIGHT_FADEOUT_MS);
  ledc_fade_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, LEDC_FADE_NO_WAIT);
}

void display_enable_backlight() {
  ledc_timer_config_t ledc_timer = {
      .speed_mode = LEDC_HIGH_SPEED_MODE,    // timer mode
      .duty_resolution = LEDC_TIMER_10_BIT,  // resolution of PWM duty
      .timer_num = LEDC_TIMER_0,             // timer index
      .freq_hz = 5000,                       // frequency of PWM signal
      .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
  };
  // Set configuration of timer0 for high speed channels
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {
      .gpio_num = ZEBUS_BACKLIGHT_PIN,
      .speed_mode = LEDC_HIGH_SPEED_MODE,
      .channel = LEDC_CHANNEL_0,
      .intr_type = LEDC_INTR_FADE_END,
      .timer_sel = LEDC_TIMER_0,
      .duty = 0,
      .hpoint = 0,
  };
  ledc_channel_config(&ledc_channel);
  ledc_fade_func_install(ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED);
  ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 900, 0);
  ESP_ERROR_CHECK(ledc_isr_register(display_fadeout_complete, NULL, ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_SHARED, NULL));
}

// implementations
void setupDisplay() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Setup display");
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Set "cursor" at top left corner of display (0,0) and select font 1
  tft.setCursor(0, 0, 1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //  tft.println("Initialised default\n");

  display_enable_backlight();

  init_sprites(&tft);
}

void disableDisplay() {
  ESP_LOGI(ZEBUS_LOG_TAG, "Disable display");

  display_disable_backlight();
}

void updateDisplay() {
  tft.setCursor(0, 0, 1);
  if (system_info->wifi.config_ap.active) {
    tft.printf("AP Name    : %s\n", system_info->wifi.config_ap.ap_name);
    tft.printf("AP Password: %s\n", system_info->wifi.config_ap.ap_password);
  } else {
    tft.println("                                  ");
    tft.println("                                  ");
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    print_time(&timeinfo, "%c\n");
    tft.println();
  }

  print_ip_addr();
  tft.println();
  tft.printf("Command Queue size: %d             \n", queue_size);

  tft.setCursor(0, 195, 1);
  tft.printf("Self  : %s, sw: %s, hw: %s\n", system_info->ebus.self_id.device, system_info->ebus.self_id.sw_version, system_info->ebus.self_id.hw_version);
  tft.printf("Heater: %s, sw: %s, hw: %s\n", system_info->ebus.heater_id.device, system_info->ebus.heater_id.sw_version, system_info->ebus.heater_id.hw_version);
  tft.printf("Flame : %s       \n", measurement_valid(flame) ? (flame.value ? "ON" : "OFF") : "UNKNOWN");
  tft.printf("Flow  : %.2f     \n", measurement_valid(flow) ? flow.value : -1 );

  if (measurement_valid(flame) && measurement_valid(flow)) {
    draw_sprite_heater(280, 0, flame.value && !flow.value);
  }
  if (measurement_valid(flow)) {
    draw_sprite_shower(280, 40, flow.value);
  }
  draw_sprite_wifi_strength(280, 80, system_info->wifi.rssi);
  draw_sprite_ebus_queue(280, 120, queue_size);
}

size_t print_time(struct tm * timeinfo, const char * format) {
    char buf[64];
    size_t written = strftime(buf, sizeof(buf), format, timeinfo);
    if (written == 0) {
      return written;
    }
    return tft.printf(buf);
}

void print_ip_addr() {
  uint32_t ip = system_info->wifi.ip_addr;
  tft.printf("%d.%d.%d.%d", GET_BYTE(ip, 0), GET_BYTE(ip, 1), GET_BYTE(ip, 2), GET_BYTE(ip, 3));
}
