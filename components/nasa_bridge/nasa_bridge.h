#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"  // GPIOPin
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/web_server_base/web_server_base.h"  // /fsv.csv dump handler

namespace esphome {
namespace nasa_bridge {

// One FSV installer setting from the catalog (loaded from bridge_fsv_catalog.yaml
// by __init__.py). The scale and sign are pre-computed: value = raw * mul, read as
// int16 when is_signed (temperatures). writable gates generic fsv_write().
struct FsvEntry {
  uint16_t number;    // human FSV number, e.g. 1031
  uint16_t code;      // NASA message id, e.g. 0x424E
  float mul;          // 0.1 for temp, the catalog mul for raw, 1 for enum
  bool is_signed;     // true only for temperatures
  bool writable;      // false for the derating/forced-off/FR/PV/Smart-Grid group
  std::string name;   // English name (used in the /fsv.csv dump)
  std::string unit;   // unit string (may be empty)
};

// One queued NASA operation: message number + encoded value, the destination
// address, and the NASA type byte (0x12 WRITE / 0x13 REQUEST / 0x11 READ /
// 0x14 NOTIFY). value_len is 1 (enum) / 2 (variable) / 4 (long).
struct NasaWrite {
  uint16_t message_number;
  uint8_t value[4];
  uint8_t value_len;
  uint8_t dst[3];
  uint8_t type_byte;
};

// In-line NASA bridge (man-in-the-middle) sitting between the Immergas MIM and
// the Samsung outdoor unit (ODU).
//
// Design goals, in priority order:
//   1. FIDELITY  - every byte of Immergas <-> ODU traffic is forwarded verbatim.
//                  The bridge never rewrites, reorders or drops a byte that is
//                  part of the live Immergas control loop.
//   2. ADDITIVE  - the only thing the bridge ever puts on the wire of its own
//                  accord is a brand-new NASA write frame addressed to the ODU,
//                  injected in an idle gap. It never edits an in-flight frame.
//   3. WHITELIST - a frame may only be injected for the handful of Samsung-only
//                  functions below (quiet mode, outdoor heating curve, outdoor
//                  max heating temperature). Anything else is rejected.
class NasaBridge : public Component, public AsyncWebHandler {
 public:
  // Wiring: one UART toward the Immergas MIM, one toward the Samsung ODU.
  void set_mim_uart(uart::UARTComponent *u) { this->mim_uart_ = u; }
  void set_odu_uart(uart::UARTComponent *u) { this->odu_uart_ = u; }

  // Identity used as the *source* address of injected frames (we masquerade as
  // the controller/indoor side) and the *destination* address (the ODU).
  void set_source_address(uint8_t a, uint8_t b, uint8_t c) {
    this->src_[0] = a; this->src_[1] = b; this->src_[2] = c;
  }
  void set_dest_address(uint8_t a, uint8_t b, uint8_t c) {
    this->dst_[0] = a; this->dst_[1] = b; this->dst_[2] = c;
  }

  // Minimum idle time (ms) on BOTH directions before we are allowed to inject.
  void set_inject_gap_ms(uint32_t ms) { this->inject_gap_ms_ = ms; }

  // Diagnostics. When relay is disabled the bridge only LISTENS on both sides
  // and counts/logs what it sees (no forwarding, so no echo) - use it to prove
  // which side actually receives bus traffic. diag_interval 0 disables the
  // periodic status log.
  void set_relay_enabled(bool e) { this->relay_enabled_ = e; }
  void set_diag_interval_ms(uint32_t ms) { this->diag_interval_ms_ = ms; }
  // Continuous background FSV poll: when idle, read the next catalog FSV
  // (round-robin), so values refresh on their own and the "Read all FSV" button
  // becomes optional. Self-paced (one read per bus-silence window, no overflow).
  void set_auto_poll(bool e) { this->auto_poll_ = e; }
  bool is_auto_poll() const { return this->auto_poll_; }

  // Silence (ms) on the active side that marks the end of its burst, after which
  // we drain the echo and release the direction latch. Must be larger than the
  // inter-byte gap inside a frame and smaller than the far unit's turnaround.
  void set_turnaround_gap_ms(uint32_t ms) { this->turnaround_gap_ms_ = ms; }

  // Optional DE/TX-enable pin for the MIM transceiver (e.g. the Atomic RS485
  // Base). When set, it is driven high around a MIM-side write and low again,
  // so bytes we relay/inject toward the bus actually reach it. Auto-direction
  // transceivers (the Grove Isolated Unit) need no pin.
  void set_mim_de_pin(GPIOPin *pin) { this->mim_de_pin_ = pin; }

