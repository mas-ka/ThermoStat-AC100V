// EEPROM
#include <EEPROM.h>

// for LCD_I2C
#include <LiquidCrystal_I2C.h>
#define LCD_WIDTH 16
volatile LiquidCrystal_I2C lcd(0x27, LCD_WIDTH , 2);

byte deg[8] = {
	0b00010,
	0b00101,
	0b00101,
	0b00010,
	0b00000,
	0b00000,
	0b00000,
	0b00000
};

// for Thermo-Couple
#include <SPI.h>
#include "Adafruit_MAX31855.h"
#define MAXCS     10
Adafruit_MAX31855 thermocouple(MAXCS);

// 動作定義
enum Mode_operation {IDLE, ACTIVE, SETTING_TERM, SETTING_LO, SETTING_HI};
Mode_operation mode_operation = IDLE;
enum Selected_Term {LO, HI};
Selected_Term selected_term = LO;

// ボタン定義
#define PIN_SW_ACT 0
#define PIN_SW_NEG 1
#define PIN_SW_POS 7

enum Status_Button {RELEASE, PRESS, HOLD, CLICK, LONG};
volatile Status_Button status_button_neg = RELEASE,
                        status_button_pos = RELEASE,
                        status_button_act = RELEASE;

enum Mutex_Button {NONE, ACT, NEG, POS};
volatile Mutex_Button mutex_button = NONE;

volatile unsigned long msec_button_neg_last = 0,
                        msec_button_pos_last = 0,
                        msec_button_act_last = 0;

// 変数
short val_LO = 0, val_HI = 0; 


void setup() {
  Serial.begin(9600);
  //while (!Serial) {;}
  delay(1000);

  // Init LCD_I2C
  lcd.init(); lcd.setBacklight(255); lcd.clear(); lcd.noCursor();
  lcd.createChar(0, deg);

  // EEPROMからLOとHIの設定値を取得
  EEPROM.get(0,val_LO); // 0-1番地に(short)LO
  EEPROM.get(2, val_HI); // 2-3番地に(short)HI

  // ボタン初期化
  pinMode(PIN_SW_ACT, INPUT_PULLUP);
  pinMode(PIN_SW_NEG, INPUT_PULLUP);
  pinMode(PIN_SW_POS, INPUT_PULLUP);
  msec_button_act_last = 0;
  msec_button_neg_last = 0;
  msec_button_pos_last = 0;

  attachInterrupt(digitalPinToInterrupt(PIN_SW_ACT), button_act_changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SW_NEG), button_neg_changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SW_POS), button_pos_changed, CHANGE);

  // LCD表示
  display_mode_operation();
  display_val_LO(); display_val_HI();
  display_curr_temp(12.345);

}

