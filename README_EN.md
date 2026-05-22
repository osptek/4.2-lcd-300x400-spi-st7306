# 4.2" 300×400 tri-color reflective SPI module (ST7306) — documentation & samples

**简体中文：** [`README.md`](README.md)

---

> This repository currently provides an **ESP-IDF sample project**. Datasheets and specifications will be added to `docs/` when available.

## Product overview

| Item | Description |
|:--|:--|
| Module | 4.2-inch **tri-color reflective LCD**, **300×400** resolution |
| Interface | **SPI** |
| Driver IC | **ST7306** |
| Spec ID | **`4.2-lcd-300x400-spi-st7306`** is the common product designation in documentation |

---

## Repository layout

### Top-level

| Path | Contents |
|:--|:--|
| `docs/` | Datasheets and specifications (**to be added**) |
| `examples/` | **Sample projects** |

### `examples/` layout

| Location | Description |
|:--|:--|
| `examples/` root | ESP32-S3 + IDF5; ST7306 SPI multi-color demo with rotary encoder page switching |

### Sample project paths

| Description | Path |
|:--|:--|
| ST7306 SPI multi-color display (Color4 driver) | `examples/esp32s3-idf5_st7306-spi_color4/` |
