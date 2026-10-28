"""ESPHome codegen for the NASA bridge / ODU control (Samsung NASA bus).

Two roles, chosen by `relay_enabled`:
  * false (default use now): PARALLEL TAP + INJECT. The device taps the NASA bus
    alongside the indoor/MIM/ODU, listens for silence, and injects ONE whitelisted
    control write (0x428D silence / Hz-limit) when armed.
  * true: in-line cut-through relay (legacy experiment; broke the handshake).

A control frame leaves the device ONLY when `allow_write` is armed. Every write
funnels through the C++ inject_write_() gate; an ON request auto-reverts via a
fail-safe timer. set_quiet_mode() is called from a YAML lambda via the id.
"""

from pathlib import Path

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, uart, web_server_base
from esphome.const import CONF_ID
from esphome.yaml_util import load_yaml

CODEOWNERS = ["@jotuera"]
DEPENDENCIES = ["uart", "web_server_base"]
AUTO_LOAD = ["sensor", "web_server_base"]

nasa_bridge_ns = cg.esphome_ns.namespace("nasa_bridge")
NasaBridge = nasa_bridge_ns.class_("NasaBridge", cg.Component)

CONF_MIM_UART = "mim_uart"
CONF_ODU_UART = "odu_uart"
CONF_SOURCE_ADDRESS = "source_address"
CONF_DEST_ADDRESS = "dest_address"
CONF_INJECT_GAP = "inject_gap"
CONF_RELAY_ENABLED = "relay_enabled"
CONF_AUTO_POLL = "auto_poll"
CONF_DIAG_INTERVAL = "diag_interval"
CONF_MIM_DE_PIN = "mim_de_pin"
CONF_TURNAROUND_GAP = "turnaround_gap"
CONF_ALLOW_WRITE = "allow_write"
CONF_REVERT_AFTER = "revert_after"
CONF_PACKET_TYPE = "packet_type"
CONF_HZ_LIMIT = "hz_limit"
CONF_SILENCE_ACTIVE = "silence_active"
CONF_FSV_1012 = "fsv_1012"
CONF_SETPOINT = "setpoint"
CONF_FLOW_TEMP = "flow_temp"
CONF_COMP_FREQ = "comp_freq"
CONF_FR_CONTROL = "fr_control"
CONF_FSV_REGISTER = "fsv_register"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

# The FSV catalog (all 94 installer settings) ships next to this file; it is the
# single source of truth shared with the generated bridge_fsv_* packages.
_FSV_CATALOG = Path(__file__).parent / "bridge_fsv_catalog.yaml"

# NASA type byte = packetType(Normal=1)<<4 | dataType.
PACKET_TYPES = {"request": 0x13, "write": 0x12, "notify": 0x14}


