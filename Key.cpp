/*
  Key.cpp
*/
#include "Arduino.h"
#include "Key.h"

// コンストラクタ
Key::Key(unsigned int p) {
  // private の初期化
  isParalyzing = false; // 麻痺中ではない
  ms_paralyzed = 0; // 麻痺開始時刻
  ms_pressed = 0; // 押下開始時刻
  is_press_attached = false; // callback_pressedがアタッチされていない
  is_click_attached = false; // callback_clickedがアタッチされていない
  is_repeat_attached = false; // callback_repeatedがアタッチされていない
  is_long_attached = false; // callback_long_clickedがアタッチされていない
  is_release_attached = false; // callback_releasedがアタッチされていない

  // public の初期化
  pin = p; // ピン番号を割り当てる
  time_paralyzing = 5; // 5ミリ秒間麻痺する
  time_pressing = 500; // 初回キーリピートまで500ミリ秒
  time_repeating = 500; // キーリピート間隔は500ミリ秒
  status = RELEASED; // 現在のキーの状態

  pinMode(pin, INPUT_PULLUP); // ピンをプルアップ入力にセット
}

// publicメンバー関数

// 割り込み発生時の麻痺開始処理
void Key::paralyze() {
  if (!isParalyzing) {
    isParalyzing = true;
    ms_paralyzed = millis();
  }
}

// atacchInterruptのラッパー
void Key::attachPinInterrupt(void(* func)(void)) {
  attachInterrupt(digitalPinToInterrupt(pin), func, CHANGE);
}

// 押下状態遷移時のコールバック関数登録
void Key::attachCallback_Pressed(void(* func)(void)) {
  callback_pressed = func;
  is_press_attached = true;
}

// 短押し発生時のコールバック関数登録
void Key::attachCallback_Clicked(void(* func)(void)) {
  callback_clicked = func;
  is_click_attached = true;
}

// 保持状態遷移時のコールバック関数登録
void Key::attachCallback_Holded(void(* func)(void)) {
  callback_holded = func;
  is_hold_attached = true;
}

// キーリピート発生時のコールバック関数登録
void Key::attachCallback_Repeated(void(* func)(void)) {
  callback_repeated = func;
  is_repeat_attached = true;
}

// 長押し発生時のコールバック関数登録
void Key::attachCallback_LongClicked(void(* func)(void)) {
  callback_long_clicked = func;
  is_long_attached = true;
}

// キーリリース時のコールバック関数登録
void Key::attachCallback_Released(void(* func)(void)) {
  callback_released = func;
  is_release_attached = true;
}

// 各種時間の設定関数
void Key::setTimeParalyzing(unsigned int t) {time_paralyzing = t;} // 麻痺時間の設定
void Key::setTimePressing(unsigned int t) {time_pressing = t;} // 押下から保持までの時間の設定
void Key::setTimeRepeating(unsigned int t) {time_repeating = t;} // 保持後のキーリピート時間の設定

// キー状態の取得
KEY_STATUS Key::readKeyStatus(void) {return status;} // 現在のキー状態の取得

// キーのチェックと状態遷移時に登録されたコールバック関数を呼び出す
void Key::check() {
  switch (status) { // キー状態が
    default:
    case RELEASED: // 解放中
      if (isParalyzing && millis() - ms_paralyzed > time_paralyzing) { // 規定時間を超えて麻痺してるなら
        if (digitalRead(pin) == LOW) { // ピン電圧がLOWなので押下とみなす
          if (is_press_attached) callback_pressed(); // 押下時コールバックが設定されてれば呼ぶ
          status = PRESSED; // 状態を押下中に更新
          ms_pressed = millis(); // 押下開示時刻を更新
        } else { }
        isParalyzing = false; // 麻痺を解除
      }
      break;
    case PRESSED: // 押下中
      if (isParalyzing) { // 麻痺中で
        if (millis() - ms_paralyzed > time_paralyzing) { // 規定時間を超えて麻痺してる
          if (digitalRead(pin) == HIGH) { // ピン電圧がHIGHなので解放とみなす
            if (is_click_attached) callback_clicked(); // クリック時（キーリピート前に解放）コールバックが設定されてれば呼ぶ
            if (is_release_attached) callback_released(); // キーリリース時コールバックが設定されてれば呼ぶ
            status = RELEASED; // 状態を解放中に更新
          } else { } // ピン電圧がLOWなので押下とみなすが「押下中 -> 押下」なので何もしない
          isParalyzing = false; // 麻痺を解除
        } // 麻痺中だがまだ規定時間を超えていない
      } else { // 麻痺中ではない（つまり、ただ押下され続けてるだけ）
        if (millis() - ms_pressed > time_pressing) { // 保持時間を超えて押下され続けてた
          if (is_hold_attached) callback_holded(); // 保持開始時コールバックが設定されてれば呼ぶ
          if (is_repeat_attached) callback_repeated(); // キーリピート時コールバックが設定されてれば呼ぶ
          status = HOLDED; // 状態を保持に更新
          ms_pressed = millis(); // 押下開始時刻を保持開始時刻に更新
        } // 押下中だがまだ保持時間に達していない
      }
      break;
    case HOLDED: // 保持中
      if (isParalyzing) { // 麻痺中で
        if (millis() - ms_paralyzed > time_paralyzing) { // 規定時間を超えて麻痺してる
          if (digitalRead(pin) == HIGH) { // ピン電圧がHIGHなので解放とみなす
            if (is_long_attached) callback_long_clicked(); // ロングクリック時（キーリピート中に解放）コールバックが設定されてれば呼ぶ
            if (is_release_attached) callback_released(); // キーリリース時コールバックが設定されてれば呼ぶ
            status = RELEASED; // 状態を解放中に更新
          } else { } // ピン電圧がLOWなので押下とみなすが「押下中 -> 押下」なので何もしない
          isParalyzing = false; // 麻痺を解除
        } // 麻痺中だがまだ規定時間を超えていない
      } else { // 麻痺中ではない（つまり、ただ押下され続けてるだけ）
        if (millis() - ms_pressed > time_repeating) { // 保持時間を超えて押下され続けてた
          if (is_repeat_attached) callback_repeated(); // キーリピート時コールバックが設定されてれば呼ぶ
          status = HOLDED; // 状態を保持に更新
          ms_pressed = millis(); // 押下開始時刻を保持開始時刻に更新
        } // 押下中だがまだ保持時間に達していない
      }
      break;
  }
}