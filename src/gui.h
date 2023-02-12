#pragma once

#define UI_PAGE_COUNT  5
#define MSG_PAGE1      1
#define MSG_PAGE2      2
#define MSG_TABLE      3
#define MSG_PTHER      4

#define MAX_PTS_SERIE  50

struct DataMsg {
    uint32_t volt; // 0--3300
    uint32_t raw;  // 0--4096
    uint32_t fsin; // serie2
    float value;   // 0.0--3.3
    int count;     // fps
    String msg;
};

#define LV_DELAY(x)                                   \
  do {                                                \
  uint32_t t = x;                                     \
    while (t--) {                                     \
      lv_timer_handler();                             \
      delay(1);                                       \
    }                                                 \
  } while (0);

void lcd_setup();
void gui_pages();
void gui_switch_page();
void gui_switch_clock(int h, int m, int s);