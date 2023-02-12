#include "Arduino.h"
#include "lvgl.h"

static lv_obj_t* scSecD;
static lv_obj_t* scSecU;
static lv_obj_t* scMinD;
static lv_obj_t* scMinU;
static lv_obj_t* scHouD;
static lv_obj_t* scHouU;

const lv_font_t* myFont = &lv_font_montserrat_48;

// Create one tile
lv_obj_t* createTile( lv_obj_t *tv, int x, int y) {
  lv_obj_t* sc = lv_tileview_create(tv);
  lv_obj_align(sc, LV_ALIGN_TOP_LEFT, x, y);
  lv_obj_set_size(sc, myFont->line_height-2, myFont->line_height);
  return sc;
}
void createDblPts(lv_obj_t *tv, int x, int y){
  lv_obj_t* wgtPP = lv_label_create(tv);
  lv_label_set_text( wgtPP , ":" );
  lv_obj_set_style_text_font(wgtPP, myFont, 0); 
  lv_obj_align(wgtPP, LV_ALIGN_TOP_LEFT, x, y);
}
// create HH:MM.SS
void createScolledClock(lv_obj_t *tv, int x, int y) {
  int off [8]= {0, 15, 30, 45, 60, 75, 90, 105}; 
  for (int i=0; i<8;i++) {
    off[i] = i * ((myFont->line_height/2)+myFont->base_line);
  }
  off[2]=off[2]+myFont->base_line;
  off[5]=off[5]+myFont->base_line;
  scHouD = createTile(tv, x+off[0], y);
  scHouU = createTile(tv, x+off[1], y);
  createDblPts(tv, x+off[2], y);
  scMinD = createTile(tv, x+off[3], y);
  scMinU = createTile(tv, x+off[4], y);
  createDblPts(tv, x+off[5], y);
  scSecD = createTile(tv, x+off[6], y);
  scSecU = createTile(tv, x+off[7], y);
}
String lad[] = {"0","1","2","3","4","5","6","7","8","9"}; 
void setDigit(lv_obj_t *wdgt, int i ) {
  lv_obj_t* wgtDTile = lv_tileview_add_tile(wdgt, 0, i, LV_DIR_HOR);
  lv_obj_t* wgtDLab = lv_label_create(wgtDTile);
  lv_obj_align(wgtDLab, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_label_set_text( wgtDLab , lad[i].c_str() );
  lv_obj_set_style_text_font(wgtDLab, myFont, 0); 
}
void buildClock(lv_obj_t *tv, int x, int y) {
  createScolledClock(tv, x, y);
  for (int i=0; i<10; i++) {
    setDigit(scSecD, i );
    setDigit(scSecU, i );
    setDigit(scMinD, i );
    setDigit(scMinU, i );
    setDigit(scHouD, i );
    setDigit(scHouU, i );
  }
}