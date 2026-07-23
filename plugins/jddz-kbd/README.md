---
title: Plugin: JDDZ Keyboard
---

## Introduction

The JDDZ keyboard interface is used by the second-generation StarLite Magnetic
Keyboard. The controller uses SiGma Micro's USB VID and a JDDZ-specific
bootloader.

## Firmware Format

The daemon will decompress the cabinet archive and extract a decoded 0x4000
byte firmware image. The vendor OTA container must be decoded before it is
packaged.

This plugin supports the following protocol ID:

* `com.jddz.kbd`

## GUID Generation

These devices use the standard USB DeviceInstanceId values, e.g.

* `USB\VID_1C4F&PID_007F`

## Update Behavior

The plugin disconnects the runtime device into a separate IAP mode to perform
the update. The device restarts into the new firmware after the final block is
written.

## Vendor ID Security

The vendor ID is set from the runtime USB vendor, in this instance set to
`USB:0x1C4F`.

## External Interface Access

This plugin requires read/write access to `/dev/bus/usb`.

## Version Considerations

This plugin has been available since fwupd version `2.1.7`.
