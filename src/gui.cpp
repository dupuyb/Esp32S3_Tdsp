#include "Arduino.h"
#include "lvgl.h"
// File FS SPI Flash File System
#include <FS.h>
#include <SPIFFS.h>
// LCD setup
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
// LCD pins
#include "pin_config.h"
// Gui definition
#include "gui.h"
#include "scrollHMS.h"
// implementation Drive File SPIFFS
#include "driverSpiFs.h"

// Lcd setup
static esp_lcd_panel_io_handle_t io_handle = NULL;
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
static lv_disp_drv_t disp_drv;      // contains callback functions
static lv_color_t *lv_disp_buf;
// Gui
static lv_obj_t *display;
static uint8_t pageNbr;
static lv_obj_t *chart;
static lv_chart_series_t *ser1;
static lv_chart_series_t *ser2;
static lv_obj_t *qrImage;
static String httpstr = "";
typedef struct
{
  lv_img_header_t header; /**< A header describing the basics of the image*/
  uint32_t data_size;     /**< Size of the image in bytes*/
  uint8_t *data;          /**< Pointer to the data of the image*/
} img_dsc_t;
static img_dsc_t img_purethermal;

// copy a buffer's content to a specific area of the display
static bool gui_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
  return false;
}

static void gui_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
  esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)drv->user_data, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);
}

void lcd_setup()
{
  // Pin LCD
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);
  pinMode(PIN_LCD_RD, OUTPUT);
  digitalWrite(PIN_LCD_RD, HIGH);
  // Initializing Hardware LCD
  esp_lcd_i80_bus_handle_t i80_bus = NULL;
  esp_lcd_i80_bus_config_t bus_config = {
      .dc_gpio_num = PIN_LCD_DC,
      .wr_gpio_num = PIN_LCD_WR,
      .clk_src = LCD_CLK_SRC_PLL160M,
      .data_gpio_nums = {
          PIN_LCD_D0,
          PIN_LCD_D1,
          PIN_LCD_D2,
          PIN_LCD_D3,
          PIN_LCD_D4,
          PIN_LCD_D5,
          PIN_LCD_D6,
          PIN_LCD_D7,
      },
      .bus_width = 8,
      .max_transfer_bytes = LVGL_LCD_BUF_SIZE * sizeof(uint16_t),
  };
  esp_lcd_new_i80_bus(&bus_config, &i80_bus);
  esp_lcd_panel_io_i80_config_t io_config = {
      .cs_gpio_num = PIN_LCD_CS,
      .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
      .trans_queue_depth = 20,
      .on_color_trans_done = gui_notify_lvgl_flush_ready,
      .user_ctx = &disp_drv,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .dc_levels = {
          .dc_idle_level = 0,
          .dc_cmd_level = 0,
          .dc_dummy_level = 0,
          .dc_data_level = 1,
      },
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));
  esp_lcd_panel_handle_t panel_handle = NULL;
  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = PIN_LCD_RES,
      .color_space = ESP_LCD_COLOR_SPACE_RGB,
      .bits_per_pixel = 16,
  };
  esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle);
  esp_lcd_panel_reset(panel_handle);
  esp_lcd_panel_init(panel_handle);
  esp_lcd_panel_invert_color(panel_handle, true);
  // Swap Mirror And Gap
  esp_lcd_panel_swap_xy(panel_handle, true);
  esp_lcd_panel_mirror(panel_handle, false, true);
  esp_lcd_panel_set_gap(panel_handle, 0, 35);
  /* Lighten the screen with gradient */
  ledcSetup(0, 10000, 8);
  ledcAttachPin(PIN_LCD_BL, 0);
  for (uint8_t i = 0; i < 0xFF; i++)
  {
    ledcWrite(0, i);
    delay(2);
  }
  lv_init();
  lv_disp_buf = (lv_color_t *)heap_caps_malloc(LVGL_LCD_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  lv_disp_draw_buf_init(&disp_buf, lv_disp_buf, NULL, LVGL_LCD_BUF_SIZE);
  lv_disp_drv_init(&disp_drv);
  /* Change the following line to your display resolution*/
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = gui_lvgl_flush_cb;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.user_data = panel_handle;
  lv_disp_drv_register(&disp_drv);
}

