Ingenic TCU channel ownership: PWM vs Motor

Overview
- This repository now provides a central arbitration module (tcu_alloc) and runtime-configurable selection of TCU channels for PWM and Motor drivers.
- Goal: per-board configurable, conflict-free allocation without source edits; clear error handling when misconfigured.

Components
1) tcu_alloc (3.10/misc/tcu_alloc)
   - API (exported):
     - int tcu_alloc_set_max_channels(unsigned int n);
     - int tcu_alloc_claim(unsigned int ch, const char *owner); // returns 0 or -EBUSY
     - void tcu_alloc_release(unsigned int ch, const char *owner);
     - bool tcu_alloc_is_claimed(unsigned int ch);
     - const char *tcu_alloc_owner(unsigned int ch);
   - Logs claims, releases, and conflicts.

2) PWM driver (3.10/misc/pwm)
   - New module param: pwm.tcu_channels (charp)
     - Example: pwm.tcu_channels=0,1,3
     - Gating: only the listed channels will register platform drivers (tcu_chnX).
   - Probe-time arbitration: claims the TCU channel via tcu_alloc; on conflict prints owner and returns -EBUSY.
   - Backward-compat: if the param is not set, compiled CONFIG_PWMn macros decide which channels are registered (as before).

3) Motor driver (3.10/misc/motor)
   - New module param: motor.tcu_channels (CSV string, e.g., "2" or "2,3")
   - Binding: the driver binds to the first valid channel listed; additional channels are reserved for future multi-channel support.
   - Dynamic binding: .driver.name (and T40+ .of_match) are built from the first channel.
   - Probe-time arbitration: claims the TCU channel via tcu_alloc; on conflict returns -EBUSY with a clear error.

Device Tree usage (preferred)
- Per-channel tcu nodes should exist (e.g., tcu_chn0..tcu_chn7). Mark unused channels as status = "disabled".
- For PWM: ensure the PWM module param (or compiled CONFIG_PWMn) only includes channels intended for PWM on that board.
- For Motor: set motor.tcu_channels to include the intended channel so the motor driver binds to that node.
- Optional role tagging: you may add a property on the tcu_chnX node, e.g. ingenic,role = "pwm" | "motor" for documentation; enforcement is via module params + tcu_alloc.

Non-DT fallback
- PWM: compile-time Kbuild DEFS -DCONFIG_PWMn selects available channels; optional runtime narrowing with pwm.tcu_channels.
- Motor: set motor.tcu_channels=2 (default bind to 2) to bind to tcu_chn2.

Examples
- T40 board A: PWM on 0,1,3; Motor on 2
  - Kernel cmdline or modprobe.d:
    - pwm.tcu_channels=0,1,3
    - motor.tcu_channels=2
  - DTS: ensure tcu_chn2 is present and status = "okay"; PWM pins muxed for 0,1,3.

- T31 board B: PWM on 0,2; Motor on 1
  - pwm.tcu_channels=0,2
  - motor.tcu_channels=1

Troubleshooting
- If a conflict occurs, you’ll see:
  - TCU: channel X already claimed by Y, cannot assign to Z
  - <driver>: TCU chX busy (owner=Y), refusing to bind: -16
- Resolve by adjusting pwm.tcu_channels and/or motor.tcu_channel so no overlap exists. Ensure DTS status for unused channels is "disabled" as appropriate.

Migration guide
- Remove any in-source CONFIG_PWMn auto-enables. Use Kbuild/Kconfig or module params to select channels.
- For existing boards that used PWM2 alongside Motor on tcu_chn2, set pwm.tcu_channels to exclude 2, and set motor.tcu_channel=2.

Build notes
- Build modules: tcu_alloc, pwm_core, motor
- Load order: tcu_alloc must be available before pwm/motor (MODULE_SOFTDEP hints this). With modprobe, dependencies resolve automatically.

Validation plan
- Boot with intentional overlap (e.g., pwm.tcu_channels=2 and motor.tcu_channels=2): motor or pwm probe should fail with -EBUSY and clear logs; no kernel oops.
- Boot with disjoint channels; verify drivers bind and operate; verify bcmdhd loads without crash.

