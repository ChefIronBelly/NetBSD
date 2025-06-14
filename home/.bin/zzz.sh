#!/bin/sh

doas sysctl -w hw.acpi.sleep.vbios=2
doas sysctl -w hw.acpi.sleep.state=3
