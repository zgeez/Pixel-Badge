/* SPDX-License-Identifier: MIT */
#pragma once
#include "badge/ntag.hpp"

namespace badge
{
int bench_bus_lock();
void bench_bus_unlock();
int prepare_nfc_transport();
NtagIo nfc_transport();
int nfc_bringup_check();
void nfc_log_field();
} // namespace badge
