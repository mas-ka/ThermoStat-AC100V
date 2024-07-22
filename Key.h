/*
  Key.h
  キーはINPUT_PULLUPで接続されているものとする
  キーが開放状態から押下された瞬間に「callback_pressed」が呼ばれて押下状態に遷移する
  押下開始から「time_pressing」以内に解放された瞬間に「callback_clicked」と「callback_released」が呼ばれて開放状態に遷移する
  押下開始から押しっぱなしで「time_pressing」が経過した瞬間に「callback_holded」と「callback_repeated」が呼ばれて保持状態に遷移する
  保持状態では押しっぱなしで「time_repeating」が経過する毎に「callback_repeated」が呼ばれる
  保持状態で解放された瞬間に「callback_long_clicked」と「callback_released」が呼ばれて開放状態に遷移する
*/
#ifndef __KEY_H_INCLUDED__
#define __KEY_H_INCLUDED__

#include "Arduino.h"

enum KEY_STATUS {RELEASED, PRESSED, HOLDED}; // キー状態の列挙 {解放, 押下, 保持}

class Key {
  private:
    unsigned int time_paralyzing; // 麻痺するミリ秒
    unsigned int time_pressing; // 押下開始から保持に遷移するまでのミリ秒
    unsigned int time_repeating; // キーリピート間隔のミリ秒
    KEY_STATUS status; // 現在のキーの状態

    volatile boolean isParalyzing; // 麻痺中かどうか
    volatile unsigned long ms_paralyzed; // 麻痺開始時刻

    unsigned long ms_pressed; // 押下開始時刻
    boolean is_press_attached; // callback_pressedがアタッチされてるかどうか
    void(* callback_pressed)(void); // press時のコールバック関数
    boolean is_click_attached; // callback_clickedがアタッチされてるかどうか
    void(* callback_clicked)(void); // click時のコールバック関数
    boolean is_hold_attached; // callback_clickedがアタッチされてるかどうか
    void(* callback_holded)(void); // hold時のコールバック関数
    boolean is_repeat_attached; // callback_repeatedがアタッチされてるかどうか
    void(* callback_repeated)(void); // repeat時のコールバック関数
    boolean is_long_attached; // callback_long_clickedがアタッチされてるかどうか
    void(* callback_long_clicked)(void); // long_click時のコールバック関数
    boolean is_release_attached; // callback_releasedがアタッチされてるかどうか
    void(* callback_released)(void); // release時のコールバック関数

  public:
    unsigned int pin; // 割り当てられてるピン番号

    // コンストラクタ
    Key(unsigned int); // ピン番号

    // 割り込み処理のコールバック登録
    void paralyze(void);
    void attachPinInterrupt(void(* func)(void));
    
    // キー動作のコールバック登録
    void attachCallback_Pressed(void(* func)(void));
    void attachCallback_Clicked(void(* func)(void));
    void attachCallback_Holded(void(* func)(void));
    void attachCallback_Repeated(void(* func)(void));
    void attachCallback_LongClicked(void(* func)(void));
    void attachCallback_Released(void(* func)(void));

    // 各種時間の設定
    void setTimeParalyzing(unsigned int); // 麻痺時間の設定
    void setTimePressing(unsigned int); // 押下から保持までの時間の設定
    void setTimeRepeating(unsigned int); // 保持後のキーリピート時間の設定

    // キー状態の取得
    KEY_STATUS readKeyStatus(void); // 現在のキー状態の取得
    
    // キーのチェックと動作状態遷移によるコールバックの呼び出し
    void check(void);
};

#endif