void loop() {
  // ボタン動作のロジック
  // ACTボタン
  if (status_button_act == CLICK) {
    if (mutex_button == ACT) {
      on_button_act_click(); // 短クリック処理を呼び出す
      mutex_button = NONE; // ミューテックスを解放
    }
    status_button_act = RELEASE; // ステータスを解放に遷移
    msec_button_act_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_act == LONG) {
    if (mutex_button == ACT) {
      // 長クリック処理はない
      mutex_button = NONE; // ミューテックスを解放
    }
    status_button_act = RELEASE; // ステータスを解放に遷移
    msec_button_act_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_act == PRESS && millis() - msec_button_act_last > 500) { // ボタンが500msec以上長押しされた
    if (mutex_button == ACT) on_button_act_hold(); // 長押し処理を呼び出す
    status_button_act = HOLD; // ステータスを長押し中に遷移
    msec_button_act_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_act == HOLD && millis() - msec_button_act_last > 500) { // 長押しのまま500msec以上が経過した
    // ACTボタンに長押しリピート処理はない
    msec_button_act_last = millis(); // 前回ボタンイベント時刻を更新
  }
  // NEGボタン
  if (status_button_neg == CLICK) {
    if (mutex_button == NEG) {
      on_button_neg_click(); // 短クリック処理を呼び出す
      mutex_button = NONE; // ミューテックスを解放
    }
    status_button_neg = RELEASE; // ステータスを解放に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == LONG) {
    if (mutex_button == NEG) mutex_button = NONE; // ミューテックスを解放
    status_button_neg = RELEASE; // ステータスを解放に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == PRESS && millis() - msec_button_neg_last > 500) { // ボタンが500msec以上長押しされた
    status_button_neg = HOLD; // ステータスを長押し中に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == HOLD && millis() - msec_button_neg_last > 500) { // 長押しのまま500msec以上が経過した
    if (mutex_button == NEG) on_button_neg_hold(); // 長押しリピート処理を呼び出す
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  // POSボタン
  if (status_button_pos == CLICK) {
    if (mutex_button == POS) {
      on_button_pos_click(); // 短クリック処理を呼び出す
      mutex_button = NONE; // ミューテックスを解放
    }
    status_button_pos = RELEASE; // ステータスを解放に遷移
    msec_button_pos_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_pos == LONG) {
    if (mutex_button == POS) mutex_button = NONE; // ミューテックスを解放
    status_button_pos = RELEASE; // ステータスを解放に遷移
    msec_button_pos_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_pos == PRESS && millis() - msec_button_pos_last > 500) { // ボタンが500msec以上長押しされた
    status_button_pos = HOLD; // ステータスを長押し中に遷移
    msec_button_pos_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_pos == HOLD && millis() - msec_button_pos_last > 500) { // 長押しのまま500msec以上が経過した
    if (mutex_button == POS) on_button_pos_hold(); // 長押しリピート処理を呼び出す
    msec_button_pos_last = millis(); // 前回ボタンイベント時刻を更新
  }

  
  
  
}

// LCD表示
void display_mode_operation() {
    lcd.setCursor(0, 1); lcd.print("[L:");
    lcd.setCursor(7, 1); lcd.print("][H:");
    lcd.setCursor(15, 1); lcd.print("]");
  switch (mode_operation) {
    case IDLE:
      lcd.setCursor(0, 0); lcd.print("IDL");
      lcd.noCursor(); lcd.noBlink();
      break;
    case ACTIVE:
      lcd.setCursor(0, 0); lcd.print("ACT");
      lcd.noCursor(); lcd.noBlink();
      break;
    case SETTING_TERM:
      lcd.setCursor(0, 0); lcd.print("SET");
      lcd.setCursor((selected_term==LO)?1:9, 1); lcd.cursor(); lcd.blink();
      break;
    case SETTING_LO:
      lcd.setCursor(0, 0); lcd.print("SET");
      lcd.setCursor(6, 1); lcd.cursor(); lcd.blink();
      break;
    case SETTING_HI:
      lcd.setCursor(0, 0); lcd.print("SET");
      lcd.setCursor(14, 1); lcd.cursor(); lcd.blink();
      break;
  }
}

void display_val_LO() {
  lcd.setCursor(3, 1);
  if (val_LO < -99) { lcd.print(val_LO);
  } else if (val_LO < -9)  { lcd.print("- "); lcd.print(abs(val_LO));
  } else if (val_LO < 0)   { lcd.print("-  "); lcd.print(abs(val_LO));
  } else if (val_LO < 10)  { lcd.print("+  "); lcd.print(val_LO);
  } else if (val_LO < 100) { lcd.print("+ "); lcd.print(val_LO);
  } else {                   lcd.print("+"); lcd.print(val_LO); }
  lcd.setCursor(6, 1);
}

void display_val_HI() {
  lcd.setCursor(11, 1);
  if (val_HI < -99) { lcd.print(val_HI);
  } else if (val_HI < -9)  { lcd.print("- "); lcd.print(abs(val_HI));
  } else if (val_HI < 0)   { lcd.print("-  "); lcd.print(abs(val_HI));
  } else if (val_HI < 10)  { lcd.print("+  "); lcd.print(val_HI);
  } else if (val_HI < 100) { lcd.print("+ "); lcd.print(val_HI);
  } else {                   lcd.print("+"); lcd.print(val_HI); }
  lcd.setCursor(14, 1);
}

void display_curr_temp(double temp) {
  lcd.setCursor(4, 0);
  if (       temp <= -100.0) { lcd.print(temp);
  } else if (temp <= -10.0 ) { lcd.print("- "); lcd.print(abs(temp)); 
  } else if (temp <    0.0 ) { lcd.print("-  "); lcd.print(abs(temp));
  } else if (temp <   10.0 ) { lcd.print("+  "); lcd.print(temp);
  } else if (temp <  100.0 ) { lcd.print("+ "); lcd.print(temp);
  } else {                     lcd.print("+"); lcd.print(temp); }
  lcd.setCursor(10, 0); lcd.write(byte(0)); lcd.print("C");
}


// ACTボタンの動作定義
void on_button_act_click() {
  if (mutex_button != ACT) return; // ミューテックスがACTでなければ無視
  switch (mode_operation) {
    case IDLE: // ACTIVEモードに遷移
      mode_operation = ACTIVE;
      display_mode_operation();
      break;
    case ACTIVE: // IDLEモードに遷移
      mode_operation = IDLE;
      display_mode_operation();
      break;
    case SETTING_TERM: // LOかHIの値設定モードに遷移
      if (selected_term == LO) mode_operation = SETTING_LO;
      else mode_operation = SETTING_HI;
      display_mode_operation();
      break;
    case SETTING_LO: // 現在のLO値を記憶し設定項目選択モードに遷移
      EEPROM.put(0, val_LO);
      mode_operation = SETTING_TERM;
      display_mode_operation();
      break;
    case SETTING_HI: // 現在のHI値を記憶し設定項目選択モードに遷移
      EEPROM.put(2, val_HI);
      mode_operation = SETTING_TERM;
      display_mode_operation();
      break;
    default:
      display_mode_operation();
      break;
  }
}

void on_button_act_hold() {
  if (mutex_button != ACT) return; // ミューテックスがACTでなければ無視
  switch (mode_operation) {
    case IDLE:
      mode_operation = SETTING_TERM; // IDLEなら設定項目選択モードに遷移
      selected_term = LO;
      display_mode_operation();
      break;
    case SETTING_TERM:
      mode_operation = IDLE; // 設定項目選択モードならIDLEに遷移
      display_mode_operation();
      break;
    case SETTING_LO: // LO値設定モードならEEPROMからLO値を復元して設定項目選択モードに遷移
      EEPROM.get(0, val_LO);
      display_val_LO(); // val_LOの再表示
      mode_operation = SETTING_TERM;
      display_mode_operation();
      break;
    case SETTING_HI: // HI値設定モードならEEPROMからHI値を復元して設定項目選択モードに遷移
      EEPROM.get(2, val_HI);
      display_val_HI(); // val_HIの再表示
      mode_operation = SETTING_TERM;
      display_mode_operation();
      break;
    default: break;
  }
}

void button_act_changed() {
  if (millis() - msec_button_act_last < 20) return; // 前回ボタンイベント時刻から20msec未満なら無視
  if (digitalRead(PIN_SW_ACT) == LOW) { // ボタン押下
    if (status_button_act != RELEASE) return; // ボタンが解放でなければ無視
    msec_button_act_last = millis();
    status_button_act = PRESS; // ステータスをPRESSに遷移
    if (mutex_button == NONE) mutex_button = ACT; // ミューテックス取得
  } else { // ボタン解放
    if (status_button_act == PRESS) { // 短押し中だった
        msec_button_act_last = millis();
        status_button_act = CLICK; // ステータスをクリックに遷移
    } else if (status_button_act == HOLD) { // 長押し中だった
      msec_button_act_last = millis();
      status_button_act = LONG; // ステータスを長クリックに遷移
    }
  }
}

// NEGボタンの動作定義
void on_button_neg_click() {
  if (mutex_button != NEG) return; // ミューテックスがNEGでなければ無視
  switch (mode_operation) {
    case SETTING_TERM: selected_term = LO; display_mode_operation(); break;
    case SETTING_LO: val_LO--; val_LO = (val_LO<-999)?-999:val_LO; display_val_LO(); break;
    case SETTING_HI: val_HI--; val_HI = (val_HI<-999)?-999:(val_HI<=val_LO)?val_LO+1:val_HI; display_val_HI(); break;
    default: break;
  }
}

void on_button_neg_hold() {
  if (mutex_button != NEG) return; // ミューテックスがNEGでなければ無視
  switch (mode_operation) {
    case SETTING_LO: val_LO -= 10; val_LO = (val_LO<-999)?-999:val_LO; display_val_LO(); break;
    case SETTING_HI: val_HI -= 10; val_HI = (val_HI<-999)?-999:(val_HI<=val_LO)?val_LO+1:val_HI; display_val_HI(); break;
    default: break;
  }
}

void button_neg_changed() {
  if (millis() - msec_button_neg_last < 20) return; // 前回ボタンイベント時刻から20msec未満なら無視
  if (digitalRead(PIN_SW_NEG) == LOW) { // ボタン押下
    if (status_button_neg != RELEASE) return; // ボタンが解放でなければ無視
    msec_button_neg_last = millis();
    status_button_neg = PRESS; // ステータスをPRESSに遷移
    if (mutex_button == NONE) mutex_button = NEG; // ミューテックス取得
  } else { // ボタン解放
    if (status_button_neg == PRESS) { // 短押し中だった
        msec_button_neg_last = millis();
        status_button_neg = CLICK; // ステータスをクリックに遷移
    } else if (status_button_neg == HOLD) { // 長押し中だった
      msec_button_neg_last = millis();
      status_button_neg = LONG; // ステータスを長クリックに遷移
    }
  }
}

// POSボタンの動作定義
void on_button_pos_click() {
  if (mutex_button != POS) return; // ミューテックスがPOSでなければ無視
  switch (mode_operation) {
    case SETTING_TERM: selected_term = HI; display_mode_operation(); break;
    case SETTING_LO: val_LO++; val_LO = (val_LO>=val_HI)?val_HI-1:val_LO; display_val_LO(); break;
    case SETTING_HI: val_HI++; val_HI = (val_HI>999)?999:val_HI; display_val_HI(); break;
    default: break;
  }
}

void on_button_pos_hold() {
  if (mutex_button != POS) return; // ミューテックスがPOSでなければ無視
  switch (mode_operation) {
    case SETTING_LO: val_LO += 10; val_LO = (val_LO>=val_HI)?val_HI-1:val_LO; display_val_LO(); break;
    case SETTING_HI: val_HI += 10; val_HI = (val_HI>999)?999:val_HI; display_val_HI(); break;
    default: break;
  }
}

void button_pos_changed() {
  if (millis() - msec_button_pos_last < 20) return; // 前回ボタンイベント時刻から20msec未満なら無視
  if (digitalRead(PIN_SW_POS) == LOW) { // ボタン押下
    if (status_button_pos != RELEASE) return; // ボタンが解放でなければ無視
    msec_button_pos_last = millis();
    status_button_pos = PRESS; // ステータスをPRESSに遷移
    if (mutex_button == NONE) mutex_button = POS; // ミューテックス取得
  } else { // ボタン解放
    if (status_button_pos == PRESS) { // 短押し中だった
        msec_button_pos_last = millis();
        status_button_pos = CLICK; // ステータスをクリックに遷移
    } else if (status_button_pos == HOLD) { // 長押し中だった
      msec_button_pos_last = millis();
      status_button_pos = LONG; // ステータスを長クリックに遷移
    }
  }
}