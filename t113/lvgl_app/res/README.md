# LVGL Resources

The public repository keeps resource path headers but intentionally omits most
fonts, music, avatars and image files because their redistribution rights have
not been established.

Place locally licensed assets in the directories expected by the existing
configuration headers:

```text
res/
|-- font/
|-- image/
`-- music/
```

Do not commit private avatars, commercial fonts, downloaded music or API
credentials. When building inside the original T113 SDK, the resource CMake
file copies these directories next to the application binary.
