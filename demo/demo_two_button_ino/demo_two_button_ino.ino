// for Switches
#define PIN_SW_NEG 1
#define PIN_SW_POS 7

enum Status_Button {UNKNOWN, RELEASE, PRESS, HOLD, CLICK, LONG};
volatile Status_Button status_button_neg = RELEASE,
                        status_button_pos = RELEASE;

enum Mutex_Button {NONE, ACT, NEG, POS};
volatile Mutex_Button mutex_button = NONE;

volatile unsigned long msec_button_neg_last = 0,
                        msec_button_pos_last = 0;

long value = 0, value_prev = 0; 


void setup() {
  Serial.begin(9600);
  while (!Serial) {;}

  pinMode(PIN_SW_NEG, INPUT_PULLUP);
  pinMode(PIN_SW_POS, INPUT_PULLUP);
  msec_button_neg_last = 0;
  msec_button_pos_last = 0;

  attachInterrupt(digitalPinToInterrupt(PIN_SW_NEG), button_neg_changed, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SW_POS), button_pos_changed, CHANGE);

  Serial.println(value);
}

void loop() {
  // ボタン動作のロジック
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

  if (value != value_prev) {
    Serial.print(value); Serial.print(" Mutex=");
    switch (mutex_button) {
      case NEG: Serial.println("NEG"); break;
      case POS: Serial.println("POS"); break;
      case ACT: Serial.println("ACT"); break;
      default: Serial.println("NONE"); break;
    }
    value_prev = value;
  }

  
}

void on_button_neg_click() {
  if (mutex_button != NEG) return; // ミューテックスがNEGでなければ無視
  value--;
}

void on_button_neg_hold() {
  if (mutex_button != NEG) return; // ミューテックスがNEGでなければ無視
  value -= 10;
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


void on_button_pos_click() {
  if (mutex_button != POS) return; // ミューテックスがPOSでなければ無視
  value++;
}

void on_button_pos_hold() {
  if (mutex_button != POS) return; // ミューテックスがPOSでなければ無視
  value += 10;
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