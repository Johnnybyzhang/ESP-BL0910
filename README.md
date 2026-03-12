# BL0910 ESPHome Component

This repository contains custom ESPHome components for the Shanghai Belling BL0910 10-phase AC energy metering chip. It allows you to integrate the BL0910 into Home Assistant via ESPHome, providing detailed multi-circuit or 3-phase electrical monitoring.

## Components

This repository provides two ESPHome components:
1. `bl0910`: The base component for a single BL0910 chip, with mode-aware mapping for `1U10I`, `5U5I`, and `3U6I` operation.
2. `bl0910_3phase`: An advanced coordinator component that links three BL0910 chips to provide comprehensive 3-phase electrical analysis (phase angles, line-to-line voltages, voltage unbalance, and total system power).

## Installation

You can use these components in your ESPHome project by including this repository as an `external_components` source in your YAML configuration.

```yaml
external_components:
  - source: github://<YOUR_GITHUB_USERNAME>/ESP-BL0910@master
    components: [bl0910, bl0910_3phase]
```
*(Alternatively, you can download the `components` folder and use `source: type: local`)*

## The `bl0910` Component (Single Chip / Multi-Circuit)

The `bl0910` component communicates with the chip via SPI. It exposes the global measurements (primary voltage, line frequency, chip temperature, total power, total energy) as well as per-channel measurements (mapped voltage, current, active power, energy, power factor) for up to 10 BL0910 output slots.

### Configuration Variables

- **id** (*Optional*, ID): The ID of the BL0910 component.
- **spi_id** (**Required**, ID): The ID of the SPI bus to use.
- **cs_pin** (**Required**, Pin): The Chip Select pin for the SPI bus.
- **reset_pin** (*Optional*, Pin): The GPIO pin connected to the chip's NRST (hardware reset).
- **irq_pin** (*Optional*, Pin): The GPIO pin connected to the chip's IRQ output.
- **mode** (*Optional*, string): The measurement mode. One of `1U10I` (default), `5U5I`, `3U6I`.
- **line_frequency** (*Optional*, string): The AC line frequency. One of `50HZ` (default), `60HZ`.
- **voltage** (*Optional*, map): Voltage front-end definition used to derive the initial voltage scale from hardware.
  - **load_res** (*Optional*, float): High-side divider resistor in ohms. Default `2000000`.
  - **sample_res** (*Optional*, float): Low-side divider resistor in ohms. Default `510`.
  - **sample_ratio** (*Optional*, float): Additional external ratio multiplier (PT ratio, etc). Default `1.0`.
  - **pga_gain** (*Optional*, int): BL0910 PGA gain. One of `1`, `2`, `8`, `16`. Default `1`.
- **current** (*Optional*, map): Current front-end definition used to derive the initial current scale from hardware.
  - **sample_res** (*Optional*, float): Shunt or burden resistor in ohms. Default `0.001`.
  - **sample_ratio** (*Optional*, float): Current ratio multiplier. Use `1.0` for a shunt, or the CT primary:secondary ratio for a CT. Default `1.0`.
  - **pga_gain** (*Optional*, int): BL0910 PGA gain. One of `1`, `2`, `8`, `16`. Default `1`.
- **cfdiv** (*Optional*, int): BL0910 `CFDIV` register value. Default `0x010`.
- **voltage_reference** (*Optional*, float): Extra RMS trim written into the chip's voltage `RMSGN` register(s). Default `1.0`.
- **current_reference** (*Optional*, float): Extra RMS trim written into the chip's current `RMSGN` register(s). Default `1.0`.
- **power_reference** (*Optional*, float): Extra active-power trim written into the chip's `WATTGN` register(s). Default `1.0`.
- **energy_reference** (*Optional*, float): Host-side energy conversion trim for CF counts. Default `1.0`.
- **update_interval** (*Optional*, Time): The interval to update the sensors. Defaults to `5s`.

### Example Configuration

```yaml
spi:
  - id: bl0910_spi
    clk_pin: GPIO36
    mosi_pin: GPIO35
    miso_pin: GPIO37
    interface: spi3

bl0910:
  - id: bl0910_chip
    spi_id: bl0910_spi
    cs_pin: GPIO34
    reset_pin: GPIO33
    mode: 1U10I
    line_frequency: 50Hz
    voltage:
      load_res: 2000000
      sample_res: 510
      sample_ratio: 1.0
      pga_gain: 1
    current:
      sample_res: 0.001
      sample_ratio: 1.0
      pga_gain: 1
    cfdiv: 0x010
    voltage_reference: 1.0
    current_reference: 1.0
    power_reference: 1.0
    energy_reference: 1.0
    update_interval: 5s

sensor:
  - platform: bl0910
    bl0910_id: bl0910_chip
    voltage:
      name: "Voltage"
    frequency:
      name: "Line Frequency"
    temperature:
      name: "Chip Temperature"
    total_power:
      name: "Total Active Power"
    total_energy:
      name: "Total Energy"
    
    channel_1:
      voltage:
        name: "Channel 1 Voltage"
      current:
        name: "Channel 1 Current"
      power:
        name: "Channel 1 Power"
      energy:
        name: "Channel 1 Energy"
```

## The `bl0910_3phase` Component (3-Phase Coordinator)

The `bl0910_3phase` component calculates 3-phase specific metrics by aggregating data from three separate `bl0910` components (one for each phase). It requires hardware zero-crossing interrupts (IRQ1 or IRQ2) from each chip to accurately compute phase angles.