// USER PAGES
void gui_switch_page(void)
{
  pageNbr++;
  lv_obj_set_tile_id(display, 0, pageNbr % UI_PAGE_COUNT, LV_ANIM_ON);
  LV_LOG_WARN("Page:%d", pageNbr % UI_PAGE_COUNT);
}

// CLOCK
void gui_switch_clock(int h, int m, int s)
{
  lv_obj_set_tile_id(scSecD, 0, (s / 10) % 10, LV_ANIM_ON);
  lv_obj_set_tile_id(scSecU, 0, s % 10, LV_ANIM_ON);
  lv_obj_set_tile_id(scMinD, 0, (m / 10) % 10, LV_ANIM_ON);
  lv_obj_set_tile_id(scMinU, 0, m % 10, LV_ANIM_ON);
  lv_obj_set_tile_id(scHouD, 0, (h / 10) % 10, LV_ANIM_ON);
  lv_obj_set_tile_id(scHouU, 0, h % 10, LV_ANIM_ON);
}

static void update_page_evt_cb(lv_event_t *e)
{
  lv_msg_t *m = lv_event_get_msg(e);
  uint32_t idxpage = lv_msg_get_id(m);
  if (idxpage == MSG_PAGE1)
  {
    lv_obj_t *label = lv_event_get_target(e);
    char temp[80];
    const DataMsg *v = (const DataMsg *)lv_msg_get_payload(m);
    const char *fmt = (const char *)lv_msg_get_user_data(m);
    snprintf(temp, 80, fmt, v->value, v->volt, v->raw, v->count, v->msg.c_str());
    lv_label_set_text(label, temp);
    lv_chart_set_next_value(chart, ser1, v->volt);
    lv_chart_set_next_value(chart, ser2, v->fsin);
  }
  else if (idxpage == MSG_PAGE2)
  {
    lv_obj_t *label = lv_event_get_target(e);
    esp_chip_info_t t;
    esp_chip_info(&t);
    String text = "Chip : ";
    text += ESP.getChipModel();
    text += " \nSize psram : ";
    text += ESP.getPsramSize() / 1024;
    text += " KB \nSize flash : ";
    text += ESP.getFlashChipSize() / 1024;
    text += " KB\nWifi : ";
    text += (const char *)lv_msg_get_payload(m);
    lv_label_set_text(label, text.c_str());
  }
  else if (idxpage == MSG_TABLE)
  {
    lv_obj_t *table = lv_event_get_target(e);
    String *in = (String *)lv_msg_get_payload(m);
    size_t pos1 = 0;
    size_t pos2 = in->length();
    uint16_t i = 0;
    while ((pos2 = in->indexOf('\t', pos1)) != -1)
    {
      lv_table_set_cell_value(table, i++, 1, in->substring(pos1, pos2).c_str());
      pos1 = pos2 + 1;
    }
    if (!httpstr.endsWith(lv_table_get_cell_value(table, 4, 1)))
    {
      httpstr = "http://";
      httpstr += lv_table_get_cell_value(table, 4, 1),
          LV_LOG_WARN(httpstr.c_str());
      lv_qrcode_update(qrImage, httpstr.c_str(), strlen(httpstr.c_str()));
    }
  }
  else if (idxpage = MSG_PTHER)
  {
  }
}

static void draw_table_evt_cb(lv_event_t *e)
{
  lv_obj_t *obj = lv_event_get_target(e);
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  /*If the cells are drawn...*/
  if (dsc->part == LV_PART_ITEMS)
  {
    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);

    /*Make the texts in the first cell center aligned*/
    if (row == 0)
    {
      dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
      dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), dsc->rect_dsc->bg_color, LV_OPA_20);
      dsc->rect_dsc->bg_opa = LV_OPA_COVER;
    }
    /*In the first column align the texts to the right*/
    else if (col == 0)
    {
      dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;
    }

    /*make every 2nd row grayish*/
    if ((row != 0 && row % 2) == 0)
    {
      dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY), dsc->rect_dsc->bg_color, LV_OPA_20);
      dsc->rect_dsc->bg_opa = LV_OPA_COVER;
    }
  }
}

