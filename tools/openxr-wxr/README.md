# OpenXR-WXR stereo and tracking patches

The scripted text input (`CLIENT0`) mentioned below comes from a Vulkan shim that
is not part of this repository. The runtime works without it.

The upstream runtime previously accepted binary headset packets and text injector packets into one state slot. The shim's nominal 500 Hz `CLIENT0` stream could repeatedly displace the roughly 72 Hz headset state when scripted input started, replacing physical head, IPD and controller data. The default merge policy keeps binary host state and text injector state separately. Fresh host data supplies the head, IPD and reference spaces. Active text input supplies scripted controls and can overlay a pose only for a registered action space. The runtime retires action-space registrations on destruction, purges associated cached pairs, and clears state on session teardown.

An earlier candidate build (SHA-256 `d17fd0c8a9b98acc114f2849278ed658367322184c45de19b6c67dd3ab8c80af`) could still replace generic VIEW, LOCAL and STAGE poses with text poses. The second patch includes that candidate's tracking changes and the correction to the overlay. It preserves the first patch's 0319 stereo crop and IPD fallback, including the existing synthetic head/yaw path. When both UDP streams stop, cached input can persist as before.

## Tracking policy and right-hand diagnostic

The runtime reads `D:/Winlator/oxrwxr/conf.txt` when creating an instance. It has
no environment selector for this policy. With no `tracking_source` line, the
build starts in merge mode.

| Config value | Policy | Behavior |
| --- | --- | --- |
| `legacy` | 0 | The latest accepted packet supplies all fields. Any unrecognized value also selects legacy. |
| `merge` | 1 | A binary host younger than 1000 ms supplies IPD, frame, recenter and reference poses. Text younger than 500 ms can OR buttons, replace an analog axis of smaller absolute magnitude and overlay registered action poses. |
| `host_only` | 2 | Ignore text while the binary host is younger than 1000 ms. |

In merge, fresh text can replace an exact registered action-space/base pair
when scripted buttons or axes are active, or the host pose is absent, invalid
or identity at the origin. Fresh nonidentity text head input can compose a
synthetic offset onto host head pairs. Other host reference pairs remain
unchanged. With stale or missing binary input, the existing merge fallback can
retain the last text state.

For a controlled pointer test, set both lines before instance creation.

```ini
tracking_source=merge
text_priority_right_poses=true
```

This opt-in changes only action spaces whose resolved hand is right and whose
resolved binding is exactly `/user/hand/right/input/aim/pose` or
`/user/hand/right/input/grip/pose`. After action-set attachment, the runtime
resolves the role from retained suggestions. If the action has bindings on
the attached profile, only those bindings apply. Otherwise the other
suggestions must agree on the right-hand role. An explicit right subaction
filters out left-hand suggestions in either order. An implicit subaction
with eligible suggestions on both hands stays outside the diagnostic.
Conflicting right roles, palm, grip surface and unrelated bindings keep their
original lookup and stay outside the diagnostic log. The runtime
looks up the exact action-space/base-space pair in the latest `CLIENT0` text
packet for less than 500 ms from receipt, whether or not binary is fresh.
Both sources require complete, finite records and a quaternion whose norm is
within 0.001 of one. Four-decimal text rounding changes the norm by at most
0.0001. Accepted rotations are normalized before use. The text
protocol does not transmit an active bit. The existing shim sends `CLIENT0`
only while its command is active, so stopping the shim stream takes effect
when the last packet expires. This lookup is evaluated by `xrLocateSpace`
and also applies through `xrLocateSpaces`. It falls back to the latest usable
binary pair even when no new packets arrive, provided the Android-facing
registration used the same exact aim or grip component. The legacy
registration can select a different grip bit or palm offset based on the
last suggestion. In that case the diagnostic omits the binary fallback
and returns zero location flags when text is unavailable.

