# STM32 vendor sources

The repository carries only the vendor files required by the STM32F103C8T6
firmware. They come from the official STMicroelectronics repositories and are
pinned so a build does not silently change when an upstream branch moves.

| Component | Version / commit | Upstream |
|---|---|---|
| STM32CubeF1 package | `v1.8.7` (`d12e75247d5bcedc734f829b394517ab4c2726e3`) | `STMicroelectronics/STM32CubeF1` |
| STM32F1 HAL driver | `fee494a92b5ad331f92ad21f76c66a5cb83773ee` | `STMicroelectronics/stm32f1xx-hal-driver` |
| STM32 USB Device middleware | `e5a58ee2260204bfed632c7c56da0fdb41514a51` | `STMicroelectronics/stm32-mw-usb-device` |
| CMSIS Device F1 | `c8e9a4a4f16b6d2cb2a2083cbe5161025280fb22` | `STMicroelectronics/cmsis-device-f1` |
| CMSIS Core | STM32CubeF1 `v1.8.7` snapshot | `STMicroelectronics/STM32CubeF1` |

Only the GCC and Cortex-M3 CMSIS headers are retained. Headers for Cortex-M0,
M4, M7, M23, M33, Armv8-M, Arm Compiler and IAR targets are intentionally not
part of this STM32F103 project. The ST license files remain next to the vendor
sources.