  // MASTER WRITE GATE. Default false: nothing is ever transmitted. The user
  // must explicitly arm this before any control frame can leave the device.
  void set_allow_write(bool a) { this->allow_write_ = a; }
  // Fail-safe: a silence-ON request auto-reverts to OFF after this many ms,
  // even if HA disconnects, so a Hz limit can never be left latched.
  void set_revert_after_ms(uint32_t ms) { this->revert_after_ms_ = ms; }
  // NASA type byte for injected frames (packetType<<4 | dataType):
  // 0x12 WRITE, 0x13 REQUEST, 0x14 NOTIFY. Default REQUEST per the test plan.
  void set_type_byte(uint8_t t) { this->type_byte_ = t; }
  // The single FSV register the write/read test targets (the one whitelisted
  // value that may be written). Stepped through the vetted-safe candidates.
  void set_fsv_register(uint16_t m) { this->fsv_register_ = m; }

  // Readback sensors: the component parses the bus and publishes the ODU's own
  // response to a silence request - 0x8239 (Hz limit, 64 -> 19) and 0x80B1
  // (silence active 0/1) - so the device self-verifies its write.
  void set_hz_limit_sensor(sensor::Sensor *s) { this->hz_limit_sensor_ = s; }
  void set_silence_sensor(sensor::Sensor *s) { this->silence_sensor_ = s; }
  // Readback of the configured test FSV (published as its real value, degC).
  void set_fsv_sensor(sensor::Sensor *s) { this->fsv_sensor_ = s; }
  // Readback of the Immergas active water setpoint 0x4247 (degC) - to see whether
  // lowering the FSV max actually clamps it.
  void set_setpoint_sensor(sensor::Sensor *s) { this->setpoint_sensor_ = s; }
  // Readback of the actual supply/flow water temp 0x4238 (degC) - the real clamp
  // indicator: a "water out MAX" FSV caps the flow temp, not the setpoint.
  void set_flow_temp_sensor(sensor::Sensor *s) { this->flow_temp_sensor_ = s; }
  // Actual compressor frequency 0x8238 (Hz) - the derating indicator: if FR
  // control works, this drops below what Immergas demands.
  void set_comp_freq_sensor(sensor::Sensor *s) { this->comp_freq_sensor_ = s; }
  // Readback of FR control 0x42F1: publishes the frequency-ratio % (enable logged).
  void set_fr_sensor(sensor::Sensor *s) { this->fr_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  // Run setup() AFTER WiFi: we register the /fsv.csv web handler in setup(), and
  // web_server_base->init() must not run before the network is up (prometheus does
  // the same). The bus tap itself is driven from loop(), so a late setup is fine.
  float get_setup_priority() const override { return setup_priority::WIFI - 1.0f; }

  // -------- FSV write-capability test (called from YAML / HA) --------
  // write_fsv: queue a WRITE of the test FSV (0x424B, cooling min-temp, inert)
  //   to the indoor unit (20.00.01). Leaves the device only if allow_write armed.
  // read_fsv: queue a benign READ request (no arming needed) so the indoor
  //   responds with the current FSV value -> published to the fsv sensor.
  bool write_fsv(float celsius);
  bool read_fsv();
  // Benign READ of any register (not whitelist-gated) for diagnostics - e.g. the
  // derating FSV 5041/5051/5042; the raw value is logged.
  bool read_reg(int msg);

  // -------- FR control (compressor frequency ratio / FSV 5051) --------
  // The MIM Modbus manual packs this into ONE register (0x42F1: Byte1=enable,
  // Byte0=ratio), but on the RAW NASA bus (what this bridge speaks) it is TWO
  // separate message numbers - confirmed by the F1/F2 reference code and live
  // captures:
  //   0x40A7  FSV 5051 FR control ON/OFF   - ENUM, 1 byte (0 = off, 1 = on)
  //   0x42F1  VAR_OUT_COMP_FREQ_RATE_CONTROL (the ratio value) - VAR, 2 bytes,
  //           range 0-150 (%). 100 = normal, <100 = derate, >100 = boost.
  // So fr_set_enable writes 0x40A7 and fr_set_ratio writes 0x42F1 - two
  // independent WRITEs, both allow_write-gated and whitelisted. We HARD-CAP the
  // ratio at 100 % so this lever can only DERATE, never boost. On installs where
  // the ODU does not expose FR control the write is simply ignored (benign).
  // Read them back with read_reg(0x40A7) / read_reg(0x42F1).
  void fr_set_enable(bool on);
  void fr_set_ratio(float ratio_pct);

  // -------- FSV catalog (all 94 installer settings) --------
  // add_fsv: register one catalog entry (called from __init__.py codegen).
  void add_fsv(uint16_t number, uint16_t code, float mul, bool is_signed, bool writable,
               const std::string &name, const std::string &unit);
  // fsv_value: the last-read, scaled value of a catalog FSV (NAN if never read or a
  // NaN sentinel). Called from the per-FSV template-sensor lambdas.
  float fsv_value(uint16_t code);
  // read_all: queue a benign READ of every catalog FSV (the "Read all FSV" button).
  void read_all();
  // fsv_write: generic, scale-aware WRITE of a catalog FSV. Gated by allow_write and
  // the writable flag; refused for read-only entries. Called from the number lambdas.
  bool fsv_write(uint16_t code, float value);

  // -------- operational modes that are NOT FSVs (e.g. outing/away) --------
  // read_value: last-read raw value of ANY code (incl. non-catalog modes), for the
  // mode switches' lambdas. outing_set: write ENUM_IN_OUTING_MODE 0x406D (0/1), the
  // outing/away-mode activation (the 5011-5019 FSVs only set its target temperatures).
  float read_value(uint16_t code);
  void outing_set(bool on);
  void silence_set(bool on);     // write ENUM_IN_SILENCE 0x4046 (0/1) - Samsung native quiet mode

  // The HTTP server that serves GET /fsv.csv (set from __init__.py).
  void set_web_server_base(web_server_base::WebServerBase *b) { this->web_server_base_ = b; }
  // AsyncWebHandler: serve a CSV dump of all catalog FSVs at /fsv.csv.
  bool canHandle(AsyncWebServerRequest *request) const override;
  void handleRequest(AsyncWebServerRequest *request) override;

 protected:
  // Cut-through, direction-latched forwarding: forward the active direction's
  // bytes as they arrive (low latency). While forwarding A->B we never read B
  // (its RX carries only our echo); when A falls silent for turnaround_gap_ms_
  // we drain B's echo and release the latch. Returns bytes moved this call.
  size_t forward_(uart::UARTComponent *from, uart::UARTComponent *to, bool from_is_mim);
  // Discard up to max_bytes waiting on a UART - exactly our own transmitted
  // echo for the burst just sent, leaving any real reply that follows it.
  void drain_(uart::UARTComponent *uart, uint32_t max_bytes);
  // Listen-only byte counting (relay disabled), for the diagnostic rounds.
  void sniff_(uart::UARTComponent *uart, bool is_mim);
  // TAP mode: read the bus, reassemble frames and parse out the readback
  // message-sets (0x8239 / 0x80B1), updating activity for silence detection.
  void tap_read_(uart::UARTComponent *uart);
  void parse_readback_(const uint8_t *f, size_t len);
  // Value width (bytes) encoded in bits 9-10 of a NASA message number.
  static uint8_t nasa_value_len_(uint16_t message_number);

  // True when it is safe to put an injected frame on the ODU wire right now.
  bool can_inject_now_(uint32_t now) const;

  // Periodic diagnostic line: per-side RX/TX byte counts and a hex preview of
  // the most recent bytes seen on each side.
  void log_diagnostics_(uint32_t now);

  // Build a full NASA frame for `w` (src_ -> dst_, data type = write) and send
  // it to the ODU. Returns false if the whitelist check fails.
  bool inject_write_(const NasaWrite &w);

  // Reject anything whose message number is not the configured FSV register.
  bool is_whitelisted_(uint16_t message_number);

  // CRC over frame[1 .. len-1] (everything between the start byte and the CRC).
  // NOTE: polynomial/seed flagged TO-VERIFY in the .cpp.
  static uint16_t nasa_crc_(const uint8_t *data, size_t len);

  uart::UARTComponent *mim_uart_{nullptr};
  uart::UARTComponent *odu_uart_{nullptr};

  // Our own NASA address (80.FF.00) and the ODU (10.00.00). Corrected from the
  // F1/F2 captures: masquerading as 20.00.01 would collide with the real MIM.
  uint8_t src_[3]{0x80, 0xFF, 0x00};
  uint8_t dst_[3]{0x10, 0x00, 0x00};

  // ---- write/control gating ----
  bool allow_write_{false};            // master gate; nothing transmits until armed
  uint32_t revert_after_ms_{120000};   // fail-safe auto-revert window
  uint32_t revert_due_ms_{0};          // 0 = no revert pending
  uint8_t type_byte_{0x13};            // REQUEST by default (0x12 WRITE / 0x14 NOTIFY)

  // Readback.
  sensor::Sensor *hz_limit_sensor_{nullptr};   // 0x8239
  sensor::Sensor *silence_sensor_{nullptr};    // 0x80B1
  sensor::Sensor *fsv_sensor_{nullptr};        // configured FSV readback
  sensor::Sensor *setpoint_sensor_{nullptr};   // 0x4247 active water setpoint
  sensor::Sensor *flow_temp_sensor_{nullptr};  // 0x4238 actual supply/flow temp
  sensor::Sensor *comp_freq_sensor_{nullptr};  // 0x8238 actual compressor freq (Hz)
  sensor::Sensor *fr_sensor_{nullptr};         // 0x42F1 frequency ratio % readback
  uint16_t fsv_register_{0x424E};              // FSV 1031 water-out MAX heating
  std::vector<uint8_t> parse_buf_;             // frame reassembly for readback

  // ---- FSV catalog + last-read raw values ----
  std::vector<FsvEntry> fsv_catalog_;          // all 94 FSVs (from bridge_fsv_catalog.yaml)
  std::map<uint16_t, size_t> fsv_index_;       // code -> index into fsv_catalog_
  std::map<uint16_t, int32_t> fsv_raw_;        // code -> last raw value seen on the bus
  web_server_base::WebServerBase *web_server_base_{nullptr};  // serves /fsv.csv

  uint32_t inject_gap_ms_{40};
  uint32_t last_activity_ms_{0};

  uint8_t packet_number_{0};  // incremented per injected frame

  std::vector<NasaWrite> inject_queue_;

  GPIOPin *mim_de_pin_{nullptr};

  // Direction latch for cut-through forwarding.
  enum class Dir : uint8_t { IDLE, MIM_TO_ODU, ODU_TO_MIM };
  Dir dir_{Dir::IDLE};
  uint32_t last_fwd_ms_{0};
  uint32_t turnaround_gap_ms_{2};
  uint32_t burst_tx_{0};  // bytes we drove onto the far side during this latch
  bool mim_capturing_{false}, odu_capturing_{false};

  // ---- background FSV poll ----
  bool auto_poll_{false};        // continuously read the catalog in the background
  size_t poll_cursor_{0};        // round-robin position into fsv_catalog_
  size_t extra_cursor_{0};       // round-robin position into extra_poll_
  bool poll_extra_turn_{false};  // alternate catalog/extra so the live codes refresh fast
  // Non-catalog codes also polled. These refresh twice as often as the static catalog (alternated in loop()),
  // so the live panel/status entities stay snappy. Groups:
  //  - modes:  0x406D outing, 0x42F1 FR ratio
  //  - errors: 0x0202 VAR_AD_ERROR_CODE1 (Hydro, E9xx/E6xx on the wired remote), 0x8235 VAR_OUT_ERROR_CODE (ODU)
  //  - panel (MWR-WW10N live readout): 0x8204 outdoor air, 0x4236 water-in, 0x4238 water-out, 0x4237 DHW tank,
  //    0x4247 water-out target, 0x4205 EVA-in, 0x4206 EVA-out, 0x420B discharge, 0x8238 compressor Hz,
  //    0x4089 water-pump state, 0x4001 operation mode.
  std::vector<uint16_t> extra_poll_{0x406D, 0x42F1, 0x0202, 0x8235,
                                    0x8204, 0x4236, 0x4238, 0x4237, 0x4247, 0x4205, 0x4206, 0x420B,
                                    0x8238, 0x4089, 0x4001,
                                    0x4046, 0x8239, 0x80B1,  // silence mode + Hz limit + under-limit flag
                                    0x40A7,  // FR control enable (working lever, moved to Controls)
                                    0x80AF};  // ENUM_OUT_LOAD_BASEHEATER - ODU drain-pan/base heater status (read-only)

  // ---- diagnostics ----
  bool relay_enabled_{true};
  uint32_t diag_interval_ms_{5000};
  uint32_t last_diag_ms_{0};
  uint32_t mim_rx_bytes_{0}, mim_tx_bytes_{0};
  uint32_t odu_rx_bytes_{0}, odu_tx_bytes_{0};
  std::vector<uint8_t> mim_preview_, odu_preview_;  // last bytes seen, for hex dump
  // NASA frame-edge counters: how many 0x32 (start) / 0x34 (end) bytes seen.
  // If these climb, real framed traffic IS arriving (idle gaps look like 00s).
  uint32_t mim_starts_{0}, mim_ends_{0}, odu_starts_{0}, odu_ends_{0};
  // First captured frame per side (0x32 .. 0x34, capped), logged once.
  std::vector<uint8_t> mim_frame_, odu_frame_;
  bool mim_frame_done_{false}, odu_frame_done_{false};
  bool mim_frame_logged_{false}, odu_frame_logged_{false};
};

}  // namespace nasa_bridge
}  // namespace esphome