The text shim pose is interpreted in the base named by the record. The runtime
composes the original application action-space offset onto that text sample. A binary
sample already includes the offset. The diagnostic does not fabricate missing
pairs or convert the shim's repeated transforms between different bases.
Both located sources use the existing four valid/tracked location bits
(`0xF`). Velocity flags are separate. A translated text action offset moves the
located origin. The diagnostic clears linear-velocity validity in that case
because the raw velocity describes the sampled origin. Angular velocity
retains its packet validity, and binary samples retain both packet flags.
HMD, left hand, reference pairs, buttons and analog axes follow the original
merge rules.

With this option, `RIGHT_POSE_SOURCE sync=N aim_mask=A grip_mask=G` reports
the right-hand locate queries since the previous sync boundary. Masks OR
`1` for missing, `2` for binary host and `4` for text. A zero mask means
there were no queries for that role in the interval. Logging is capped at
36,000 syncs per process. A source row records the runtime's selected pose
and does not show that a game used it.

## Eye size key (0004, on the deployed .0319 base)

Games size their render targets from the recommended eye size, which is half
the Winlator screen. Shrinking the screen also shrinks the runtime's Vulkan
preview, which is 1280x720. The
`eye_size` key changes only the recommended size:

```ini
eye_size=480x540
```

Both values must be even and within 64..4096. Otherwise the runtime logs
`conf eye_size ignored` and keeps the default. A valid key logs
`conf eye_size=WxH; Vulkan preview stays the screen, 1280x720` and
`Recommended eye size WxH (conf eye_size)`. Direct transport still takes
precedence. The maximum stays 4096x4096. With the key, the Vulkan preview
stays at the screen size, and the existing linear blit scales each submitted
eye into its half. Without the key, the code paths, sizes and log lines are
unchanged. The D3D11, D3D12 and OpenGL previews still use twice the eye size.
The D3D11 quad overlay is placed in screen coordinates, so it is misplaced
when those apps use the key.

`rebuild.sh INPUT_ROOT OUT 0319` applies 0001 alone and reproduces the host
copy of `openxr_wxr_64.0319.dll` byte for byte (SHA-256 `e521bb64…`,
1,493,504 bytes). The staged device file has the same name and size, but no
recorded hash.
`rebuild.sh INPUT_ROOT OUT eye-size` applies 0001 and 0004 with the same image
base. `bash tools/openxr-wxr/check-eye-size.sh OUT` compiles the runtime's own parser and size
selection on the host. 0004 does not apply on top of 0002 and 0003.

`0005-eye-size-on-text-priority.patch` is 0004 rebased onto 0001-0003. Its
added and removed lines are identical to 0004; only the conf-parse hunk's
context differs, and the `eye_size` block sits right after
`controller_profile` as before. `rebuild.sh INPUT_ROOT OUT
text-priority-eye-size` applies 0001-0003 and 0005 with the default profile's
epoch and image base, and pins `openxr_wxr_64.textprio-eyesize.dll`
(SHA-256 `9decc18e…`, 1,517,568 bytes). Without the key it behaves like the
default build (`0723529c…`): every new branch requires `ConfEyeWidth > 0`,
which only a valid `eye_size` line sets. With the key it behaves like the
eye-size build. `check-host.sh` and `check-eye-size.sh` both run on its output.

## Injected input priority (0006, on text-priority-eye-size)

In merge mode the runtime keeps the larger magnitude of host and text per
analog axis and ORs the buttons. A physical controller stuck at a nonzero
value (for example an idle trigger reporting 0.96) therefore hides an
injected release. With

```ini
tracking_source=merge
text_priority_input=true
```

while any injected text packet is fresh (less than 500 ms since the last
text packet of any kind, the same window the merge already uses), the text
values replace the host's for the eight analog axes (both sticks, triggers and
grips) and all 19 buttons on both hands. The shim sends idle zeros at 500 Hz
while it is active, so with the key on, the physical controllers' sticks,
triggers, grips and buttons do nothing for the whole time the shim is active,
not just during a scripted press. This key is for scripted test runs only and
must not be put in a play `conf.txt`. When text is stale, or the binary host is silent
for a second, the existing merge and fallback rules apply unchanged. Head
pose, controller poses, IPD, FOV and every other path are untouched. The key
is ignored outside `merge`, and logs `text_priority_input enabled` when it
takes effect. Without the key the composed state is identical to the
text-priority-eye-size build. `rebuild.sh INPUT_ROOT OUT text-priority-input`
applies 0001-0003, 0005 and 0006 with the same epoch and image base and pins
`openxr_wxr_64.textprio-input.dll` (SHA-256 `cb299dbf…`, 1,518,592 bytes).
`check-host.sh` on this profile's output covers the key off, on with fresh text,
and on with stale text. It runs those three checks only when the output
contains 0006, and otherwise prints `skip text_priority_input checks`.