### Configuration Variables

- **id** (*Optional*, ID): The ID of the 3-phase component.
- **phase_a** (**Required**, ID): The ID of the `bl0910` component for Phase A.
- **phase_b** (**Required**, ID): The ID of the `bl0910` component for Phase B.
- **phase_c** (**Required**, ID): The ID of the `bl0910` component for Phase C.
- **reset_pin** (*Optional*, Pin or list of Pins): The hardware reset pin(s). Can be a single pin (if shared across all 3 chips) or a list of exactly 3 pins (one per phase).
- **irq1_a_pin** / **irq1_b_pin** / **irq1_c_pin** (*Optional*, Pin): Internal GPIO pins connected to the IRQ1 outputs of the chips. Used for voltage zero-crossing detection.
- **irq2_a_pin** / **irq2_b_pin** / **irq2_c_pin** (*Optional*, Pin): Alternative internal GPIO pins connected to the IRQ2 outputs.
- **line_frequency** (*Optional*, string): The AC line frequency. `50HZ` (default) or `60HZ`.
- **update_interval** (*Optional*, Time): The interval to compute and publish sensors. Defaults to `5s`.

### Example Configuration

```yaml
# Define the three individual BL0910 chips
bl0910:
  - id: phase_a_chip
    spi_id: bl0910_spi
    cs_pin: GPIO34
    # ... basic settings ...
  - id: phase_b_chip
    spi_id: bl0910_spi
    cs_pin: GPIO40
    # ... basic settings ...
  - id: phase_c_chip
    spi_id: bl0910_spi
    cs_pin: GPIO41
    # ... basic settings ...

# Define the 3-phase coordinator
bl0910_3phase:
  id: three_phase_meter
  phase_a: phase_a_chip
  phase_b: phase_b_chip
  phase_c: phase_c_chip
  line_frequency: 50Hz
  reset_pin: GPIO33 # Shared NRST pin
  
  # Connect IRQ1 pins for zero-crossing phase angle calculations
  irq1_a_pin: GPIO38
  irq1_b_pin: GPIO39
  irq1_c_pin: GPIO42

sensor:
  - platform: bl0910_3phase
    bl0910_3phase_id: three_phase_meter
    total_active_power:
      name: "3-Phase Total Active Power"
    total_reactive_power:
      name: "3-Phase Total Reactive Power"
    total_apparent_power:
      name: "3-Phase Total Apparent Power"
    system_power_factor:
      name: "3-Phase Power Factor"
    phase_angle_ab:
      name: "Phase Angle A-B"
    phase_angle_bc:
      name: "Phase Angle B-C"
    phase_angle_ca:
      name: "Phase Angle C-A"
    line_voltage_ab:
      name: "Line Voltage A-B"
    line_voltage_bc:
      name: "Line Voltage B-C"
    line_voltage_ca:
      name: "Line Voltage C-A"
    voltage_unbalance:
      name: "Voltage Unbalance"

text_sensor:
  - platform: bl0910_3phase
    bl0910_3phase_id: three_phase_meter
    phase_sequence:
      name: "Phase Sequence"
```

## Calibration Guide

The component now starts from the hardware values in your analog front end instead of arbitrary `1.0` software scales.

### Hardware-Derived Baseline

- The BL0910 datasheet gives a differential analog full-scale input of `700 mV peak`, so the component derives an RMS full-scale of `0.7 / sqrt(2)` at the chip input.
- The datasheet's `1.8 V` rail is `DVDD18`; the metrology reference is the datasheet `VREF` value of about `1.097 V`.
- Current scaling is derived from `current.sample_res`, `current.sample_ratio`, and `current.pga_gain`.
- Voltage scaling is derived from `voltage.load_res`, `voltage.sample_res`, `voltage.sample_ratio`, and `voltage.pga_gain`.
- On every boot or reinitialization, the component rewrites `GAIN1`, `GAIN2`, `RMSGN`, `RMSOS`, `WATTGN`, `WATTOS`, and `CFDIV` so the chip always starts from the same calibration state.

### Trim Flow

1. Set the hardware front-end values to match your board.
2. Leave `voltage_reference`, `current_reference`, and `power_reference` at `1.0` for the first flash.
3. Measure a known load and compare the reported values to a trusted meter.
4. Use the ratio between real and measured values as trim:
   - `voltage_reference = V_real / V_meas`
   - `current_reference = I_real / I_meas`
   - `power_reference = P_real / P_meas`
5. If needed, trim CF-based energy separately with `energy_reference`.

### Mode Notes

- `1U10I`: `WATT[1..10]` are valid and all channels share voltage input 11.
- `5U5I`: only `WATT[1..5]` are treated as valid; per-channel `voltage` sensors expose the mapped RMS voltage for each output slot.
- `3U6I`: only `WATT[2]`, `WATT[3]`, `WATT[4]`, `WATT[7]`, `WATT[8]`, and `WATT[9]` are treated as valid.

## Actions & Automations

The component provides a custom action to trigger a hardware reset of the BL0910 chip(s) from ESPHome automations or Home Assistant buttons.

### `bl0910.reset` Action

This action pulls the configured `reset_pin` low to physically reset the chip, and then automatically reinitializes the SPI communications and register configurations.

```yaml
button:
  - platform: template
    name: "Reset BL0910"
    on_press:
      - bl0910.reset:
          id: bl0910_chip
```
