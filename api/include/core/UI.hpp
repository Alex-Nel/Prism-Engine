#include "../PrismAPI.hpp"
#include <string>


namespace Prism
{
    class PRISM_API UI
    {
    public:
        UI() = delete;

        // Start a new UI window
        static bool BeginWindow(const std::string& title, float x, float y, float width, float height);

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
    };

}