## Source and rebuild

[WinlatorXR/OpenXR-WXR commit 5f7a5c4](https://github.com/WinlatorXR/OpenXR-WXR/commit/5f7a5c455050e320578539f9009333a336599659) has `src` tree `93a31dff01d62165ef0b11aad89d9c721e5877e0` and `src/runtime.cpp` blob `ff6fd52a79158526051b0637269ac94588d7977d`. The four pristine source blobs and upstream [GPL-3.0 license](LICENSE) match that commit. The patches are source modifications under the upstream GPL terms.

Use the four upstream files at the paths listed in [inputs.sha256](inputs.sha256). Supply the recorded OpenXR SDK and Vulkan-Headers include trees, `original-include` compatibility headers, and `libvulkan-1.a` import library under the same input root. The relative manifest pins exact bytes. Dependency revision tags were not established; `fetch-inputs.sh` fetches the likely tags (marked unconfirmed in its header) and generates `libvulkan-1.a` from `vulkan-1.def`, and the manifest check decides. Unrelated headers on the compiler's normal include path and the MinGW sysroot are external toolchain inputs.

From any working directory, with Bash, Git and the x86_64 MinGW GCC 16.2.0 toolchain on PATH:

```sh
tools/openxr-wxr/rebuild.sh INPUT_ROOT NEW_OUTPUT_DIR
tools/openxr-wxr/check-host.sh NEW_OUTPUT_DIR
```

Checks per profile, each on that profile's `NEW_OUTPUT_DIR`:

| Profile | Checks |
| --- | --- |
| default | `tools/openxr-wxr/check-host.sh` |
| `0319` | the pinned DLL hash only (no 0002/0003, so `check-host.sh` does not compile) |
| `eye-size` | `bash tools/openxr-wxr/check-eye-size.sh` (no 0002/0003, so no `check-host.sh`) |
| `text-priority-eye-size` | `tools/openxr-wxr/check-host.sh` and `bash tools/openxr-wxr/check-eye-size.sh` |
| `text-priority-input` | `tools/openxr-wxr/check-host.sh` and `bash tools/openxr-wxr/check-eye-size.sh` |

The rebuild verifies both source manifests, applies 0001, 0002 and 0003 in the new output directory, and uses the recorded static C++/GCC/pthread link options, image base and compile/link epoch. The compiler target and version are checked. The final SHA check determines reproducibility for the supplied toolchain and dependencies. The previous two-patch build had SHA-256 `54da98bb840278f4c6a278c36a2b51ec7e74f5fb6f14be34e9238089175cd38b`. The three-patch build produces `openxr_wxr_64.right-pose-diag.dll`, with its expected SHA-256 pinned in the rebuild script. The host fixtures exercise the real UDP parser and composer with fake Winsock, covering IPD, synthetic yaw, PLAY press/release, reference pose preservation, registered aim/grip, destroy/re-register, stale host, session clear, diagnostic pose-only aim/grip priority, exact-pair lookup, malformed input rejection and expiry without a new packet. They also check exact diagnostic role eligibility, invalid and rounded
rotations, and the location flag mask separately from velocity flags. The host fixture also compiles the runtime's binding ingestion, controller registration and diagnostic resolver with minimal XR types. It checks shared hand bindings in both orders, attached-profile filtering, conflicts and binary fallback eligibility with the real UDP selector. The Windows build checks that the full runtime API wiring compiles. Wine, Quest and NMS behavior remain unverified.