void my_gif_set_spi(lv_obj_t *img3, String path)
{
  const char *strfile = path.c_str() + 2;
  LV_LOG_WARN("strfile=%s", strfile);
  File file = SPIFFS.open(strfile, "r");
  if (!file)
  {
    LV_LOG_WARN("File %s is absent.", path);
  }
  else
  {
    size_t size = file.size();
    char *gif_1 = (char *)malloc(size * sizeof(char));
    file.readBytes(gif_1, size);
    //! Bug in lv_img_src_get_type  "GIF89a" copy lv_gif_set_src in my_gif_set_src
    //! Change in  lv_gif_set_src if(lv_img_src_get_type(src) {..} else if() {..}  by  gifobj->gif = gd_open_gif_data(src);
    my_gif_set_src(img3, gif_1);
  }
}

void gui_pages()
{
  // Display
  display = lv_tileview_create(lv_scr_act());
  lv_obj_align(display, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_size(display, LV_PCT(100), LV_PCT(100));

  // tuiles
  lv_obj_t *tv1 = lv_tileview_add_tile(display, 0, 0, LV_DIR_HOR);
  lv_obj_t *tv2 = lv_tileview_add_tile(display, 0, 1, LV_DIR_HOR);
  lv_obj_t *tv3 = lv_tileview_add_tile(display, 0, 2, LV_DIR_HOR);
  lv_obj_t *tv4 = lv_tileview_add_tile(display, 0, 3, LV_DIR_HOR);
  lv_obj_t *tv5 = lv_tileview_add_tile(display, 0, 4, LV_DIR_HOR);

  // Page 1 TEST GRAPH + LABEL COLOR
  lv_obj_t *msg_label = lv_label_create(tv1);
  lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_width(msg_label, lv_pct(100));
  lv_label_set_long_mode(msg_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_label_set_recolor(msg_label, true);
  // change in .pio/build/esp32-s3-devkitc-1/libbd0/lvgl/font/lv_font.h LV_FONT_MONTSERRAT_20 set default 14
  lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_border_width(msg_label, 1, 0);
  lv_obj_set_style_border_color(msg_label, lv_palette_main(LV_PALETTE_YELLOW), 0);
  String /* `text` is a variable that is used to store the text that will be displayed on the screen. */
      text = "#0000ff Bruno # #ff00ff Test# #ff0000 of a# label ";
  text += "and  wrap long text automatically.";
  lv_label_set_text(msg_label, text.c_str());
  /* Voltage label */
  lv_obj_t *bat_label = lv_label_create(tv1);
  lv_obj_align_to(bat_label, msg_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
  lv_obj_add_event_cb(bat_label, update_page_evt_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_msg_subsribe_obj(MSG_PAGE1, bat_label, (void *)"Volt : %2.1f V\nMap : %04d\nRaw : %04d\n\nFps : %d\nMsg : %s");
  // Create a chart
  chart = lv_chart_create(tv1);
  lv_obj_set_size(chart, 200, 100);
  lv_obj_align(chart, LV_ALIGN_TOP_LEFT, EXAMPLE_LCD_H_RES - 200, 35);
  // Show lines and points too
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  // Del point
  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
  lv_chart_set_point_count(chart, MAX_PTS_SERIE);
  // Add two data series
  ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
  ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
  // Number of points
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_X, 0, MAX_PTS_SERIE);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 3300);
  // Axis
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 2, 1, 5, 5, true, 20);
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 4, 1, 3, 5, true, 50);
  // Required after direct set
  lv_chart_refresh(chart);

  // page 2  TEST LABEL
  lv_obj_t *debug_label = lv_label_create(tv2);
  lv_label_set_recolor(debug_label, true);
  lv_obj_add_event_cb(debug_label, update_page_evt_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_msg_subsribe_obj(MSG_PAGE2, debug_label, (void *)"");
  lv_obj_align(debug_label, LV_ALIGN_LEFT_MID, 0, 10);

  //  Install File Driver for SPIFFS format
  init_file_system_driver();

// FAIRE TEST SI£ LES FI£CHIERS N'EXISTE PAS !
  // Creating an image object and setting the source of the image to the file "example.bmp" 
  lv_obj_t *img1 = lv_img_create(tv2);
  lv_img_set_src(img1, "P:/example.bmp"); // Warming BMP codage 16 Color...Export with Gimp
  lv_obj_align(img1, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *img10 = lv_img_create(tv2);
  lv_img_set_src(img10, "P:/example.jpg");
  lv_obj_align(img10, LV_ALIGN_TOP_LEFT, 60, 0);

  // lv_obj_t * img11 = lv_img_create(tv2);
  // lv_img_set_src(img11, "P:/example.png");  // Not ok
  // lv_obj_align(img11, LV_ALIGN_TOP_LEFT, 120, 0);

  // Creating an image object and setting the source of the image to the symbol "Accept".
  lv_obj_t *img2 = lv_img_create(tv2);
  lv_img_set_src(img2, LV_SYMBOL_WIFI "Wifi");
  lv_obj_align(img2, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  lv_obj_t *img3 = lv_gif_create(tv2);
  // lv_gif_set_src(img3, "P:/example.gif");
  my_gif_set_spi(img3, "P:/example.gif");
  lv_obj_align(img3, LV_ALIGN_RIGHT_MID, 0, 0);

  // page 3 TEST TABLE
  lv_obj_t *table = lv_table_create(tv3);
  lv_label_set_recolor(table, true);
  /*Fill the first column*/
  lv_table_set_cell_value(table, 0, 0, "Chip ");
  lv_table_set_cell_value(table, 1, 0, "Psram ");
  lv_table_set_cell_value(table, 2, 0, "Flash ");
  lv_table_set_cell_value(table, 3, 0, "MAC ");
  lv_table_set_cell_value(table, 4, 0, "IP ");
  lv_table_set_cell_value(table, 5, 0, "Date ");
  lv_table_set_cell_value(table, 6, 0, "");
  lv_table_set_cell_value(table, 7, 0, "");
  lv_table_set_col_width(table, 0, 60);

  /*Fill the second column*/
  for (int i = 0; i < 8; i++)
    lv_table_set_cell_value(table, i, 1, "");
  lv_table_set_col_width(table, 1, 150);

  lv_obj_set_style_pad_ver(table, 1, LV_PART_ITEMS);
  lv_obj_set_style_pad_hor(table, 1, LV_PART_ITEMS);
  lv_obj_set_height(table, EXAMPLE_LCD_V_RES);
  lv_obj_align(table, /* see `lv_obj.h`  */ LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_event_cb(table, update_page_evt_cb, LV_EVENT_MSG_RECEIVED, NULL);
  lv_msg_subsribe_obj(MSG_TABLE, table, (void *)"");
  lv_obj_add_event_cb(table, draw_table_evt_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

  // QR code test
  lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_LIGHT_BLUE, 5);
  lv_color_t fg_color = lv_palette_darken(LV_PALETTE_BLUE, 4);
  qrImage = lv_qrcode_create(tv3, 100, fg_color, bg_color);
  lv_obj_align(qrImage, LV_ALIGN_TOP_RIGHT, 0, 0);
  // Add a border with bg_color
  lv_obj_set_style_border_color(qrImage, bg_color, 0);
  lv_obj_set_style_border_width(qrImage, 5, 0);

  // test scrolled clock PAGE 4
  buildClock(tv4, 0, 0);

  // Test Purtherminal
  lv_obj_t *ipurth = lv_img_create(tv5);

  LV_IMG_DECLARE(purthermal); // Voir file purthermal.c
  lv_img_set_src(ipurth, &purthermal);
  //   // Create a dynamic image
  //   img_purethermal.header.always_zero = 0;
  //   img_purethermal.header.w = 160;
  //   img_purethermal.header.h = 120;
  //   img_purethermal.data_size = 19200 * LV_COLOR_SIZE / 8;
  //   img_purethermal.header.cf = LV_IMG_CF_TRUE_COLOR;
  //   // Allocate on the heap.
  //   img_purethermal.data = (uint8_t*)malloc(img_purethermal.data_size);
  //   // Populate the data array (using the existing one for convenience)
  //   for (int i = 0; i < img_purethermal.data_size; ++i) {
  //       img_purethermal.data[i] = purthermal.data[i];
  //   }
  // lv_img_set_src(ipurth, &img_purethermal);
  
  lv_obj_align(ipurth, LV_ALIGN_CENTER, 0, 0);
  lv_img_set_zoom(ipurth, 360);  // x2 w.h 160.120
}
