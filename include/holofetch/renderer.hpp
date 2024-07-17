#pragma once

#include <compare>
#include <string_view>
#include <span>
#include <map>
#include <vector>
#include <filesystem>
#include <format>

namespace holofetch {

    struct console {
        void* handle{nullptr};
        uint32_t width{0};
        uint32_t height{0};

        ~console() = default;
        console();

        operator bool() const noexcept { return handle != nullptr; }

        void put(std::string_view str) const;
        void put(std::wstring_view wstr) const;

        template <class... Args>
        void print(std::string_view fmt, Args&&... args) const {
            std::string str = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
            put(str);
        }

        template <class... Args>
        void print(std::wstring_view fmt, Args&&... args) const {
            std::wstring wstr = std::vformat(fmt, std::make_wformat_args(std::forward<Args>(args)...));
            put(wstr);
        }
    };

    namespace ANSI {
        constexpr std::string_view fg_black{"\033[30m"};
        constexpr std::string_view fg_red{"\033[31m"};
        constexpr std::string_view fg_green{"\033[32m"};
        constexpr std::string_view fg_yellow{"\033[33m"};
        constexpr std::string_view fg_blue{"\033[34m"};
        constexpr std::string_view fg_magenta{"\033[35m"};
        constexpr std::string_view fg_cyan{"\033[36m"};
        constexpr std::string_view fg_white{"\033[37m"};

        constexpr std::string_view fg_bright_black{"\033[90m"};
        constexpr std::string_view fg_bright_red{"\033[91m"};
        constexpr std::string_view fg_bright_green{"\033[92m"};
        constexpr std::string_view fg_bright_yellow{"\033[93m"};
        constexpr std::string_view fg_bright_blue{"\033[94m"};
        constexpr std::string_view fg_bright_magenta{"\033[95m"};
        constexpr std::string_view fg_bright_cyan{"\033[96m"};
        constexpr std::string_view fg_bright_white{"\033[97m"};
        
        constexpr std::string_view fg_bold_black{"\033[1;30m"};
        constexpr std::string_view fg_bold_red{"\033[1;31m"};
        constexpr std::string_view fg_bold_green{"\033[1;32m"};
        constexpr std::string_view fg_bold_yellow{"\033[1;33m"};
        constexpr std::string_view fg_bold_blue{"\033[1;34m"};
        constexpr std::string_view fg_bold_magenta{"\033[1;35m"};
        constexpr std::string_view fg_bold_cyan{"\033[1;36m"};
        constexpr std::string_view fg_bold_white{"\033[1;37m"};

        constexpr std::string_view fg_bold_bright_black{"\033[1;90m"};
        constexpr std::string_view fg_bold_bright_red{"\033[1;91m"};
        constexpr std::string_view fg_bold_bright_green{"\033[1;92m"};
        constexpr std::string_view fg_bold_bright_yellow{"\033[1;93m"};
        constexpr std::string_view fg_bold_bright_blue{"\033[1;94m"};
        constexpr std::string_view fg_bold_bright_magenta{"\033[1;95m"};
        constexpr std::string_view fg_bold_bright_cyan{"\033[1;96m"};
        constexpr std::string_view fg_bold_bright_white{"\033[1;97m"};
        
        constexpr std::string_view fg_underline_black{"\033[4;30m"};
        constexpr std::string_view fg_underline_red{"\033[4;31m"};
        constexpr std::string_view fg_underline_green{"\033[4;32m"};
        constexpr std::string_view fg_underline_yellow{"\033[4;33m"};
        constexpr std::string_view fg_underline_blue{"\033[4;34m"};
        constexpr std::string_view fg_underline_magenta{"\033[4;35m"};
        constexpr std::string_view fg_underline_cyan{"\033[4;36m"};
        constexpr std::string_view fg_underline_white{"\033[4;37m"};