def _three_byte_address(value):
    """A NASA address: exactly three bytes (class, channel, address)."""
    value = cv.ensure_list(cv.hex_uint8_t)(value)
    if len(value) != 3:
        raise cv.Invalid("A NASA address must be exactly 3 bytes, e.g. [0x80, 0xFF, 0x00]")
    return value


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NasaBridge),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(web_server_base.WebServerBase),
        # At least one UART (the bus tap). In TAP mode mim_uart is the tap.
        cv.Required(CONF_MIM_UART): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_ODU_UART): cv.use_id(uart.UARTComponent),
        cv.Optional(CONF_SOURCE_ADDRESS): _three_byte_address,
        cv.Optional(CONF_DEST_ADDRESS): _three_byte_address,
        cv.Optional(CONF_INJECT_GAP, default="40ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RELAY_ENABLED, default=True): cv.boolean,
        cv.Optional(CONF_AUTO_POLL, default=False): cv.boolean,
        cv.Optional(CONF_DIAG_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MIM_DE_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_TURNAROUND_GAP, default="2ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ALLOW_WRITE, default=False): cv.boolean,
        cv.Optional(CONF_REVERT_AFTER, default="120s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PACKET_TYPE, default="request"): cv.enum(PACKET_TYPES, lower=True),
        cv.Optional(CONF_HZ_LIMIT): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_SILENCE_ACTIVE): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_FSV_1012): sensor.sensor_schema(accuracy_decimals=1),
        cv.Optional(CONF_SETPOINT): sensor.sensor_schema(accuracy_decimals=1),
        cv.Optional(CONF_FLOW_TEMP): sensor.sensor_schema(accuracy_decimals=1),
        cv.Optional(CONF_COMP_FREQ): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_FR_CONTROL): sensor.sensor_schema(accuracy_decimals=0),
        cv.Optional(CONF_FSV_REGISTER, default=0x424E): cv.hex_uint16_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mim = await cg.get_variable(config[CONF_MIM_UART])
    cg.add(var.set_mim_uart(mim))
    if CONF_ODU_UART in config:
        odu = await cg.get_variable(config[CONF_ODU_UART])
        cg.add(var.set_odu_uart(odu))

    if CONF_SOURCE_ADDRESS in config:
        a = config[CONF_SOURCE_ADDRESS]
        cg.add(var.set_source_address(a[0], a[1], a[2]))
    if CONF_DEST_ADDRESS in config:
        a = config[CONF_DEST_ADDRESS]
        cg.add(var.set_dest_address(a[0], a[1], a[2]))

    cg.add(var.set_inject_gap_ms(config[CONF_INJECT_GAP]))
    cg.add(var.set_relay_enabled(config[CONF_RELAY_ENABLED]))
    cg.add(var.set_auto_poll(config[CONF_AUTO_POLL]))
    cg.add(var.set_diag_interval_ms(config[CONF_DIAG_INTERVAL]))
    cg.add(var.set_turnaround_gap_ms(config[CONF_TURNAROUND_GAP]))
    cg.add(var.set_allow_write(config[CONF_ALLOW_WRITE]))
    cg.add(var.set_revert_after_ms(config[CONF_REVERT_AFTER]))
    cg.add(var.set_type_byte(config[CONF_PACKET_TYPE]))
    if CONF_MIM_DE_PIN in config:
        de = await cg.gpio_pin_expression(config[CONF_MIM_DE_PIN])
        cg.add(var.set_mim_de_pin(de))
    if CONF_HZ_LIMIT in config:
        s = await sensor.new_sensor(config[CONF_HZ_LIMIT])
        cg.add(var.set_hz_limit_sensor(s))
    if CONF_SILENCE_ACTIVE in config:
        s = await sensor.new_sensor(config[CONF_SILENCE_ACTIVE])
        cg.add(var.set_silence_sensor(s))
    if CONF_FSV_1012 in config:
        s = await sensor.new_sensor(config[CONF_FSV_1012])
        cg.add(var.set_fsv_sensor(s))
    if CONF_SETPOINT in config:
        s = await sensor.new_sensor(config[CONF_SETPOINT])
        cg.add(var.set_setpoint_sensor(s))
    if CONF_FLOW_TEMP in config:
        s = await sensor.new_sensor(config[CONF_FLOW_TEMP])
        cg.add(var.set_flow_temp_sensor(s))
    if CONF_COMP_FREQ in config:
        s = await sensor.new_sensor(config[CONF_COMP_FREQ])
        cg.add(var.set_comp_freq_sensor(s))
    if CONF_FR_CONTROL in config:
        s = await sensor.new_sensor(config[CONF_FR_CONTROL])
        cg.add(var.set_fr_sensor(s))
    cg.add(var.set_fsv_register(config[CONF_FSV_REGISTER]))

    # Wire the HTTP dump handler (GET /fsv.csv) onto the existing web server.
    wsb = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    cg.add(var.set_web_server_base(wsb))

    # Load the FSV catalog and register every entry. dec -> (mul, is_signed):
    #   temp = signed x0.1, enum = unsigned x1, raw = unsigned x(mul or 1).
    catalog = load_yaml(_FSV_CATALOG)["fsv"]
    for number, entry in catalog.items():
        dec = entry["dec"]
        if dec == "temp":
            mul, is_signed = 0.1, True
        elif dec == "raw":
            mul, is_signed = float(entry.get("mul", 1)), False
        else:  # enum
            mul, is_signed = 1.0, False
        cg.add(
            var.add_fsv(
                int(str(number), 0),
                int(str(entry["pdu"]), 0),
                mul,
                is_signed,
                bool(entry["writable"]),
                str(entry["name_en"]),
                str(entry.get("unit", "")),
            )
        )
