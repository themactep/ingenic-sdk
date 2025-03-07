#!/bin/bash
set -e  # Exit on error

# Configuration variables with defaults
DEFAULT_KERNEL_VERSION="3.10"
KDIR="${KDIR:-}"
CROSS_COMPILE="${CROSS_COMPILE:-}"

# Function to display usage information
show_usage() {
	echo "Usage: $0 <soc> [kernel_version] [make_args...]"
	echo "Usage: $0 clean"
	echo ""
	echo "Supported SoCs: t10, t20, t21, t23, t30, t31, t40, t41, a1"
	echo "Example: $0 t20 3.10"
	echo ""
	echo "Environment variables:"
	echo "  KDIR          - Path to kernel source (required)"
	echo "  CROSS_COMPILE - Cross compiler prefix (required)"
}

# Function to clean build artifacts
clean_build() {
	echo "Cleaning build artifacts..."
	find . -type f -name "*.o"     -delete
	find . -type f -name "*.o.*"   -delete
	find . -type f -name "*.ko"    -delete
	find . -type f -name "*.ko.*"  -delete
	find . -type f -name "*.mod.c" -delete
	find . -type f -name ".*.cmd"  -delete
	find . -type d -name ".tmp_versions" -exec rm -rf {} + 2>/dev/null || true
	rm -f Module.symvers modules.order
	echo "Clean complete"
}

# Parse command line arguments
if [ $# -lt 1 ]; then
	show_usage
	exit 1
fi

SOC_MODEL="$1"

# Handle 'clean' command
if [ "$SOC_MODEL" = "clean" ]; then
	clean_build
	exit 0
fi

# Check supported SOC models
SUPPORTED_SOCS="t10 t20 t21 t23 t30 t31 t40 t41 a1"
if ! echo "$SUPPORTED_SOCS" | grep -wq "$SOC_MODEL"; then
	echo "Error: Unsupported SoC model '$SOC_MODEL'"
	show_usage
	exit 1
fi

# Set kernel version from second argument or use default
KERNEL_VERSION="${2:-$DEFAULT_KERNEL_VERSION}"
if [ "$2" ]; then
	shift 2  # Remove first two arguments (SOC and kernel version)
else
	shift    # Remove only first argument (SOC)
fi

# Check if KDIR is set
if [ -z "$KDIR" ]; then
	echo "Error: KDIR is not set. Please export KDIR=/path/to/kernel"
	exit 1
fi

# Check if KDIR exists
if [ ! -d "$KDIR" ]; then
	echo "Error: Kernel directory '$KDIR' does not exist"
	exit 1
fi

# Check if CROSS_COMPILE is set and fail if it's not
if [ -z "$CROSS_COMPILE" ]; then
	echo "Error: CROSS_COMPILE is not set. Cross compiler is required."
	echo "Please export CROSS_COMPILE=/path/to/toolchain/prefix-"
	exit 1
fi

# Verify CROSS_COMPILE points to a valid compiler
if ! command -v "${CROSS_COMPILE}gcc" &> /dev/null; then
	echo "Error: Cross compiler '${CROSS_COMPILE}gcc' not found or not executable"
	exit 1
fi

# Convert SOC model to uppercase for config
SOC_UPPER=$(echo "$SOC_MODEL" | tr '[:lower:]' '[:upper:]')

# Build command
echo "Building for SoC: $SOC_MODEL, Kernel: $KERNEL_VERSION"
echo "Using kernel directory: $KDIR"
echo "Using cross compiler: ${CROSS_COMPILE}gcc"

# Set environment variables
export CROSS_COMPILE="${CROSS_COMPILE}"

# Build make command
make \
	V=1 \
	BR2_THINGINO_MOTORS=y \
	BR2_THINGINO_MOTORS_SPI=y \
	CROSS_COMPILE="$CROSS_COMPILE" \
	ARCH=mips \
	SOC_FAMILY="$SOC_MODEL" \
	CONFIG_SOC_${SOC_UPPER}=y \
	KERNEL_VERSION="$KERNEL_VERSION" \
	-C "$KDIR" \
	M="$PWD" \
	"$@"

exit 0
