// EEPROM
#include <EEPROM.h>

// I2C液晶
#include <LiquidCrystal_I2C.h>
#define LCD_WIDTH 16
volatile LiquidCrystal_I2C lcd(0x27, LCD_WIDTH , 2);

// カスタムキャラクタ
#define cc_degree 0
#define cc_L      1
#define cc_H      2
#define cc_O      3
#define cc_RMODE  4
#define cc_OP_0   5
#define cc_OP_1   6
#define cc_OP_2   7

byte cmap_degree[8] = {0b00010, 0b00101, 0b00101, 0b00010, 0b00000, 0b00000, 0b00000, 0b00000};
byte cmap_invA[8] =   {0b11111,	0b11011, 0b10101, 0b10001, 0b10101, 0b10101, 0b11111, 0b00000};
byte cmap_invC[8] =   {0b11111, 0b11011, 0b10101, 0b10111, 0b10101, 0b11011, 0b11111, 0b00000};
byte cmap_invD[8] =   {0b11111, 0b10011, 0b10101, 0b10101, 0b10101, 0b10011, 0b11111, 0b00000};
byte cmap_invE[8] =   {0b11111, 0b10001, 0b10111, 0b10001, 0b10111, 0b10001, 0b11111, 0b00000};
byte cmap_invF[8] =   {0b11111, 0b10001, 0b10111, 0b10001, 0b10111, 0b10111, 0b11111, 0b00000};
byte cmap_invH[8] =   {0b11111, 0b10101, 0b10101, 0b10001, 0b10101, 0b10101, 0b11111, 0b00000};
byte cmap_invI[8] =   {0b11111, 0b11011, 0b11011, 0b11011, 0b11011, 0b11011, 0b11111, 0b00000};
byte cmap_invL[8] =   {0b11111, 0b10111, 0b10111, 0b10111, 0b10111, 0b10001, 0b11111, 0b00000};
byte cmap_invN[8] =   {0b11111, 0b10101, 0b10001, 0b10101, 0b10101, 0b10101, 0b11111, 0b00000};
byte cmap_invO[8] =   {0b11111, 0b10001, 0b10101, 0b10101, 0b10101, 0b10001, 0b11111, 0b00000};
byte cmap_invR[8] =   {0b11111, 0b10011, 0b10101, 0b10011, 0b10101, 0b10101, 0b11111, 0b00000};
byte cmap_invS[8] =   {0b11111, 0b11001, 0b10111, 0b11011, 0b11101, 0b10011, 0b11111, 0b00000};
byte cmap_invT[8] =   {0b11111, 0b10001, 0b11011, 0b11011, 0b11011, 0b11011, 0b11111, 0b00000};

// 熱電対
#include <SPI.h>
#include "Adafruit_MAX31855.h"
#define MAXCS     18
Adafruit_MAX31855 thermocouple(MAXCS);

double curr_temp, tmp_temp;
unsigned long msec_update_curr_temp_last = 0;

// リレー
#define PIN_RELAY 10
enum Mode_Relay {OFF, ON};
Mode_Relay mode_relay = OFF;

// 動作定義
enum Mode_operation {ERROR, IDLE, ACTIVE, SEL_LO, SEL_HI, SET_LO, SET_HI, PSEUDO_IDLE, PSEUDO_SEL_LO};
Mode_operation mode_operation = IDLE;

// ボタン定義
#include <VersatileSwitch.h> // 多用途スイッチライブラリ

#define PIN_SW_ACT 8
#define PIN_SW_NEG 7
#define PIN_SW_POS 9

VersatileSwitch btn_ACT(PIN_SW_ACT);
VersatileSwitch btn_NEG(PIN_SW_NEG);
VersatileSwitch btn_POS(PIN_SW_POS);

// ボタンの排他制御
enum Mutex {NONE, ACT, NEG, POS};
volatile Mutex mutex = NONE;

// 変数
short val_LO = 0, val_HI = 0; 


