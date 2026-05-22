def apply(config, args):
    config["mapfile"] = "build/mariogolf64.map"
    config["myimg"] = "build/mariogolf64.z64"
    config["baseimg"] = "baserom.z64"
    config["arch"] = "mips"
    config["make_command"] = ["make"]
    config["build_dir"] = "build"
