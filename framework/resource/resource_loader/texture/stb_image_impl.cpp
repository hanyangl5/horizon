// Keep stb's single-file implementation isolated from unity builds.
// FIXME(hyl5): stb complains after directxmath, https://github.com/nothings/stb/issues/978
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