void setup() {
  //Serial.begin(9600);
  //while (!Serial) {;}
  delay(500);

  // Init LCD_I2C
  lcd.init(); lcd.setBacklight(255); lcd.clear(); lcd.noCursor();

  // カスタムキャラクタの設定（共通して使う分）
  lcd.createChar(cc_degree, cmap_degree);
  lcd.createChar(cc_H, cmap_invH);
  lcd.createChar(cc_L, cmap_invL);
  lcd.createChar(cc_O, cmap_invO);

  // EEPROMからLOとHIの設定値を取得
  EEPROM.get(0,val_LO); // 0-1番地に(short)LO
  EEPROM.get(2, val_HI); // 2-3番地に(short)HI

  // ボタンのコールバック登録
  btn_ACT.attachCallback_Pressed(on_act_pressed);
  btn_ACT.attachCallback_Clicked(on_act_clicked);
  btn_ACT.attachCallback_Held(on_act_held);
  btn_ACT.attachCallback_LongClicked(on_act_longclicked);

  btn_NEG.attachCallback_Pressed(on_neg_pressed);
  btn_NEG.attachCallback_Clicked(on_neg_clicked);
  btn_NEG.attachCallback_Repeated(on_neg_repeated);
  btn_NEG.attachCallback_LongClicked(on_neg_longclicked);

  btn_POS.attachCallback_Pressed(on_pos_pressed);
  btn_POS.attachCallback_Clicked(on_pos_clicked);
  btn_POS.attachCallback_Repeated(on_pos_repeated);
  btn_POS.attachCallback_LongClicked(on_pos_longclicked);

  // スイッチが離された際にミューテックスを解除する必要があるのだが、
  // それにReleasedコールバックを使うことはできない。
  // ReleasedはClickedやLongClickedの「前」にコールバックされるので、
  // Releasedでミューテックスを解除すると、
  // その後のClickedやLongClickedのコールバックにて
  // ミューテックスが取れていなかったことになり処理ができなくなる
  // そのため、btn_NEGとbtn_POSでは長押しに機能はないのだが、
  // リピート後に離されたときにミューテックスを解除するためだけに
  // LongClickedをアタッチする必要がある

  mutex = NONE; // 排他制御状態の初期化
  mode_operation = IDLE; // IDEL状態で初期化

  // リレー初期化
  pinMode(PIN_RELAY, OUTPUT);
  mode_relay = OFF;
  digitalWrite(PIN_RELAY, LOW);

  // MAX31855の初期化
  delay(1000); // wait for MAX chip to stabilize
  if (!thermocouple.begin()) {
    mode_operation = ERROR; // 動作状態をエラーにする
  }
  curr_temp = thermocouple.readCelsius();

  // LCD表示
  display_mode_operation();
  display_val_LO(); display_val_HI();
  display_curr_temp(curr_temp);
  display_mode_relay();
}

void loop() {
  // 温度の表示と制御
  if (millis() > msec_update_curr_temp_last + 500) { // 前回更新時から500msec以上経過した
    // 熱電対の状態監視
    tmp_temp = thermocouple.readCelsius(); // 仮変数に温度を取得
    if (isnan(tmp_temp)) { // 温度が取得できていなかった
      mode_relay = OFF; digitalWrite(PIN_RELAY, LOW); // とりあえずリレーを緊急断
      if (mode_operation == IDLE) { // 現在がアイドル状態モードなら
        mode_operation = ERROR; // エラー状態モードに遷移
        display_mode_operation();
      } // ※ SETおよびACTならモードは維持する
    } else { // 温度が取得できていた
      curr_temp = tmp_temp; // 読めた温度を本番変数へ移す
      if (mode_operation == ERROR) { // 現在がエラー状態モードなら
        mode_operation = IDLE; // アイドル状態モードに遷移
        display_mode_operation();
      } // ※ ACTIVE/SEL/SETモードならERRORに入らず現状のモードを維持する
    }

    // 温度表示
    msec_update_curr_temp_last = millis();
    display_curr_temp(curr_temp);

    // 温調
    if (mode_operation == ACTIVE) {
      double temp = thermocouple.readCelsius();
      if (temp < val_LO) { // 現在温度がL設定を下回ってる
        mode_relay = ON; digitalWrite(PIN_RELAY, HIGH); // リレーを入
        display_mode_relay();
      } else if (temp > val_HI) { // 現在温度がH設定を上回ってる
        mode_relay = OFF; digitalWrite(PIN_RELAY, LOW); // リレーを断
        display_mode_relay();
      }
    }
  }
  
  // ボタン動作のポーリング
  btn_ACT.poll();
  btn_NEG.poll();
  btn_POS.poll();

} // END of loop

