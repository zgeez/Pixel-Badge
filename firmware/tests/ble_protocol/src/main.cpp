/* SPDX-License-Identifier: MIT */
#include "badge/ble_protocol.hpp"
#include <zephyr/ztest.h>
#include <errno.h>
#include <string.h>
using namespace badge;

static ConfigCommand accept(BleProtocol &protocol, uint8_t id, Field field)
{
	uint8_t packet[] = {1, id, 0, 0, 0, static_cast<uint8_t>(field)};
	ConfigCommand command;
	zassert_ok(protocol.accept(packet, sizeof(packet), true, command));
	return command;
}
ZTEST(ble_protocol, test_reject_unauthenticated_and_unstaged_requests)
{
	BleProtocol protocol;
	const uint8_t brightness = 64;
	zassert_equal(protocol.stage(Field::kBrightness, &brightness, 1, false), -EACCES);
	const uint8_t request[] = {1, 1, 0, 0, 0, 3};
	ConfigCommand untouched;
	untouched.id = 99;
	zassert_equal(protocol.accept(request, sizeof(request), false, untouched), -EACCES);
	zassert_equal(protocol.accept(request, sizeof(request), true, untouched), -EINVAL);
	zassert_equal(untouched.id, 99);
	zassert_false(protocol.busy());
}
ZTEST(ble_protocol, test_field_bounds_and_qr_fit)
{
	BleProtocol protocol;
	const char *valid = "HTTPS://A.B/X";
	zassert_ok(protocol.stage(Field::kUrl, reinterpret_cast<const uint8_t *>(valid),
				  strlen(valid), true));
	const char *invalid = "HTTPS://EXAMPLE.COM/x";
	zassert_equal(protocol.stage(Field::kUrl, reinterpret_cast<const uint8_t *>(invalid),
				     strlen(invalid), true),
		      -EINVAL);
	uint8_t value = 0;
	zassert_equal(protocol.stage(Field::kBrightness, &value, 1, true), -EINVAL);
	value = 100;
	zassert_equal(protocol.stage(Field::kBrightness, &value, 1, true), -EINVAL);
	value = 2;
	zassert_equal(protocol.stage(Field::kDefaultMode, &value, 1, true), -EINVAL);
	zassert_equal(protocol.stage(Field::kSchema, &value, 1, true), -EINVAL);
	zassert_equal(protocol.stage(Field::kUrl, nullptr, 1000, true), -EINVAL);
}
ZTEST(ble_protocol, test_name_fragments_must_be_complete_and_contiguous)
{
	BleProtocol protocol;
	uint8_t first[20] = {0, 32}, second[16] = {18, 32};
	memset(first + 2, 'A', 18);
	memset(second + 2, 'B', 14);
	zassert_ok(protocol.stage(Field::kName, first, sizeof(first), true));
	ConfigCommand command;
	const uint8_t request[] = {1, 1, 0, 0, 0, 2};
	zassert_equal(protocol.accept(request, sizeof(request), true, command), -EINVAL);
	second[0] = 17;
	zassert_equal(protocol.stage(Field::kName, second, sizeof(second), true), -EINVAL);
	second[0] = 18;
	zassert_ok(protocol.stage(Field::kName, second, sizeof(second), true));
	command = accept(protocol, 1, Field::kName);
	zassert_equal(command.length, 32);
	zassert_mem_equal(command.data, first + 2, 18);
	zassert_mem_equal(command.data + 18, second + 2, 14);
}
ZTEST(ble_protocol, test_name_restart_cannot_reuse_a_partial_or_invalid_value)
{
	BleProtocol protocol;
	const uint8_t name[] = {0, 3, 'A', 'B', 'C'};
	zassert_ok(protocol.stage(Field::kName, name, sizeof(name), true));
	const uint8_t partial[] = {0, 3, 'X'};
	zassert_ok(protocol.stage(Field::kName, partial, sizeof(partial), true));
	const uint8_t request[] = {1, 1, 0, 0, 0, 2};
	ConfigCommand command;
	zassert_equal(protocol.accept(request, sizeof(request), true, command), -EINVAL);
	const uint8_t invalid[] = {1, 3, 0, 'Z'};
	zassert_equal(protocol.stage(Field::kName, invalid, sizeof(invalid), true), -EINVAL);
	zassert_equal(protocol.accept(request, sizeof(request), true, command), -EINVAL);
}
ZTEST(ble_protocol, test_busy_duplicate_and_status_encoding)
{
	BleProtocol protocol;
	const uint8_t level = 64;
	zassert_ok(protocol.stage(Field::kBrightness, &level, 1, true));
	const auto command = accept(protocol, 1, Field::kBrightness);
	zassert_equal(protocol.stage(Field::kBrightness, &level, 1, true), -EBUSY);
	uint8_t status[kBleStatusSize];
	protocol.status(status, 3);
	const uint8_t pending[] = {1, 1, 0, 0, 0, 3, 1, 0x8d, 0xff, 0xff, 0xff, 3};
	zassert_mem_equal(status, pending, sizeof(pending));
	protocol.complete(command, -EACCES);
	protocol.status(status, 1);
	const uint8_t failed[] = {1, 1, 0, 0, 0, 3, 2, 0xf3, 0xff, 0xff, 0xff, 1};
	zassert_mem_equal(status, failed, sizeof(failed));
	zassert_ok(protocol.stage(Field::kBrightness, &level, 1, true));
	const uint8_t duplicate[] = {1, 1, 0, 0, 0, 3};
	ConfigCommand ignored;
	zassert_equal(protocol.accept(duplicate, sizeof(duplicate), true, ignored), -EALREADY);
	(void)accept(protocol, 2, Field::kBrightness);
}
ZTEST(ble_protocol, test_disconnect_discards_staging_and_late_completion)
{
	BleProtocol protocol;
	const uint8_t mode = 1;
	zassert_ok(protocol.stage(Field::kDefaultMode, &mode, 1, true));
	const auto previous = accept(protocol, 1, Field::kDefaultMode);
	protocol.reset();
	zassert_false(protocol.current(previous));
	protocol.complete(previous, 0);
	uint8_t status[kBleStatusSize];
	protocol.status(status, 1);
	zassert_equal(status[1], 0);
	zassert_equal(status[6], 0);
	zassert_ok(protocol.stage(Field::kDefaultMode, &mode, 1, true));
	const auto next = accept(protocol, 1, Field::kDefaultMode);
	protocol.complete(previous, -EIO);
	zassert_true(protocol.current(next));
	protocol.complete(next, 0);
	zassert_false(protocol.busy());
}
ZTEST(ble_protocol, test_apply_wire_version_length_and_zero_id)
{
	BleProtocol protocol;
	const uint8_t level = 16;
	zassert_ok(protocol.stage(Field::kBrightness, &level, 1, true));
	uint8_t packet[] = {2, 1, 0, 0, 0, 3};
	ConfigCommand output;
	zassert_equal(protocol.accept(packet, sizeof(packet), true, output), -EINVAL);
	packet[0] = 1;
	zassert_equal(protocol.accept(packet, 5, true, output), -EINVAL);
	packet[1] = 0;
	zassert_equal(protocol.accept(packet, sizeof(packet), true, output), -EALREADY);
	packet[1] = 0x78;
	packet[2] = 0x56;
	packet[3] = 0x34;
	packet[4] = 0x12;
	zassert_ok(protocol.accept(packet, sizeof(packet), true, output));
	zassert_equal(output.id, 0x12345678);
}
ZTEST_SUITE(ble_protocol, nullptr, nullptr, nullptr, nullptr, nullptr);
