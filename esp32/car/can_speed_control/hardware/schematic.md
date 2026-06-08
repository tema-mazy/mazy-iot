# Hardware Schematic — Suzuki Swift AZ Parking Sensor Speed Gate

## Components

| # | Part | Description |
|---|------|-------------|
| U1 | ESP32-C3 SuperMini | Main controller |
| U2 | SN65HVD230 | CAN bus transceiver, 3.3 V |
| U3 | DC-DC SSR module | e.g. Fotek SSR-10DA or any DC–DC opto-isolated relay, input 3–32 V, output up to 60 V DC / 10 A |
| C1 | 100 nF ceramic | Decoupling, SN65HVD230 VCC |
| C2 | 10 µF electrolytic | Bulk decoupling, 3.3 V rail |
| R1 | 120 Ω / 0.25 W | CAN bus termination (only if this node is a bus endpoint) |
| R2 | 330 Ω / 0.25 W | SSR input current limiter |
| PSU | LM2596 / MP1584 buck module | 12 V → 5 V, 1 A (power ESP32 via USB-5 V pin) |

---

## Power Supply

```
                                    F1 (1 A fuse)
Fuse box +12 V (ignition-switched) ─┤├────┬──────────────────────────── SSR load supply
                                         │
                                    LM2596 buck
                                    12 V → 5 V
                                         │
                                        +5 V ──── ESP32-C3 5V pin
                                         │
                             ┌───────────┤
                            C2           │
                          10 µF          │
                             └───────────┤
                                         │
Car GND ─────────────────────────────────┴──── ESP32-C3 GND
```

> Power comes from a fuse box ignition-switched tap, **not** OBD-II pin 16 (pin 16 is always-on battery — using it would drain the battery when the car is parked).

---

## CAN Bus Interface (U2 — SN65HVD230)

```
                          ┌─────────────────────┐
  ESP32-C3 GPI20 (TX) ───►│ TXD             CANH ├───── OBD-II pin 6  (CAN-H)
  ESP32-C3 GPI21 (RX) ◄───│ RXD             CANL ├───── OBD-II pin 14 (CAN-L)
                  3.3 V ──│ VCC               Rs  │──── GND  (Rs=GND → normal drive)
                   GND ───│ GND                   │
                          └─────────────────────┘
                                │           │
                               C1          R1
                             100 nF      120 Ω          ← R1 only if bus endpoint
                                │           │               (measure ~60 Ω CANH–CANL
                               GND        CANL               with ignition off to verify)
```

> Firmware runs in **TWAI_MODE_LISTEN_ONLY** — no ACK bits or frames are ever transmitted. The device is electrically invisible to the ECU. TXD is wired but kept recessive by the TWAI controller; the OBD-II fallback polling was removed for the same reason.

**OBD-II tap points:**

```
OBD-II socket (under dashboard)
 ┌──────────────────────────┐
 │  1   2   3   4   5   6  │
 │  7   8   9  10  11  12  │
 │     13  14  15  16      │
 └──────────────────────────┘
      │   │           │  │
      │  GND     CAN-H  +12V battery (not used for PSU)
    CAN-L   (pin4) (pin6)           (pin16)
   (pin14)
```

---

## SSR — Brake Signal Switching (U3)

```
Car +12 V brake wire
(from brake light switch)
          │
          │         ┌──────────────────────────────┐
          └─────────┤ OUT+                          │
                    │        SSR (opto-isolated)    │
 PARKING_BRAKE_IN ─────┤ OUT−        DC–DC             ├──── Parking sensor "brake in" wire
                    │                               │
  ESP32 GPIO0 ──R2──┤ IN+                           │
        330 Ω       │                               │
           GND ─────┤ IN−                           │
                    └──────────────────────────────┘
```

**Relay state table:**

| Speed     | GPIO0 | SSR output | Parking sensors |
|-----------|-------|------------|-----------------|
| < 8 km/h  | LOW   | CLOSED     | **ENABLED** — receives +12 V brake signal |
| 8–10 km/h | —     | unchanged  | Hysteresis — holds previous state |
| > 10 km/h | HIGH  | OPEN       | **DISABLED** — no brake signal |

> GPIO0 is active-low for the SSR input. If your SSR is active-high, invert `relay_set()` in firmware: `gpio_set_level(RELAY_GPIO, on ? 1 : 0)`.

---

## Status LED

Built-in LED on GPIO8 (ESP32-C3 SuperMini):

| LED | Relay | Parking sensors |
|-----|-------|-----------------|
| ON  | ON    | Enabled (low speed / stopped) |
| OFF | OFF   | Disabled (driving) |

---

## Full Wiring Diagram

```
  CAR SIDE                                              ESP32-C3 SuperMini
  ──────────────────────────────────────────────────────────────────────────────

  OBD-II pin 6  (CAN-H) ────────────────── SN65HVD230 ─── GPIO20 (TX)
  OBD-II pin 14 (CAN-L) ──────────────────  CANH/CANL  ─── GPIO21 (RX)
                                              3.3V/GND ─── 3.3V / GND

  OBD-II pin 4  (GND)   ────────────────────────────────── GND
  Fuse box IGN +12V ────── F1(1A) ─── LM2596 ─── +5V ─── 5V pin
  (OBD-II pin 16 = always-on battery — not used for power)

  Brake +12V wire ──────────────────────────────────────── SSR OUT+
                                                           SSR OUT− ─── Parking ctrl "brake in"
                                                           SSR IN+  ─── R2(330Ω) ─── GPIO0
                                                           SSR IN−  ─── GND

  ──────────────────────────────────────────────────────────────────────────────

                                                              GPIO8 ─── Built-in LED
```

---

## Important Notes

1. **Do NOT cut the brake wire** — splice in (T-tap or solder + heatshrink). The SSR must be in series only on the wire to the parking sensor controller, not the main brake light circuit.
2. **Fuse** — add a 1 A inline fuse (F1) on the +12 V feed to the buck converter.
3. **CAN termination** — the OBD-II port is typically mid-bus; do NOT add R1 unless termination is missing. Check: ~60 Ω between CANH/CANL with ignition off = correctly terminated; ~120 Ω = only one terminator present, add R1.
4. **CAN baud rate** — Suzuki Swift AZ uses 500 kbps on HS-CAN, set in firmware via `TWAI_TIMING_CONFIG_500KBITS()`.
5. **Speed broadcast** — Known Swift AZ: CAN ID `0x0AA`, bytes 1–2: `(b[1] << 8 | b[2]) / 100.0` km/h. Verify by sniffing before deployment. OBD-II polling is the automatic fallback if no broadcast is seen for 2 s.
