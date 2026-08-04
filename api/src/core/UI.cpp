#include "../include/core/UI.hpp"



extern "C"
{
    #include "../../../core/ui_core.h"
}



namespace Prism
{

    bool UI::BeginWindow(const std::string& title, float x, float y, float width, float height, WindowFlags flags) {
        return UI_BeginWindow(title.c_str(), title.c_str(), x, y, width, height, static_cast<int>(flags));
    }

    bool UI::BeginWindow(const std::string& id, const std::string& title, float x, float y, float width, float height, WindowFlags flags) {
        return UI_BeginWindow(id.c_str(), title.c_str(), x, y, width, height, static_cast<int>(flags));
    }

    void UI::EndWindow() {
        UI_EndWindow();
    }

    void UI::LayoutRowDynamic(float item_height, int cols) {
        UI_LayoutRowDynamic(item_height, cols);
    }

    bool UI::Button(const std::string& label) {
        return UI_Button(label.c_str());
    }

    void UI::Label(const std::string& text) {
         UI_Label(text.c_str());
    }

    bool UI::SliderFloat(float min, float* val, float max, float step) {
        return UI_SliderFloat(min, val, max, step);
    }

    bool UI::SliderInt(int min, int* val, int max, int step) {
        return UI_SliderInt(min, val, max, step);
    }

    bool UI::Checkbox(const std::string& label, bool* active) {
        return UI_Checkbox(label.c_str(), active);
    }

    bool UI::RadioButton(const std::string& label, bool active) {
        return UI_RadioButton(label.c_str(), active);
    }

    void UI::PropertyInt(const std::string& name, int min, int* val, int max, int step, float inc_per_pixel) {
        UI_PropertyInt(name.c_str(), min, val, max, step, inc_per_pixel);
    }

    bool UI::ColorPicker(Color* color) {
        return UI_ColorPicker(reinterpret_cast<::Color*>(color));
    }

    bool UI::Combo(const std::vector<std::string>& items, int* selected, int item_height, float width, float height) {
        std::vector<const char*> c_items;
        c_items.reserve(items.size());
        for (const auto& item : items) {
            c_items.push_back(item.c_str());
        }
        return UI_Combo(c_items.data(), static_cast<int>(c_items.size()), selected, item_height, width, height);
    }

    void UI::TextBox(char* buffer, int max_len) {
        UI_TextBox(buffer, max_len);
    }

    void UI::SetTheme(UITheme theme) {
        UI_SetTheme(static_cast<int>(theme));
    }

    void UI::SetElementStyleColor(UIElement element, const Color& color) {
        UI_SetElementStyleColor(static_cast<int>(element), *reinterpret_cast<const ::Color*>(&color));
    }

}
