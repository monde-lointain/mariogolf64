#!/usr/bin/env python3

import argparse


def apply(config: dict, args: argparse.Namespace):
    config["baseimg"] = "baserom.z64"
    config["myimg"] = "build/mariogolf64.z64"
    config["mapfile"] = "build/mariogolf64.map"
    config["source_directories"] = ["./src", "./include", "./asm"]
    config["objdump_flags"] = ["-Mreg-names=32"]
    config["makeflags"] = ["NON_MATCHING=0"]
