# sen::kernel

The Sen Kernel library provides Sen's runtime environment, known as the kernel. This kernel is
responsible for instantiating components and coordinating the communication between them.

The most common method for instantiating a kernel is through the use of a YAML configuration file.
The kernel library is equipped with a bootloader that is tasked with parsing this YAML file. The
bootloader then uses the parsed configuration to create the appropriate kernel instance, ensuring
that all specified components and settings are correctly initialized.

## API Reference

Check out the [API Reference](../doxygen_gen/html/index.html) for a detailed description of the Sen
Kernel library.
