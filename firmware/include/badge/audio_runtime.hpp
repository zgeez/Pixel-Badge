/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/audio.hpp"
#include <zephyr/bluetooth/conn.h>
namespace badge
{
/* All capture/display functions are main-loop owned. BT callbacks only queue
 * authenticated requests or revoke permission under the audio mutex. */
void start_audio_service();
void audio_connected(bt_conn *conn);
void audio_disconnected();
bool audio_pairing_window();
bool audio_new_pairing();
bool audio_button(bool long_press);
bool audio_overlay(Frame &frame);
bool audio_recording();
bool audio_ui_busy();
bool audio_capture_active();
void audio_tick(bool display_healthy, bool visual, bool spectrum);
void audio_display_result(int result);
void audio_halt();
void audio_visual_frame(Frame &frame, bool spectrum);
} // namespace badge
