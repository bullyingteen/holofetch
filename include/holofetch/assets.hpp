#pragma once

#include <string_view>

namespace holofetch::assets {

    constexpr std::string_view default_image_texture = "I@love@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@"
    "and@spice@and@wolf@and@spice@and@wolf@and@by@the@way@I@love@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@"
    "and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@stein;gate@as@well@and@spice@and@wolf@"
    "and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@spice@and@wolf@and@";

    constexpr std::string_view default_header = R"(                                                                 
 _____       _                            _   _    _       _  __ 
/  ___|     (_)                          | | | |  | |     | |/ _|
\ `--. _ __  _  ___ ___    __ _ _ __   __| | | |  | | ___ | | |_ 
 `--. \ '_ \| |/ __/ _ \  / _` | '_ \ / _` | | |/\| |/ _ \| |  _|
/\__/ / |_) | | (_|  __/ | (_| | | | | (_| | \  /\  / (_) | | |  
\____/| .__/|_|\___\___|  \__,_|_| |_|\__,_|  \/  \/ \___/|_|_|  
    | |                                                          
    |_|                                                          
                                                                 
)";

    constexpr std::string_view whitespaces = //[256]
    "                                                                                                                                "
    "                                                                                                                                ";
    
    constexpr std::string_view dots = //[64]
    "................................................................";
}