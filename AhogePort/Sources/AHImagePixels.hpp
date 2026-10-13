#pragma once
#include <string>
#include <vector>

namespace ah {
struct ImagePixels { int width=0,height=0; std::vector<unsigned char> rgba; };
ImagePixels loadImage(const std::string& file);
}
