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

        // Start a new UI window
        static bool BeginWindow(const std::string& title, float x, float y, float width, float height, WindowFlags flags = WindowFlags::Default);

        // End the current UI window
        static void EndWindow();

        // Create a new layout row. item_height is the height of the row. cols is how many items per row.
        static void LayoutRowDynamic(float item_height, int cols);

        // Create a button. Returns true if clicked.
        static bool Button(const std::string& label);

        // Create a text label
        static void Label(const std::string& text);

        // Create a float slider
        static bool SliderFloat(float min, float* val, float max, float step);

        // Create an int slider
        static bool SliderInt(int min, int* val, int max, int step);

        // Create a checkbox
        static bool Checkbox(const std::string& label, bool* active);

        // Create a radio button
        static bool RadioButton(const std::string& label, bool active);

        // Create a property int slider/input
        static void PropertyInt(const std::string& name, int min, int* val, int max, int step, float inc_per_pixel);

        // Create a color picker
        static bool ColorPicker(Color* color);

        // Create a combo box (drop down box)
        static bool Combo(const std::vector<std::string>& items, int* selected, int item_height, float width, float height);

        // Theming functions
        static void SetTheme(UITheme theme);
        static void SetElementStyleColor(UIElement element, const Color& color);
    };

}
