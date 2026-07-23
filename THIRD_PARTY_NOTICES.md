# Third-Party Notices

This repository contains project-authored integration code together with files
from, or derived from, third-party SDKs and development-board examples.

## T113 and LVGL application

- The T113/LVGL application combines project-authored UI pages, gateway data
  integration and interaction logic with selected code or structural ideas
  adapted from an existing T113/LVGL teaching project.
- Project-authored additions include the distributed-gateway status
  integration, live sensor dashboard, online-state display and related
  data-refresh logic.
- Files containing third-party-derived code or existing author notices retain
  their original attribution.
- LVGL and the complete T113 SDK are not vendored in this public package. They
  must be obtained from their respective upstream distributions and remain
  subject to their own licenses.
- Fonts, music, avatars and most image resources are intentionally omitted
  because their redistribution rights have not been established.

## STM32 firmware

- STM32 CMSIS and HAL files are provided by STMicroelectronics and remain
  subject to the notices and license terms embedded in those files and their
  upstream software package.
- Board-support and example code derived from the ALIENTEK STM32F103 examples
  remains attributable to its original authors. Existing notices must not be
  removed. Before commercial redistribution, review the terms supplied with
  the original development-board package.

## General rule

The root `LICENSE` applies only to code authored specifically for this project.
It does not relicense third-party code. When a source file contains its own
license or copyright notice, that file-level notice takes precedence.