// LCD表示
void display_mode_operation() {
  lcd.setCursor(0, 1); lcd.print("["); lcd.write(byte(cc_L)); lcd.print(" ");
  lcd.setCursor(7, 1); lcd.print("]["); lcd.write(byte(cc_H)); lcd.print(" ");
  lcd.setCursor(15, 1); lcd.print("]");
  switch (mode_operation) {
    case ERROR:
      lcd.createChar(cc_OP_0, cmap_invE); lcd.createChar(cc_OP_1, cmap_invR); lcd.createChar(cc_OP_2, cmap_invR); break;
    case IDLE: case PSEUDO_IDLE:
      lcd.createChar(cc_OP_0, cmap_invI); lcd.createChar(cc_OP_1, cmap_invD); lcd.createChar(cc_OP_2, cmap_invL); break;
    case ACTIVE:
      lcd.createChar(cc_OP_0, cmap_invA); lcd.createChar(cc_OP_1, cmap_invC); lcd.createChar(cc_OP_2, cmap_invT); break;
    case SEL_LO: case SEL_HI: case SET_LO: case SET_HI: case PSEUDO_SEL_LO:
      lcd.createChar(cc_OP_0, cmap_invS); lcd.createChar(cc_OP_1, cmap_invE); lcd.createChar(cc_OP_2, cmap_invT); break;
  }
  lcd.setCursor(0, 0); lcd.write(byte(cc_OP_0)); lcd.write(byte(cc_OP_1)); lcd.write(byte(cc_OP_2)); 
  control_cursor();
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
  if (isnan(temp)) { // 温度を取得できていなかった
    lcd.print("        "); lcd.setCursor(5, 0);
    uint8_t e = thermocouple.readError();
    if (e & MAX31855_FAULT_OPEN) lcd.print("TO"); // 回路オープン
    if (e & MAX31855_FAULT_SHORT_GND) lcd.print("SG"); // GNDショート
    if (e & MAX31855_FAULT_SHORT_VCC) lcd.print("SV"); // VCCショート
  } else { // 取得できていれば
    if (       temp <= -100.0) { lcd.print(temp);
    } else if (temp <= -10.0 ) { lcd.print("- "); lcd.print(abs(temp)); 
    } else if (temp <    0.0 ) { lcd.print("-  "); lcd.print(abs(temp));
    } else if (temp <   10.0 ) { lcd.print("+  "); lcd.print(temp);
    } else if (temp <  100.0 ) { lcd.print("+ "); lcd.print(temp);
    } else {                     lcd.print("+"); lcd.print(temp); }
    lcd.setCursor(10, 0); lcd.write(byte(cc_degree)); lcd.print("C");
  }
  control_cursor();  
}

void display_mode_relay() {
  switch (mode_relay) {
    case OFF:
      lcd.createChar(cc_RMODE, cmap_invF);
      lcd.setCursor(13, 0); lcd.write(byte(cc_O)); lcd.write(byte(cc_RMODE)); lcd.write(byte(cc_RMODE));
      break;
    case ON:
      lcd.createChar(cc_RMODE, cmap_invN);
      lcd.setCursor(13, 0); lcd.print(" "); lcd.write(byte(cc_O)); lcd.write(byte(cc_RMODE));
      break;
    default: break;
  }
  control_cursor();
}

void control_cursor() { // カーソル位置の制御
  switch (mode_operation) {
    case IDLE: case ACTIVE: case PSEUDO_IDLE:
      lcd.noCursor(); lcd.noBlink(); break;
    case SEL_LO: case PSEUDO_SEL_LO:
      lcd.setCursor(1, 1); lcd.cursor(); lcd.blink(); break;
    case SEL_HI:
      lcd.setCursor(9, 1); lcd.cursor(); lcd.blink(); break;
    case SET_LO:
      lcd.setCursor(6, 1); lcd.cursor(); lcd.noBlink(); break;
    case SET_HI:
      lcd.setCursor(14, 1); lcd.cursor(); lcd.noBlink(); break;
    default: break;
  }
}

// ACTボタンのコールバック関数
void on_act_pressed() {if (mutex == NONE) mutex = ACT;} // ミューテックスをACTが取得

void on_act_clicked() {
  if (mutex != ACT) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case IDLE: // ACTIVEモードに遷移
      mode_operation = ACTIVE;
      display_mode_operation();
      break;
    case ACTIVE: // IDLEモードに遷移
      mode_operation = IDLE;
      mode_relay = OFF; digitalWrite(PIN_RELAY, LOW); // 念のためリレーを断
      display_mode_operation();
      display_mode_relay();
      break;
    case SEL_LO: // SET_LOモードに遷移
      mode_operation = SET_LO;
      display_mode_operation();
      break;
    case SEL_HI: // SET_HIモードに遷移
      mode_operation = SET_HI;
      display_mode_operation();
      break;
    case SET_LO: // 現在のLO値を保存してSEL_LOモードに遷移
      EEPROM.put(0, val_LO); // 現在のLO値をEEPROMに保存
      mode_operation = SEL_LO;
      display_mode_operation();
      break;
    case SET_HI: // 現在のHI値を保存してSEL_HIモードに遷移
      EEPROM.put(2, val_HI); // 現在のHI値をEEPROMに保存
      mode_operation = SEL_HI;
      display_mode_operation();
      break;
    default: // 上記以外なら表示を更新だけする
      display_mode_operation();
      break;
  }
  mutex = NONE; // スイッチが離されたのでミューテックスを解除する
}