        constexpr std::string_view fg_underline_bright_black{"\033[4;90m"};
        constexpr std::string_view fg_underline_bright_red{"\033[4;91m"};
        constexpr std::string_view fg_underline_bright_green{"\033[4;92m"};
        constexpr std::string_view fg_underline_bright_yellow{"\033[4;93m"};
        constexpr std::string_view fg_underline_bright_blue{"\033[4;94m"};
        constexpr std::string_view fg_underline_bright_magenta{"\033[4;95m"};
        constexpr std::string_view fg_underline_bright_cyan{"\033[4;96m"};
        constexpr std::string_view fg_underline_bright_white{"\033[4;97m"};

        constexpr std::string_view bg_black{"\033[40m"};
        constexpr std::string_view bg_red{"\033[41m"};
        constexpr std::string_view bg_green{"\033[42m"};
        constexpr std::string_view bg_yellow{"\033[43m"};
        constexpr std::string_view bg_blue{"\033[44m"};
        constexpr std::string_view bg_magenta{"\033[45m"};
        constexpr std::string_view bg_cyan{"\033[46m"};
        constexpr std::string_view bg_white{"\033[47m"};

        constexpr std::string_view bg_bright_black{"\033[100m"};
        constexpr std::string_view bg_bright_red{"\033[101m"};
        constexpr std::string_view bg_bright_green{"\033[102m"};
        constexpr std::string_view bg_bright_yellow{"\033[103m"};
        constexpr std::string_view bg_bright_blue{"\033[104m"};
        constexpr std::string_view bg_bright_magenta{"\033[105m"};
        constexpr std::string_view bg_bright_cyan{"\033[106m"};
        constexpr std::string_view bg_bright_white{"\033[107m"};

        constexpr std::string_view bold{"\033[1m"};
        constexpr std::string_view underline{"\033[4m"};
        constexpr std::string_view reset{"\033[0m"};
    }

    std::string read_file(const std::filesystem::path& filename);
    std::vector<std::string_view> split_lines(std::string_view content);
    size_t get_line_length_excluding_ansi_sequences(std::string_view line);
    size_t max_line_length_excluding_ansi_sequences(const std::vector<std::string_view>& lines);

    struct palette {
        std::string_view border{ANSI::fg_bold_bright_black};
        std::string_view section{ANSI::fg_cyan};
        std::string_view property{ANSI::fg_bright_cyan};
        std::string_view comment{ANSI::fg_bright_black};
    };


    struct prerender {
        std::string data;
        std::vector<std::string_view> lines;
        uint32_t width{0};
        uint32_t height{0};
        
        /*
         * Similar to print/println
         * Centers the image if width is greater than this->width
         * Returns false if nothing to draw
         */
        bool draw(const console& out, std::string_view tab);
        bool drawln(const console& out, std::string_view tab);
        bool draw(const console& out, size_t width = 0);
        bool drawln(const console& out, size_t width = 0);
        bool filldrawln(std::string_view color, const console& out, size_t width = 0);
        
    private:
        size_t _drawn_lines{0};
    };

    struct image {
        std::string data;
        std::string texture;
        std::string border_left;
        std::string border_right;
        std::string border_top;
        std::string border_bottom;

        prerender render(const palette& p) const;
    };

    struct section {
        std::string_view header;
        std::vector<std::pair<std::string, std::string>> properties;
        
        std::pair<size_t, size_t> max_lengths() const noexcept;
        
        size_t width() const noexcept {
            auto [m1, m2] = max_lengths();
            return m1+m2+2;
        }

        size_t height() const noexcept {
            return 1 + properties.size();
        }

        prerender render(const palette& p, size_t mkl = 0) const;
    };

    class renderer {
        image avatar_;
        image header_;
        console con_;
        palette palette_;
        bool portrait_mode_{false};
    public:
        renderer() = default;
        ~renderer() = default;

        void set_palette(palette p) noexcept { 
            palette_ = p; 
        }

        void set_avatar(image&& img) noexcept;

        void set_header(image&& img) noexcept {
            header_ = std::forward<image>(img);
        }

        void set_portrait_mode(bool enabled) {
            portrait_mode_ = enabled;
        }

        void draw(const std::vector<section>& sections);

    private:
        size_t console_left_offset(size_t content_width);
        size_t landscape_mode_top_offset(size_t content_height);
        size_t portrait_mode_left_offset(size_t content_width);
    };

} // namespace holofetch
