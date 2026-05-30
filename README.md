# Procedural Character Animation Plugin

A custom C++ procedural animation plugin built for the N8RO simulation engine. This plugin directly manipulates 10 joints on a bipedal rig using procedural math (sine waves) to create fluid, dynamic movement states without relying on pre-baked keyframe animations.

## Features & Controls

The animation state machine is driven by keyboard inputs. The current states are:

* **Stand (Key 4)**: The default idle state. Features a procedural breathing cycle tied to the elbows and shoulders, alongside a slow, side-to-side weight shift (sway) to keep the character grounded and organic.
* **Walk (Key 1)**: A steady forward walking cycle. The arm swings are phase-inverted from the legs for natural counterbalance. *Note: Walking automatically stops after 10 seconds unless SHIFT is held.*
* **Run (Key 2)**: A high-energy sprinting cycle with deeper knee bends, higher leg lifts, and wider arm swings. *Note: Running automatically stops after 10 seconds unless SHIFT is held.*
* **Jump (Key 3)**: A quick gathering animation where the character drops into a wide, crouched stance before leaping. Automatically recovers to the Stand state after 0.8 seconds.
* **Continuous Movement (SHIFT)**: Holding the `SHIFT` key while pressing `1` or `2` bypasses the 10-second timeout, allowing the character to walk or run indefinitely until `SHIFT` is released.

## Controlled Joints

The plugin outputs real-time rotational data to the following 10 joints:
1. `leftHip` / `rightHip`
2. `leftKnee` / `rightKnee`
3. `leftAnkle` / `rightAnkle`
4. `leftShoulder` / `rightShoulder`
5. `leftElbow` / `rightElbow`

## Build Instructions

To compile the plugin DLL:
1. Open a Developer Command Prompt for Visual Studio.
2. Navigate to the project root directory.
3. Run `build.cmd` (or compile `student-char-anim.vcxproj` using MSBuild).
4. The generated DLL will be deployed to the `userPlugins/sim/` folder. Ensure the N8RO viewer is closed during compilation to avoid file lock errors.

## Technical Details

* **Language:** C++
* **Interpolation:** Uses an exponential decay lerp function (speed = 80.0) to smoothly blend between intra-state frames while preventing robotic joint snapping.
* **Architecture:** Implements the `ARKHEON_PLUGIN_V1` interface for seamless integration with the N8RO engine.