void on_act_held() {
  if (mutex != ACT) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case IDLE: // 表示だけをSEL_LOモードにするためにPSEUDO-SEL_LOモードに遷移
      mode_operation = PSEUDO_SEL_LO;
      display_mode_operation();
      break;
    case SEL_LO: case SEL_HI: // 表示だけIDLEモードにするためにPSEUDO_IDLEモードに遷移
      mode_operation = PSEUDO_IDLE;
      display_mode_operation();
      break;
    case SET_LO: // LO値だけを変更前の値に書き戻す
      EEPROM.get(0, val_LO); // 変更前のLO値をEEPROMから読み出す
      display_val_LO(); // val_LOの再表示
      break;
    case SET_HI: // HI値だけを変更前の値に書き戻す
      EEPROM.get(2, val_HI); // 変更前のHI値をEEPROMから読み出す
      display_val_HI(); // val_HIの再表示
      break;
    default: break;
  }
}

void on_act_longclicked() {
  if (mutex != ACT) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case IDLE: case PSEUDO_SEL_LO: // SEL_LOモードに遷移
      mode_operation = SEL_LO;
      display_mode_operation();
      break;
    case SEL_LO: case SEL_HI: case PSEUDO_IDLE: // IDLEモードに遷移
      mode_operation = IDLE;
      display_mode_operation();
      break;
    default: break;
  }
  mutex = NONE; // スイッチが離されたのでミューテックスを解除する
}

// NEGボタンのコールバック関数
void on_neg_pressed() {if (mutex == NONE) mutex = NEG;} // ミューテックスをNEGが取得

void on_neg_clicked() {
  if (mutex != NEG) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case SEL_HI: // SEL_LOモードに遷移
      mode_operation = SEL_LO;
      display_mode_operation();
      break;
    case SET_LO: // LO値を１減らす
      val_LO--; val_LO = (val_LO<-999)?-999:val_LO;
      display_val_LO();
      break;
    case SET_HI: // HI値を１減らす
      val_HI--; val_HI = (val_HI<-999)?-999:(val_HI<=val_LO)?val_LO+1:val_HI;
      display_val_HI();
      break;
    default: break;
  }
  mutex = NONE; // スイッチが離されたのでミューテックスを解除する
}

void on_neg_repeated() {
  if (mutex != NEG) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case SET_LO: // LO値を10減らす
      val_LO -= 10; val_LO = (val_LO<-999)?-999:val_LO;
      display_val_LO();
      break;
    case SET_HI: // HI値を10減らす
      val_HI -= 10; val_HI = (val_HI<-999)?-999:(val_HI<=val_LO)?val_LO+1:val_HI;
      display_val_HI();
      break;
    default: break;
  }
}

void on_neg_longclicked() {if (mutex == NEG) mutex = NONE;} // ミューテックスが取れていたならそれを解除する

// POSボタンのコールバック関数
void on_pos_pressed() {if (mutex == NONE) mutex = POS;} // ミューテックスをPOSが取得

void on_pos_clicked() {
  if (mutex != POS) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case SEL_LO: // SEL_HIモードに遷移
      mode_operation = SEL_HI;
      display_mode_operation();
      break;
    case SET_LO: // LO値を１増やす
      val_LO++; val_LO = (val_LO>=val_HI)?val_HI-1:val_LO;
      display_val_LO();
      break;
    case SET_HI: // HI値を１増やす
      val_HI++; val_HI = (val_HI>999)?999:val_HI;
      display_val_HI();
      break;
    default: break;
  }
  mutex = NONE; // スイッチが離されたのでミューテックスを解除する
}

void on_pos_repeated() {
  if (mutex != POS) return; // ミューテックスが取れてなければ何もしない
  switch (mode_operation) {
    case SET_LO: // LO値を10増やす
      val_LO += 10; val_LO = (val_LO>=val_HI)?val_HI-1:val_LO;
      display_val_LO();
      break;
    case SET_HI: // HI値を10増やす
      val_HI += 10; val_HI = (val_HI>999)?999:val_HI;
      display_val_HI();
      break;
    default: break;
  }
}

void on_pos_longclicked() {if (mutex == POS) mutex = NONE;} // ミューテックスが取れていたならそれを解除する
