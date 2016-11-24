# Kernel Permissive Patcher for Android
 * <b>[See the project on XDA](http://forum.xda-developers.com/-/-/-t3506338)</b>

## How the project works and what it does
 * Appends the permissive command to the kernel
    installed on the device without other changes

 * Detects the bootimage partition, detection
    based on the same logic from Chainfire

 * Reads the kernel header and contents with
    libbootimg used for our MultiROM projects

 * Appends if necessary the new permissive element
    and write the kernel back to the partition

 * Also compatible with the Sony ELF boot partitions
    by using my MultiROM version of the libbootimg
