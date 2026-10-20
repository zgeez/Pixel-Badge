/*
 * SPDX-License-Identifier: MIT
 *
 * Rev A matrix bring-up: TLC chain readback with outputs disabled, and the
 * button-stepped matrix bench. See src/app/matrix_diag.cpp.
 */

#pragma once

struct device;

namespace badge
{

/* Checks MTX_SAFE_SENSE and the TLC59283 chain readback. Returns the number
 * of failed checks; a no-op on boards without the Rev A matrix. */
int matrix_chain_check();

/* Staged matrix tests, stepped with the buttons (CONFIG_APP_MATRIX_BENCH). */
int run_matrix_bench(const struct device *display);

} // namespace badge
