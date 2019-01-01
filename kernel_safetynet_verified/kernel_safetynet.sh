#!/sbin/sh

# find_boot_image logic by Chainfire
for PARTITION in kern-a KERN-A android_boot ANDROID_BOOT Kernel kernel KERNEL boot BOOT lnx LNX; do
  BOOTIMAGE=$(readlink /dev/block/by-name/$PARTITION || readlink /dev/block/platform/*/by-name/$PARTITION || readlink /dev/block/platform/*/*/by-name/$PARTITION);
  if [ ! -z "$BOOTIMAGE" ]; then
    break;
  fi;
done;

# Bootimage not found
if [ -z "$BOOTIMAGE" ]; then
  return -1;
fi;

# Inject the SafetyNet verified cmdline
chmod 755 /tmp/kernel_cmdline;
/tmp/kernel_cmdline --inject="$BOOTIMAGE" --cmd_append="androidboot.verifiedbootstate=green";
return $?;

