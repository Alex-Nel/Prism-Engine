#include "../include/core/UI.hpp"



extern "C"
{
    #include "../../../core/ui_core.h"
}



namespace Prism {

    bool UI::BeginWindow(const std::string& title, float x, float y, float width, float height) {
        return UI_BeginWindow(title.c_str(), x, y, width, height);
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

}
