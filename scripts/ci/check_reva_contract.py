#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check compiled Rev A pins and flash against the schematic-derived contract.

Consumes only a trusted local build's edt.pickle, not downloaded pickle data.
An optional KiCad XML netlist checks the public pin contract against its source.
"""
import argparse
import json
import os
import pickle
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def check(edt, contract, partitions):
    pins = {}
    analog = {}
    for pad in contract["pads"]:
        m = re.match(r"P([01])\.(\d+)", pad["pin_function"])
        if m:
            pins[pad["net"]] = (int(m[1]), int(m[2]))
        m = re.search(r"AIN(\d+)", pad["pin_function"])
        if m:
            analog[pad["net"]] = int(m[1])

    def require(condition, message):
        if not condition:
            raise ValueError(message)

    def gpio(node, prop, net, active_low=False):
        value = node.props[prop].val[0]
        port = value.controller.props["port"].val
        require((port, value.data["pin"]) == pins[net], f"Wrong GPIO for {net}")
        require(bool(value.data["flags"] & 1) == active_low, f"Wrong polarity for {net}")

    user = edt.get_node("/zephyr,user")
    properties = {
        "mtx-lat": "MTX_LAT", "mtx-blank": "MTX_BLANK", "mtx-arm": "MTX_ARM",
        "mtx-safe": "MTX_SAFE_SENSE", "led-en": "LED_EN", "vbat-meas-en": "VBAT_MEAS_EN",
        "chg-en": "CHG_EN", "nfc-pwr": "NFC_PWR", "imu-int2": "IMU_INT2",
    }
    for name, net in properties.items():
        gpio(user, name + "-gpios", net)
    for name, net in {"nfc-fd": "NFC_FD", "chg-int": "CHG_INT", "pwr-sw": "PWR_SW_N"}.items():
        gpio(user, name + "-gpios", net, True)
    gpio(edt.get_node("/buttons/mode"), "gpios", "BTN_MODE", True)
    gpio(edt.get_node("/buttons/action"), "gpios", "BTN_ACTION", True)
    gpio(edt.get_node("/leds/status_led"), "gpios", "STATUS_LED")
    matrix = edt.chosen_nodes["zephyr,display"]
    for prop, net in {"lat": "MTX_LAT", "blank": "MTX_BLANK", "arm": "MTX_ARM",
                      "safe": "MTX_SAFE_SENSE", "enable": "LED_EN"}.items():
        gpio(matrix, prop + "-gpios", net)
    spi = matrix.props["spi"].val
    require(spi.regs[0].addr == 0x4002F000, "Matrix must own SPIM3")
    require(matrix.props["timer"].val.regs[0].addr == 0x4001A000, "Matrix must own TIMER3")
    nfc_bus = user.props["ntag-i2c-bus"].val
    imu = next(node for node in edt.nodes if "invensense,icm42688p-i2c" in node.compats)
    gpio(imu, "int-gpios", "IMU_INT1")
    require(imu.bus_node != nfc_bus, "NFC and IMU must be on distinct buses")
    require(imu.bus_node.regs[0].addr == 0x40003000, "IMU must use TWIM0")
    require(nfc_bus.regs[0].addr == 0x40004000, "NFC must use TWIM1")
    require(user.props["charger-i2c-bus"].val == imu.bus_node, "Charger shares TWIM0 with the IMU")

    # Compare Nordic PSEL function numbers plus physical port/pin. Constants
    # come from the pinned SDK binding, not from the board being checked.
    header = (Path(os.environ["ZEPHYR_BASE"]) / "include/zephyr/dt-bindings/pinctrl/nrf-pinctrl.h").read_text()
    signals = {
        "spi3": {"SPIM_SCK": "MTX_SCK", "SPIM_MOSI": "MTX_MOSI", "SPIM_MISO": "MTX_MISO"},
        "i2c0": {"TWIM_SCL": "I2C1_SCL", "TWIM_SDA": "I2C1_SDA"},
        "i2c1": {"TWIM_SCL": "NFC_SCL", "TWIM_SDA": "NFC_SDA"},
        "pdm0": {"PDM_CLK": "PDM_CLK", "PDM_DIN": "PDM_DIN"},
    }
    for name, functions in signals.items():
        expected = set()
        for function, net in functions.items():
            match = re.search(r"#define\s+NRF_FUN_" + function + r"\s+(\d+)", header)
            require(match is not None, f"SDK PSEL definition missing: {function}")
            port, pin = pins[net]
            expected.add(int(match[1]) << 24 | port << 5 | pin)
        for state in ["default", "sleep"]:
            group = edt.get_node(f"/pin-controller/{name}_{state}/group1")
            require(set(group.props["psels"].val) == expected, f"Wrong {name} {state} pinctrl")
    for channel, net in zip(user.props["io-channels"].val,
                            ["VBAT_SENSE", "CC1_SENSE", "CC2_SENSE", "VLED_SENSE"]):
        adc = edt.get_node(channel.controller.path + "/channel@" + str(channel.data["input"]))
        # The pinned Zephyr SAADC binding uses zero-based AIN indices.
        require(adc.props["zephyr,input-positive"].val == analog[net], f"Wrong ADC input: {net}")
    uicr = next(node for node in edt.nodes if "nordic,nrf-uicr" in node.compats)
    require(uicr.props["nfct-pins-as-gpios"].val, "IMU pins require NFCT GPIO mode")
    require(uicr.props["gpio-as-nreset"].val, "Module reset pin must stay reset")

    leaves = [p for p in partitions.values() if not p.get("span") and p.get("region") == "flash_primary"]
    leaves.sort(key=lambda p: p["address"])
    end = 0
    for partition in leaves:
        require(partition["address"] == end, "Flash has an unexpected gap/overlap")
        end += partition["size"]
    require(end == 1024 * 1024, "Flash layout must fit exactly1MiB")
    for key, address, size in [("mcuboot", 0, 0x10000), ("mcuboot_primary", 0x10000, 0x50000),
                               ("mcuboot_secondary", 0x60000, 0x50000),
                               ("artwork_storage", 0xB0000, 0x42000),
                               ("settings_storage", 0xF8000, 0x8000)]:
        actual = partitions[key]
        require(actual["address"] == address and actual["size"] == size, f"Wrong partition: {key}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--zephyr-base", type=Path, default=os.environ.get("ZEPHYR_BASE"))
    parser.add_argument("--netlist", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.zephyr_base is None:
        parser.error("Set ZEPHYR_BASE or pass --zephyr-base")
    os.environ["ZEPHYR_BASE"] = str(args.zephyr_base)
    sys.path.insert(0, str(args.zephyr_base / "scripts/dts/python-devicetree/src"))
    import yaml
    contract = json.loads((ROOT / "firmware/rev_a_nrf52840/pinmap.json").read_text())
    if args.netlist:
        actual = {}
        for net in ET.parse(args.netlist).getroot().findall("./nets/net"):
            for node in net.findall("node"):
                if node.get("ref") == "U401":
                    actual[int(node.get("pin"))] = (node.get("pinfunction"), net.get("name").split("/")[-1])
        expected = {p["module_pin"]: (p["pin_function"], p["net"]) for p in contract["pads"]}
        if actual != expected:
            raise ValueError("Schematic U401 differs from the pin contract")
    with (args.build_dir / "rev_a_nrf52840/zephyr/edt.pickle").open("rb") as stream:
        edt = pickle.load(stream)
    partitions = yaml.safe_load((args.build_dir / "partitions.yml").read_text())
    check(edt, contract, partitions)
    if args.self_test:
        value = edt.get_node("/zephyr,user").props["mtx-lat-gpios"].val[0].data
        original = value["pin"]
        value["pin"] ^= 1
        try:
            try:
                check(edt, contract, partitions)
            except ValueError as exc:
                if "MTX_LAT" not in str(exc):
                    raise
            else:
                raise AssertionError("Wrong GPIO was accepted")
        finally:
            value["pin"] = original
        damaged = json.loads(json.dumps(partitions))
        damaged["settings_storage"]["address"] -= 4096
        try:
            check(edt, contract, damaged)
        except ValueError:
            pass
        else:
            raise AssertionError("Overlapping flash was accepted")
        print("Contract self-test rejected wrong GPIO and overlapping flash.")
    print("Rev A compiled pin/bus/ADC/UICR and1MiB flash contract passed.")


if __name__ == "__main__":
    main()
