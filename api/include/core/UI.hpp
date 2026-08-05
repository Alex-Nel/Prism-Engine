#include "../PrismAPI.hpp"
#include "Color.hpp"
#include <string>
#include <vector>
#include <cstdint>


namespace Prism
{
    enum UITheme
    {
        Dark = 0,
        Light = 1,
        Red = 2,
        Blue = 3
    };



    enum UIElement
    {
        Text = 0,
        Window,
        Header,
        Border,
        Button,
        ButtonHover,
        ButtonActive,
        Toggle,
        ToggleHover,
        ToggleCursor,
        Select,
        SelectActive,
        Slider,
        SliderCursor,
        SliderCursorHover,
        SliderCursorActive,
        Property,
        Edit,
        EditCursor,
        Combo,
        Chart,
        ChartColor,
        ChartColorHighlight,
        Scrollbar,
        ScrollbarCursor,
        ScrollbarCursorHover,
        ScrollbarCursorActive,
        TabHeader
    };



    enum WindowFlags
    {
        None              = 0,
        Bordered          = 1 << 0,
        Movable           = 1 << 1,
        Scalable          = 1 << 2,
        Closable          = 1 << 3,
        Minimizable       = 1 << 4,
        NoScrollbar       = 1 << 5,
        Title             = 1 << 6,
        ScrollAutoHide    = 1 << 7,
        Background        = 1 << 8,
        ScaleLeft         = 1 << 9,
        NoInput           = 1 << 10,
        Default           = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4) | (1 << 6)
    };



    inline WindowFlags operator|(WindowFlags a, WindowFlags b) {
        return static_cast<WindowFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    inline WindowFlags operator&(WindowFlags a, WindowFlags b) {
        return static_cast<WindowFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }



    class PRISM_API UI
    {
    public:
        UI() = delete;

        // Start a new UI window. The title doubles as the window's identity, so it must stay the same every frame.
        static bool BeginWindow(const std::string& title, float x, float y, float width, float height, WindowFlags flags = WindowFlags::Default);

        // Start a new UI window with an identity that is separate from its header text. 'id' must stay the same every frame, 'title' is free to change every frame.
        static bool BeginWindow(const std::string& id, const std::string& title, float x, float y, float width, float height, WindowFlags flags = WindowFlags::Default);

        // End the current UI window
        static void EndWindow();

        // Create a new layout row. item_height is the height of the row. cols is how many items per row.
        static void LayoutRowDynamic(float item_height, int cols);

        // Create a fixed-width layout row
        static void LayoutRowStatic(float item_height, int item_width, int cols);

        // Create a button. Returns true if clicked.
        static bool Button(const std::string& label);

        // Create a text label
        static void Label(const std::string& text);

        // Create a text label that wraps to fit its layout bounds
        static void LabelWrapped(const std::string& text);

        // Create an item that can be selected and deselected
        static bool Selectable(const std::string& label, bool* selected);

        // Create a float slider
        static bool SliderFloat(float min, float* val, float max, float step);

        // Create an int slider
        static bool SliderInt(int min, int* val, int max, int step);

        // Create an optionally interactive progress bar
        static bool ProgressBar(uint32_t* value, uint32_t max, bool modifiable = false);

        // Create a checkbox
        static bool Checkbox(const std::string& label, bool* active);

        // Create a radio button
        static bool RadioButton(const std::string& label, bool active);

        // Create a property int slider/input
        static void PropertyInt(const std::string& name, int min, int* val, int max, int step, float inc_per_pixel);

        // Create a property float slider/input
        static void PropertyFloat(const std::string& name, float min, float* val, float max, float step, float inc_per_pixel);

        // Create a color picker
        static bool ColorPicker(Color* color);

        // Create a combo box (drop down box)
        static bool Combo(const std::vector<std::string>& items, int* selected, int item_height, float width, float height);

        // Create a text input box
        static void TextBox(char* buffer, int max_len);

        // Create a multiline text input area
        static void TextArea(char* buffer, int max_len);

        // Start a scrollable group in the next layout cell. EndGroup must only be called if this returns true.
        static bool BeginGroup(const std::string& id, const std::string& title, WindowFlags flags = WindowFlags::Bordered);

        // End the current group
        static void EndGroup();

        // Create a horizontal separator
        static void Separator(const Color& color, bool rounded = false);

        // Show a tooltip when the next submitted widget is hovered over
        static void Tooltip(const std::string& text);

        // Theming functions
        static void SetTheme(UITheme theme);
        static void SetElementStyleColor(UIElement element, const Color& color);
    };

}
