#include "holofetch/renderer.hpp"

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <format>
#include <concepts>
#include <codecvt>

#include <cassert>

#include "holofetch/assets.hpp"

namespace holofetch {

    std::string read_file(const std::filesystem::path& filename) {
        auto filename_utf8 = filename.string();
        
        FILE* file = fopen(filename_utf8.c_str(), "r");
        if (!file)
            return {};
        
        fseek(file, 0, SEEK_END);
        auto size = ftell(file);
        fseek(file, 0, SEEK_SET);

        std::string content;
        content.resize(size);

        size_t bytes_read = fread(content.data(), sizeof(char), size, file);
        fclose(file);
        
        content.resize(bytes_read);

        return content;
    }

    std::vector<std::string_view> split_lines(std::string_view content) {
        if (content.empty())
            return {};
        
        std::vector<std::string_view> lines;
        
        size_t lastpos = 0;
        size_t pos = content.find('\n');
        while (pos != std::string_view::npos) {
            if (auto ss = content.substr(lastpos, pos - lastpos); !ss.empty()) {
                lines.push_back(ss);
            }
            lastpos = pos + 1;
            pos = content.find('\n', lastpos);
        }

        if (lastpos < content.size())
            lines.push_back(content.substr(lastpos + 1));

        return lines;
    }

