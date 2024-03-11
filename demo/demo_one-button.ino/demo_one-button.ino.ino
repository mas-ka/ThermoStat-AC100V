// for Switches
#define PIN_SW_NEG 0

enum Status_Button {UNKNOWN, RELEASE, PRESS, HOLD, CLICK, LONG};
volatile Status_Button status_button_neg = RELEASE;

volatile unsigned long msec_button_neg_last = 0;

volatile unsigned int value = 0, value_prev = 0; 


void setup() {
  Serial.begin(9600);
  while (!Serial) {;}

  pinMode(PIN_SW_NEG, INPUT_PULLUP);
  msec_button_neg_last = 0;

  attachInterrupt(digitalPinToInterrupt(PIN_SW_NEG), button_neg_changed, CHANGE);

  Serial.println(value);
}

void loop() {
  // ボタン動作のロジック
  if (status_button_neg == CLICK) {
    on_button_neg_click(); // 短クリック処理を呼び出す
    status_button_neg = RELEASE; // ステータスを解放に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == LONG) {
    status_button_neg = RELEASE; // ステータスを解放に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == PRESS && millis() - msec_button_neg_last > 500) { // ボタンが500msec以上長押しされた
    status_button_neg = HOLD; // ステータスを長押し中に遷移
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }
  if (status_button_neg == HOLD && millis() - msec_button_neg_last > 500) { // 長押しのまま500msec以上が経過した
    on_button_neg_hold(); // 長押しリピート処理を呼び出す
    msec_button_neg_last = millis(); // 前回ボタンイベント時刻を更新
  }

  if (value != value_prev) {
    Serial.println(value);
    value_prev = value;
  }

  
}

void on_button_neg_click() {
  value++;
}

void on_button_neg_hold() {
  value += 10;
}

void button_neg_changed() {
  if (millis() - msec_button_neg_last < 20) return; // 前回ボタンイベント時刻から20msec未満なら無視
  if (digitalRead(PIN_SW_NEG) == LOW) { // ボタン押下
    if (status_button_neg != RELEASE) return; // ボタンが解放でなければ無視
    msec_button_neg_last = millis();
    status_button_neg = PRESS; // ステータスをPRESSに遷移
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