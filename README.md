## Mario Golf 64

Matching decompilation of Mario Golf (USA) for the Nintendo 64.

## Dependencies

The build process has the following package requirements:

* make
* git
* build-essential
* binutils-mips-linux-gnu
* gcc-mips-linux-gnu
* python3
* pip3

Under Debian / Ubuntu (which we recommend using), you can install them with the following commands:

```bash
sudo apt update
sudo apt install make git build-essential binutils-mips-linux-gnu gcc-mips-linux-gnu python3 python3-pip
```

## Building

To build the ROM, you must first install the necessary tools by running the following command:

```bash
make setup
```

Next, copy your ROM into the root directory of this repository and rename it to `baserom.z64`, then run the following commands:

```bash
make extract
make
```