    size_t get_line_length_excluding_ansi_sequences(std::string_view line) {
        size_t line_length = 0;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '\033') {
                i = line.find('m', i+1);
                continue;
            }
            ++line_length;
        }
        return line_length;
    }

    size_t max_line_length_excluding_ansi_sequences(const std::vector<std::string_view>& lines) {
        size_t length = 0;
        for (auto line : lines) {
            if (size_t line_length = get_line_length_excluding_ansi_sequences(line); line_length > length) {
                length = line_length;
            }
        }
        return length;
    }


    console::console() {
        _setmode(_fileno(stdout), _O_U8TEXT);

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        
        handle = GetStdHandle(STD_OUTPUT_HANDLE);
        assert(handle != INVALID_HANDLE_VALUE);

        if (!GetConsoleScreenBufferInfo(handle, &csbi))
            assert(false && "ERROR: GetConsoleScreenBufferInfo");

        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    void console::put(std::string_view str) const {
        if (str.empty())
            return;
        DWORD dummy = 0;
        WriteConsoleA(handle, (const void*)str.data(), (DWORD)str.size(), &dummy, NULL);
    }

    void console::put(std::wstring_view wstr) const {
        if (wstr.empty())
            return;
        DWORD dummy = 0;
        WriteConsoleW(handle, (const void*)wstr.data(), (DWORD)wstr.size(), &dummy, NULL);
    }


    bool prerender::draw(const console& out, std::string_view tab) {
        if (_drawn_lines >= lines.size())
            return false;

        std::string_view line = lines[_drawn_lines++];

        out.put(tab);
        out.put(line);

        return true;
    }

    bool prerender::drawln(const console& out, std::string_view tab) {
        bool r = draw(out, tab);
        if (r) {
            out.put("\n");
        }
        return r;
    }

    bool prerender::draw(const console& out, size_t width_) { 
        std::string_view tab;
        if (width_ > this->width) {
            tab = assets::whitespaces.substr(0, (width_ - this->width) / 2);
        }
        
        return draw(out, tab);
    }

    bool prerender::drawln(const console& out, size_t width_) {
        bool r = draw(out, width_);
        if (r) {
            out.put("\n");
        }
        return r;
    }
    
    bool prerender::filldrawln(std::string_view color, const console& out, size_t width_) {
        if (_drawn_lines >= lines.size())
            return false;

        std::string_view line = lines[_drawn_lines++];
        
        std::string_view tab;
        if (width_ > this->width) {
            tab = assets::whitespaces.substr(0, (width_ - this->width) / 2);
        }

        out.put(tab);
        auto colored_line = std::vformat("{}{}{}", std::make_format_args(color, line, ANSI::reset));
        out.put(colored_line);
        out.put("\n");

        return true;
    }


    prerender image::render(const palette& palette) const {
        if (data.empty()) 
            return {};

        std::ostringstream ss;
        
        if (!border_top.empty()) {
            ss << palette.border << border_top << ANSI::reset << "\n";
        }

        auto lines = split_lines(data);
        size_t width = max_line_length_excluding_ansi_sequences(lines);

        size_t texture_offset{0};
        for (std::string_view line : lines) {

            std::string textured_line;
            if (!texture.empty()) {
                textured_line = std::string(line.data(), line.size());
                for (char& c : textured_line) {
                    if (c == '#') {
                        c = texture[texture_offset];
                        texture_offset = (texture_offset+1) % texture.size();
                    }
                }
            }
            
            if (!border_left.empty()) {
                ss << palette.border << border_left << ANSI::reset;
            }

            ss << (textured_line.empty() ? line : std::string_view{textured_line});
            if (size_t line_length = get_line_length_excluding_ansi_sequences(line); line_length < width) {
                ss << assets::whitespaces.substr(0, (width - line_length) / 2);
            }

            if (!border_right.empty()) {
                ss << palette.border << border_right << ANSI::reset;
            }
            ss << "\n";
        }

        if (!border_bottom.empty()) {
            ss << palette.border << border_bottom << ANSI::reset << "\n";
        }

        prerender r;
        r.data = ss.str();
        r.lines = split_lines(r.data);
        r.width = max_line_length_excluding_ansi_sequences(r.lines);
        r.height = r.lines.size();
        return r;
    }

    std::pair<size_t, size_t> section::max_lengths() const noexcept {
        size_t max_key_length = 0;
        size_t max_value_length = 0;
        
        for (const auto& [key, value] : properties) {
            if (size_t key_length = get_line_length_excluding_ansi_sequences(key); key_length > max_key_length) {
                max_key_length = key_length;
            }

            if (size_t value_length = get_line_length_excluding_ansi_sequences(value); value_length > max_value_length) {
                max_value_length = value_length;
            }
        }

        return {max_key_length, max_value_length};
    }

    prerender section::render(const palette& palette, size_t mkl) const {
        if (properties.empty()) 
            return {};

        std::ostringstream ss;

        auto [max_key_length, max_value_length] = this->max_lengths();
        
        if (mkl > max_key_length)
            max_key_length = mkl;

        const size_t width = max_key_length + 2 + max_value_length;

        std::string_view tab = assets::whitespaces.substr(0, 2); // max_key_length);
        // if (size_t header_length = header.size() + 4; header_length < width) {
        //     tab = assets::whitespaces.substr(0, (width - header_length) / 2);
        // }

        // std::string hr(width + 2, '-');
        // ss << "+" << hr << "+" << "\n";

        // - HEADER -
        // ss << "| ";
        ss << "{ " << palette.section << header << ANSI::reset << " }";
        if (size_t current_length = 4 + header.size(); current_length < width ) {
            ss << assets::whitespaces.substr(0, width - current_length);
        }
        // ss << " |";
        ss << "\n";

        for (const auto& [key, value] : properties) {
            auto current_tab = assets::dots.substr(0, max_key_length - key.size());
            //ss << "| ";
            ss << ANSI::fg_bright_black << current_tab << ANSI::reset << palette.property << key << ANSI::fg_bright_black << ": " << ANSI::reset << value;
            if (size_t current_length = max_key_length + 2 + get_line_length_excluding_ansi_sequences(value); current_length < width) {
                ss << assets::whitespaces.substr(0, width - current_length);
            }
            // ss << " |"; 
            ss << "\n";
        }
        
        // ss << "+" << hr << "+" << "\n";

        prerender r;
        r.data = ss.str();
        r.lines = split_lines(r.data);
        r.width = max_line_length_excluding_ansi_sequences(r.lines);
        r.height = r.lines.size();
        return r;
        
    }

    void renderer::set_avatar(image&& img) noexcept {
        avatar_.data = std::move(img.data);
        avatar_.texture = std::move(img.texture);

        if (avatar_.texture.empty() && avatar_.data.find('#') != std::string::npos) {
            avatar_.texture = assets::default_image_texture;
        }
        
        std::string_view avatar_front_line_ = std::string_view{avatar_.data}.substr(0, avatar_.data.find('\n'));
        size_t avatar_width_ = get_line_length_excluding_ansi_sequences(avatar_front_line_);

        /*
         * [border style: lines]
         *  ┌─────┐
         *  │     │
         *  │     │
         *  └─────┘
         */

        // todo: support unicode characters
        // avatar_.border_left = "│ ";
        // avatar_.border_right = "│";
        // avatar_.border_top = "┌" + std::string(avatar_width_ + 1, '─') + "┐";
        // avatar_.border_bottom = "└" + std::string(avatar_width_ + 1, '─') + "┘";
        
        if (bool enabled = false; enabled) {
            avatar_.border_left = "{ ";
            avatar_.border_right = "}";
            avatar_.border_top = "+" + std::string(avatar_width_ + 1, '-') + "+";
            avatar_.border_bottom = avatar_.border_top;
        }
    }

    void renderer::draw(const std::vector<section>& sections) {
        if (avatar_.data.empty()) {
            throw std::runtime_error("avatar image data is not available");
        }

        if (header_.data.empty()) {
            header_.data = assets::default_header;
        }

        auto avatar_pr_ = avatar_.render(palette_);

        std::string_view header_front_line_ = std::string_view{header_.data}.substr(0, header_.data.find('\n')-1);
        size_t header_width_ = get_line_length_excluding_ansi_sequences(header_front_line_);

        // if does not fit
        if (!portrait_mode_ && (avatar_pr_.width + header_width_) >= con_.width) {
            portrait_mode_ = true;
        }

        const bool should_draw_avatar = avatar_pr_.width < con_.width && con_.height > 60;
        if (!should_draw_avatar) {
            portrait_mode_ = true;
        }

        if (header_.border_bottom.empty()) {
            /* 
             * [border style: dots]
             *  .......
             *  :     :
             *  :     :
             *  :.....:
             */
            
            size_t off = (avatar_pr_.width - header_width_) / 2;
            header_.border_left = portrait_mode_ 
                ? assets::whitespaces.substr(0, off)
                : "  "; 
            header_.border_right = portrait_mode_ 
                ? assets::whitespaces.substr(0, off)
                : "  ";
            
            header_.border_bottom = portrait_mode_ 
                ? std::string(avatar_pr_.width, '.')
                : std::string(header_.border_left.size() + header_width_ + header_.border_right.size() + 1, '.');
            
            bool toggle_ = false;
            for (auto& c : header_.border_bottom) {
                if (toggle_) {
                    c = ' ';
                }
                toggle_ = !toggle_;
            }

            // remove top border in portrait mode
            if (!portrait_mode_)
                header_.border_top = header_.border_bottom;
        }

        auto header_pr_ = header_.render(palette_);
        const bool should_draw_header = header_pr_.width < con_.width && con_.height > (should_draw_avatar ? 75 : 75 - avatar_pr_.height);

        constexpr size_t SECTIONS_DELIMITER_SPACES = 2;

        if (portrait_mode_) {
            size_t con_width = con_.width;

            if (should_draw_avatar) {
                while (avatar_pr_.drawln(con_, con_width));
                con_.put("\n");
            } else if (should_draw_header) {
                while (header_pr_.filldrawln(ANSI::fg_green, con_, con_width));
                con_.put("\n");
            } 
            
            if (!should_draw_avatar && !should_draw_header) {
                bool first_ = true;
                for (auto& section : sections) {
                    auto pr = section.render(palette_);
                    if (!first_) {
                        con_.put("\n");
                    }
                    first_ = false;
                    while (pr.drawln(con_, assets::whitespaces.substr(0, 4))); //, con_.width));
                }
                return;
            }

            std::vector<prerender> section_prs;
            section_prs.reserve(sections.size());

            size_t content_width = SECTIONS_DELIMITER_SPACES;
            size_t prev_section_height = static_cast<size_t>(-1);

            std::vector<std::span<prerender>> rows;
            size_t _row_start{ 0 };
            
            for (size_t i = 0; i < sections.size(); ++i) {
                size_t w = sections[i].width();

                if (i == 0) {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = SECTIONS_DELIMITER_SPACES + w;
                    prev_section_height = sections[i].height();
                    continue;
                } else if ((i - _row_start) < 2 && (content_width + SECTIONS_DELIMITER_SPACES + w) < con_.width && sections[i].height() <= prev_section_height) {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = content_width + SECTIONS_DELIMITER_SPACES + w;
                } else {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = SECTIONS_DELIMITER_SPACES + w;
                    prev_section_height = sections[i].height();
                    
                    rows.push_back(std::span<prerender>{section_prs.data() + _row_start, section_prs.data() + i});
                    _row_start = i;
                }
            }

            rows.push_back(std::span<prerender>{section_prs.data() + _row_start, section_prs.data() + section_prs.size()});

            size_t max_offset_left{0};
            for (auto& row : rows) {
                if (row.front().width > max_offset_left)
                    max_offset_left = row.front().width;
            }

            std::string_view first_tab = assets::whitespaces.substr(0, (con_.width - avatar_pr_.width) / 2);
            bool drawn;
            for (auto& row : rows) {
                std::string_view other_tab = assets::whitespaces.substr(0, (max_offset_left - row.front().width) + SECTIONS_DELIMITER_SPACES);
                do {
                    drawn = false;

                    if (row.size() == 1) {
                        if (row.front().draw(con_, con_width)) {
                            drawn = true;
                        }
                        else {
                            con_.put(assets::whitespaces.substr(0, row.front().width + SECTIONS_DELIMITER_SPACES));
                        }
                    } else {
                        bool first = true;
                        for (auto& pr : row) {
                            if (pr.draw(con_, first ? first_tab : other_tab)) {
                                drawn = true;
                            }
                            else {
                                con_.put(assets::whitespaces.substr(0, pr.width + SECTIONS_DELIMITER_SPACES));
                            }
                            first = false;
                        }
                    }

                    con_.put("\n");

                } while (drawn);
            }

        } else {
            size_t con_width = con_.width;
            std::vector<prerender> section_prs;
            section_prs.reserve(sections.size());
            
            size_t content_width = SECTIONS_DELIMITER_SPACES;
            size_t prev_section_height = static_cast<size_t>(-1);

            std::vector<std::span<prerender>> rows;
            size_t _row_start{ 0 };
            
            for (size_t i = 0; i < sections.size(); ++i) {
                size_t w = sections[i].width();

                if (i == 0) {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = SECTIONS_DELIMITER_SPACES + w;
                    prev_section_height = sections[i].height();
                    continue;
                } else if ((i - _row_start) < 2 && (content_width + SECTIONS_DELIMITER_SPACES + w) < con_.width && sections[i].height() <= prev_section_height) {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = content_width + SECTIONS_DELIMITER_SPACES + w;
                } else {
                    section_prs.push_back(sections[i].render(palette_));
                    content_width = SECTIONS_DELIMITER_SPACES + w;
                    prev_section_height = sections[i].height();
                    
                    rows.push_back(std::span<prerender>{section_prs.data() + _row_start, section_prs.data() + i});
                    _row_start = i;
                }
            }

            rows.push_back(std::span<prerender>{section_prs.data() + _row_start, section_prs.data() + section_prs.size()});

            size_t content_height = header_pr_.height + 1;
            for (auto row : rows) {
                content_height += (row.front().height + 1);
            }
            
            size_t max_offset_left{0};
            size_t max_row_length{0};
            for (auto& row : rows) {
                if (row.front().width > max_offset_left)
                    max_offset_left = row.front().width;
                
                size_t row_length{0};
                for (auto& pr : row) {
                    row_length += pr.width + SECTIONS_DELIMITER_SPACES;
                }

                if (row_length > max_row_length)
                    max_row_length = row_length;
            }

            if (content_height > avatar_pr_.height) {
                // restart in portrait mode
                portrait_mode_ = true;
                return this->draw(sections);
            } else {
                // vertical alignment
                size_t offset = (avatar_pr_.height - content_height) / 2;
                for (size_t i = 0; i < offset; ++i) {
                    avatar_pr_.drawln(con_);
                }
            }

            std::string_view header_tab;
            if (max_row_length > header_pr_.width) {
                header_tab = assets::whitespaces.substr(0, (max_row_length - header_pr_.width) / 2);
            }

            avatar_pr_.draw(con_);
            con_.put(header_tab);
            while (header_pr_.filldrawln(ANSI::fg_green, con_)) {
                avatar_pr_.draw(con_);
                con_.put(header_tab);
            }

            con_.put("\n");
            avatar_pr_.draw(con_);

            std::string_view first_tab = assets::whitespaces.substr(0, SECTIONS_DELIMITER_SPACES);
            bool drawn;
            for (auto& row : rows) {
                std::string_view other_tab = assets::whitespaces.substr(0, (max_offset_left - row.front().width) + SECTIONS_DELIMITER_SPACES);

                do {
                    drawn = false;

                    if (row.size() == 1) {
                        if (row.front().draw(con_, con_width)) {
                            drawn = true;
                        }
                        else {
                            con_.put(assets::whitespaces.substr(0, row.front().width + SECTIONS_DELIMITER_SPACES));
                        }
                    } else {
                        bool first = true;
                        for (auto& pr : row) {
                            if (pr.draw(con_, first ? first_tab : other_tab)) {
                                drawn = true;
                            }
                            else {
                                con_.put(assets::whitespaces.substr(0, pr.width + SECTIONS_DELIMITER_SPACES));
                            }
                            first = false;
                        }
                    }

                    con_.put("\n");
                    avatar_pr_.draw(con_);

                } while (drawn);
            }

            con_.put("\n");

            // draw remainder if any
            while (avatar_pr_.drawln(con_));
        }
    }
}
