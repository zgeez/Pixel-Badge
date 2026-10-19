/* SPDX-License-Identifier: MIT */
#include "badge/settings_store.hpp"
#include "badge/artwork_store.hpp"
#include <zephyr/drivers/flash.h>
#if defined(CONFIG_ARCH_POSIX)
#include <zephyr/drivers/flash/flash_simulator.h>
#endif

#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(badge_settings, CONFIG_APP_LOG_LEVEL);

namespace badge
{
namespace
{
ArtworkStore artwork_store;
uint32_t published_art_crc;
Artwork published_art_view{nullptr, 0};
bool artwork_write_fault;
const flash_area *artwork_area;
int initialize_artwork()
{
	artwork_write_fault = false;
	published_art_view = {nullptr, 0};
	int rc = flash_area_open(PARTITION_ID(artwork_storage), &artwork_area);
	if (rc != 0) {
		return rc;
	}
	const size_t write_unit = flash_get_write_block_size(artwork_area->fa_dev);
	if (artwork_area->fa_off % 4096 || write_unit == 0 || 4 % write_unit != 0) {
		return -ENOTSUP;
	}
	flash_pages_info page{};
	rc = flash_get_page_info_by_offs(artwork_area->fa_dev, artwork_area->fa_off, &page);
	if (rc != 0) {
		return rc;
	}
	if (page.size != 4096) {
		return -ENOTSUP;
	}
	if (artwork_area->fa_size < 2 * kArtworkBankBytes) {
		return -ENOSPC;
	}
#if defined(CONFIG_ARCH_POSIX)
	size_t size;
	const auto *base = static_cast<const uint8_t *>(
		flash_simulator_get_memory(artwork_area->fa_dev, &size));
	if (artwork_area->fa_off + artwork_area->fa_size > size) {
		return -EIO;
	}
	const auto *mapped = base + artwork_area->fa_off;
#else
	const auto *mapped = reinterpret_cast<const uint8_t *>(DT_REG_ADDR(DT_NODELABEL(flash0)) +
							       artwork_area->fa_off);
#endif
	artwork_store.initialize({nullptr, mapped,
				  [](void *, size_t offset, size_t size) {
					  return flash_area_flatten(artwork_area, offset, size);
				  },
				  [](void *, size_t offset, const void *bytes, size_t size) {
					  return flash_area_write(artwork_area, offset, bytes,
								  size);
				  }});
	return 0;
}
constexpr size_t kChunkBytes = 1024;
static uint8_t asset_buffer[kLegacyArtworkBytes]; // Main-thread load/store owner.
struct ChunkContext {
	size_t length;
	unsigned mask = 0;
	int error = 0;
};
int read_chunk(const char *key, size_t length, settings_read_cb read, void *argument, void *context)
{
	auto &chunk = *static_cast<ChunkContext *>(context);
	if (chunk.error) {
		return chunk.error;
	}
	if (key[0] < '0' || key[0] > '7' || key[1] != 0) {
		return 0;
	}
	const unsigned index = key[0] - '0';
	const size_t offset = index * kChunkBytes;
	if (offset >= chunk.length) {
		return 0;
	} // Old tail chunks are unreachable.
	const size_t expected =
		chunk.length - offset < kChunkBytes ? chunk.length - offset : kChunkBytes;
	if (length != expected) {
		chunk.error = -EIO;
		return chunk.error;
	}
	const ssize_t rc = read(argument, asset_buffer + offset, length);
	if (rc != static_cast<ssize_t>(length)) {
		chunk.error = rc < 0 ? static_cast<int>(rc) : -EIO;
		return chunk.error;
	}
	chunk.mask |= 1U << index;
	return 0;
}
int load_chunks(unsigned bank, size_t length)
{
	ChunkContext chunk{length};
	const int rc = settings_load_subtree_direct(bank == 0 ? "badge/art0" : "badge/art1",
						    read_chunk, &chunk);
	if (rc != 0) {
		return rc;
	}
	if (chunk.error) {
		return chunk.error;
	}
	return chunk.mask == ((1U << ((length + kChunkBytes - 1) / kChunkBytes)) - 1) ? 0 : -EIO;
}
int store_artwork(const uint8_t *data, size_t length)
{
	if (artwork_write_fault) {
		return -EIO;
	}
	uint8_t manifest[12];
	int rc = artwork_store.prepare(data, length, manifest);
	if (rc == 0) {
		rc = settings_save_one("badge/art-meta", manifest, sizeof(manifest));
		if (rc != 0) {
			artwork_write_fault =
				true; // A lost acknowledgement may still have committed.
		}
	}
	if (rc == 0) {
		artwork_store.committed(manifest);
		published_art_crc = sys_get_le32(manifest + 8);
		published_art_view = {data, static_cast<uint32_t>(length)};
	}
	return rc;
}
struct LoadContext {
	SettingsService &service;
	int read_error = 0;
	uint8_t manifest[12]{};
	bool has_manifest = false, has_v3 = false;
	uint8_t current_manifest[12]{};
};

int read_current(const char *key, size_t length, settings_read_cb read, void *argument,
		 void *context)
{
	auto &load = *static_cast<LoadContext *>(context);
	if (load.read_error) {
		return load.read_error;
	}
	if (key && key[0] != 0) {
		return 0;
	}
	load.has_manifest = true;
	if (length != 12) {
		load.read_error = -EIO;
		return -EIO;
	}
	const ssize_t rc = read(argument, load.current_manifest, length);
	if (rc == 12) {
		if (load.current_manifest[0] != 1 && load.current_manifest[0] != 3) {
			load.read_error = -EIO;
		}
		load.has_v3 = load.current_manifest[0] == 3;
		memcpy(load.manifest, load.current_manifest, 12);
	}
	if (rc != 12) {
		load.read_error = rc < 0 ? static_cast<int>(rc) : -EIO;
	}
	return load.read_error;
}
int read_setting(const char *key, size_t length, settings_read_cb read, void *argument,
		 void *context)
{
	auto &load = *static_cast<LoadContext *>(context);
	if (load.read_error != 0) {
		return load.read_error;
	}
	if (strcmp(key, "art-meta") == 0 || (load.has_v3 && strcmp(key, "artwork") == 0)) {
		return 0;
	}
	for (unsigned i = 0; i < static_cast<unsigned>(Field::kCount); ++i) {
		const Field field = static_cast<Field>(i);
		if (strcmp(key, field_name(field)) != 0) {
			continue;
		}
		/* Startup loader has one owner; avoid a large transient stack buffer. */
		static uint8_t scalar_bytes[kTextBytes];
		auto *bytes = field == Field::kArtwork ? asset_buffer : scalar_bytes;
		constexpr size_t maxima[] = {1,          kPayloadMax,         kNameMax,    1, 1,
					     kTextBytes, kLegacyArtworkBytes, kMotionBytes};
		static_assert(sizeof(maxima) / sizeof(maxima[0]) ==
			      static_cast<unsigned>(Field::kCount));
		if (length == 0 || length > maxima[i]) {
			LOG_WRN("invalid setting %s; using default", field_name(field));
			break;
		}
		const ssize_t rc = read(argument, bytes, length);
		if (rc != static_cast<ssize_t>(length)) {
			load.read_error = rc < 0 ? static_cast<int>(rc) : -EIO;
			LOG_ERR("setting %s read failed (%d)", field_name(field), load.read_error);
			return load.read_error;
		}
		if (!load.service.load(field, bytes, length)) {
			LOG_WRN("invalid setting %s; using default", field_name(field));
		}
		break;
	}
	return 0;
}
} // namespace

int load_settings(SettingsService &service, bool factory_reset)
{
	service.begin_load();

	if (factory_reset) {
		const struct flash_area *area;
		int rc = flash_area_open(PARTITION_ID(storage_partition), &area);
		if (rc != 0) {
			return rc;
		}
		rc = flash_area_flatten(area, 0, area->fa_size);
		flash_area_close(area);
		if (rc != 0) {
			return rc;
		}
		LOG_INF("factory reset: settings partition erased");
	}
	int rc = initialize_artwork();
	if (rc == 0) {
		rc = settings_subsys_init();
	}
	if (rc == 0) {
		LoadContext load{service};
		rc = settings_load_subtree_direct("badge/art-meta", read_current, &load);
		if (rc == 0) {
			rc = load.read_error;
		}
		if (rc == 0) {
			rc = settings_load_subtree_direct("badge", read_setting, &load);
		}
		/* The pinned SDK discards backend callback errors in the direct
		 * loader. Preserve the first read failure independently. */
		if (rc == 0) {
			rc = load.read_error;
		}
		if (rc == 0 && load.has_v3) {
			Artwork art;
			rc = artwork_store.load(load.current_manifest, art);
			if (rc == 0 && !service.load(Field::kArtwork, art.bytes, art.length)) {
				rc = -EIO;
			}
		}
		if (rc == 0 && !load.has_v3 && load.has_manifest) {
			const auto *m = load.manifest;
			const size_t length = sys_get_le32(m + 4);
			if (m[0] != 1 || m[1] > 1 || m[3] != 0 || length < 4 ||
			    length > kLegacyArtworkBytes ||
			    m[2] != (length + kChunkBytes - 1) / kChunkBytes) {
				rc = -EIO;
			} else {
				rc = load_chunks(m[1], length);
				if (rc == 0 &&
				    (artwork_crc(asset_buffer, length) != sys_get_le32(m + 8) ||
				     !service.load(Field::kArtwork, asset_buffer, length))) {
					rc = -EIO;
				}
			}
		}
	}
	if (rc != 0) {
		service.begin_load();
	}
	const uint8_t missing = service.finish_load();
	published_art_view = service.values().artwork;
	published_art_crc = artwork_crc(published_art_view.bytes, published_art_view.length);
	for (unsigned i = 0; i < static_cast<unsigned>(Field::kCount); ++i) {
		if ((missing & (1U << i)) != 0) {
			LOG_WRN("setting %s defaulted", field_name(static_cast<Field>(i)));
		}
	}
	return rc;
}

int stage_artwork_flash(uint32_t offset, uint32_t total, const uint8_t *bytes, size_t length,
			const uint8_t **view)
{
	if (artwork_write_fault) {
		return -EIO;
	}
	const int rc = artwork_store.stage(offset, total, bytes, length);
	if (rc == 0 && view) {
		*view = artwork_store.staged_data();
	}
	return rc;
}

uint32_t current_artwork_crc(const Artwork &view)
{
	if (view.bytes != published_art_view.bytes || view.length != published_art_view.length) {
		published_art_view = view;
		published_art_crc = artwork_crc(view.bytes, view.length);
	}
	return published_art_crc;
}

int store_field(void *, Field field, const void *data, size_t length)
{
	if (field == Field::kArtwork) {
		return store_artwork(static_cast<const uint8_t *>(data), length);
	}
	const char *name = field_name(field);
	if (name == nullptr) {
		return -EINVAL;
	}
	char key[32] = "badge/";
	strcat(key, name);
	return settings_save_one(key, data, length);
}
} // namespace badge
