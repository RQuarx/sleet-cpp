#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <boost/ut.hpp>

#include "sleet/widget/layout.hh"
#include "sleet/widget/modifier.hh"
#include "sleet/widgets.hh"


namespace
{
    using sleet::math::vec2;
    using sleet::widget::font_spec;
    using sleet::widget::layout_result;
    using sleet::widget::node_data;

    constexpr auto char_width  = 10.F;
    constexpr auto line_height = 20.F;


    /// A deterministic measurer. Every byte is 10 px wide and every line is
    /// 20 px tall.
    auto measure(std::string_view text, const font_spec &, std::optional<float> width_limit)
        -> kei::result<vec2>
    {
        if (text.empty()) return vec2 { 0.F, 0.F };

        float       line_width = 0.F;
        float       max_width  = 0.F;
        std::size_t lines      = 1;

        for (const auto byte : text)
        {
            if (byte == '\n')
            {
                max_width  = std::max(max_width, line_width);
                line_width = 0.F;
                lines++;
                continue;
            }

            if (width_limit and line_width + char_width > *width_limit and line_width > 0.F)
            {
                max_width  = std::max(max_width, line_width);
                line_width = 0.F;
                lines++;
            }

            line_width += char_width;
        }

        max_width = std::max(max_width, line_width);
        return vec2 { max_width, line_height * static_cast<float>(lines) };
    }


    template <typename W>
    auto layout_of(W &&widget, vec2 viewport = { 200.F, 100.F }) -> layout_result
    {
        auto root = sleet::widget::to_data(std::forward<W>(widget));

        const auto finalized = root.finalize();
        if (!finalized) throw std::runtime_error { finalized.error().what() };

        auto result = sleet::widget::calculate_layout(root, measure, viewport);
        if (!result) throw std::runtime_error { result.error().what() };
        return std::move(*result);
    }
}


const boost::ut::suite layout_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;

    "fixed children, padding, and gap"_test = []
    {
        auto tree = row()[box() | id("a") | width(50.F) | height(20.F),
                          box() | id("b") | width(30.F) | height(10.F)]
                  | padding(10.F) | gap(5.F) | size(200.F, 100.F);

        const auto result = layout_of(tree);

        expect(eq(result.root.position.x, 0.F));
        expect(eq(result.root.size.x, 200.F));
        expect(eq(result.root.size.y, 100.F));

        const auto *a = result.find("a");
        const auto *b = result.find("b");
        expect(a != nullptr);
        expect(b != nullptr);
        expect(eq(a->position.x, 10.F));
        expect(eq(a->position.y, 10.F));
        expect(eq(a->size.x, 50.F));
        expect(eq(a->size.y, 20.F));
        expect(eq(b->position.x, 65.F));
        expect(eq(b->position.y, 10.F));
        expect(eq(b->size.x, 30.F));
        expect(eq(b->size.y, 10.F));
    };

    "grow children share the remaining main-axis space"_test = []
    {
        auto tree = row()[box() | id("fixed") | width(50.F) | height(20.F),
                          box() | id("grow") | height(20.F) | width(sizing::grow())]
                  | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *grow   = result.find("grow");
        expect(grow != nullptr);
        expect(eq(grow->position.x, 50.F));
        expect(eq(grow->size.x, 150.F));
    };

    "flex weights distribute main-axis space"_test = []
    {
        auto tree = row()[box() | id("a") | height(10.F) | width(sizing::grow()) | flex(1.F),
                          box() | id("b") | height(10.F) | width(sizing::grow()) | flex(3.F)]
                  | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *a      = result.find("a");
        const auto *b      = result.find("b");
        expect(a != nullptr);
        expect(b != nullptr);
        expect(eq(a->size.x, 50.F));
        expect(eq(a->position.x, 0.F));
        expect(eq(b->size.x, 150.F));
        expect(eq(b->position.x, 50.F));
    };

    "expand modifiers stretch on the cross axis"_test = []
    {
        auto tree = row()[box() | id("child") | width(20.F) | vexpand()] | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *child  = result.find("child");
        expect(child != nullptr);
        expect(eq(child->position.y, 0.F));
        expect(eq(child->size.y, 100.F));
    };

    "grow keeps the cross axis at its intrinsic size"_test = []
    {
        auto tree = row()[box()[box() | size(20.F)] | id("child") | height(sizing::grow())]
                  | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *child  = result.find("child");
        expect(child != nullptr);
        expect(eq(child->size.y, 20.F));
    };

    "a lone fill child stretches to the parent"_test = []
    {
        auto tree
            = row()[box() | id("fill") | height(20.F) | width(sizing::fill())] | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *fill   = result.find("fill");
        expect(fill != nullptr);
        expect(eq(fill->size.x, 200.F));
    };

    "box alignment centers children"_test = []
    {
        auto tree = box()[box() | id("child") | size(20.F)] | size(100.F) | halign(align::center)
                  | valign(align::center);

        const auto  result = layout_of(tree);
        const auto *child  = result.find("child");
        expect(child != nullptr);
        expect(eq(child->position.x, 40.F));
        expect(eq(child->position.y, 40.F));
    };

    "fill sizing uses the viewport"_test = []
    {
        auto       tree   = column()[label("hi")] | size(sizing::fill());
        const auto result = layout_of(tree, { 300.F, 200.F });

        expect(eq(result.root.size.x, 300.F));
        expect(eq(result.root.size.y, 200.F));
    };

    "hidden children are skipped by gaps and cursors"_test = []
    {
        auto tree = row()[box() | id("a") | width(20.F) | height(20.F),
                          box() | id("hidden") | width(100.F) | height(20.F) | visible(false),
                          box() | id("b") | width(30.F) | height(20.F)]
                  | gap(5.F) | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *b      = result.find("b");
        expect(b != nullptr);
        expect(eq(b->position.x, 25.F));
        expect(result.find("hidden") == nullptr);
    };

    "hidden subtrees are not measured"_test = []
    {
        auto tree = to_data(column()[label("hidden") | visible(false)] | size(100.F, 100.F));
        expect(static_cast<bool>(tree.finalize()));

        const auto failing
            = [](std::string_view, const font_spec &, std::optional<float>) -> kei::result<vec2>
        { return sleet::error { "hidden text was measured" }.unexpected(); };

        const auto result = calculate_layout(tree, failing, { 100.F, 100.F });
        expect(static_cast<bool>(result));
    };

    "margins and justify-between are applied"_test = []
    {
        auto tree = row()[box() | id("a") | width(20.F) | height(20.F) | margin(7.F, 4.F),
                          box() | id("b") | width(30.F) | height(20.F)]
                  | justify_content(justify::between) | size(200.F, 100.F);

        const auto  result = layout_of(tree);
        const auto *a      = result.find("a");
        const auto *b      = result.find("b");
        expect(a != nullptr);
        expect(b != nullptr);
        expect(eq(a->position.x, 4.F));
        expect(eq(b->position.x, 200.F - 30.F));
    };

    "fill-width text is re-measured at the resolved width"_test = []
    {
        auto tree = column()[label("aaaa bbbb") | id("text") | width(sizing::fill())] | width(50.F)
                  | height(100.F);

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(text->size.x, 50.F));
        expect(eq(text->size.y, 40.F));
    };

    "fit-height columns grow to fit wrapped text"_test = []
    {
        auto tree = column()[label("aaaa bbbb") | id("text") | width(sizing::fill())] | width(50.F);

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(result.root.size.x, 50.F));
        expect(eq(result.root.size.y, 40.F));
        expect(eq(text->size.x, 50.F));
        expect(eq(text->size.y, 40.F));
    };

    "fit-height containers use resolved child sizes"_test = []
    {
        auto tree = column()[column()[label("aaaa bbbb") | width(sizing::fill())] | id("inner")
                             | width(50.F)]
                  | width(50.F);

        const auto  result = layout_of(tree);
        const auto *inner  = result.find("inner");
        expect(inner != nullptr);
        expect(eq(inner->size.x, 50.F));
        expect(eq(inner->size.y, 40.F));
        expect(eq(result.root.size.y, 40.F));
    };

    "fit-height boxes grow to fit wrapped text"_test = []
    {
        auto tree = box()[label("aaaa bbbb") | width(sizing::fill())] | width(50.F);

        const auto result = layout_of(tree);
        expect(eq(result.root.size.x, 50.F));
        expect(eq(result.root.size.y, 40.F));
    };

    "a root text node wraps at a fill width"_test = []
    {
        auto tree = to_data(label("aaaa bbbb") | width(sizing::fill()));
        expect(static_cast<bool>(tree.finalize()));

        const auto result = calculate_layout(tree, measure, { 50.F, 100.F });
        expect(static_cast<bool>(result));
        expect(eq(result->root.size.x, 50.F));
        expect(eq(result->root.size.y, 40.F));
    };

    "fit-width text wraps inside a fixed-width column"_test = []
    {
        auto tree = column()[label("aaaa bbbb") | id("text")] | width(50.F);

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(text->size.x, 50.F));
        expect(eq(text->size.y, 40.F));
        expect(eq(result.root.size.x, 50.F));
        expect(eq(result.root.size.y, 40.F));
    };

    "fit-width text wraps inside a fixed-width box"_test = []
    {
        auto tree = box()[label("aaaa bbbb") | id("text")] | width(50.F);

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(text->size.x, 50.F));
        expect(eq(text->size.y, 40.F));
        expect(eq(result.root.size.x, 50.F));
        expect(eq(result.root.size.y, 40.F));
    };

    "fit-width text in a row keeps its intrinsic width"_test = []
    {
        auto tree = row()[label("aaaa bbbb") | id("text")] | width(50.F) | height(100.F);

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(text->size.x, 90.F));
        expect(eq(text->size.y, 20.F));
    };

    "fit-width columns grow to the unwrapped text width"_test = []
    {
        auto tree = column()[label("aaaa bbbb") | id("text")];

        const auto  result = layout_of(tree);
        const auto *text   = result.find("text");
        expect(text != nullptr);
        expect(eq(result.root.size.x, 90.F));
        expect(eq(result.root.size.y, 20.F));
        expect(eq(text->size.x, 90.F));
        expect(eq(text->size.y, 20.F));
    };

    "floating children are out of flow and painted above"_test = []
    {
        auto tree = column()[box() | id("header") | width(100.F) | height(30.F),
                             box() | id("low") | width(100.F) | height(20.F) | floating()
                                 | placement(0.F, 35.F) | z_index(5),
                             box() | id("high") | width(100.F) | height(20.F) | floating()
                                 | placement(10.F, 40.F) | z_index(10),
                             box() | id("after") | width(100.F) | height(10.F)]
                  | width(100.F);

        const auto  result = layout_of(tree);
        const auto *header = result.find("header");
        const auto *low    = result.find("low");
        const auto *high   = result.find("high");
        const auto *after  = result.find("after");

        expect(header != nullptr);
        expect(low != nullptr);
        expect(high != nullptr);
        expect(after != nullptr);

        // The floating nodes do not consume flow space.
        expect(eq(after->position.y, 30.F));
        expect(eq(result.root.size.y, 40.F));

        expect(eq(low->position.y, 35.F));
        expect(eq(high->position.x, 10.F));
        expect(eq(high->position.y, 40.F));

        // Paint order is ascending z-index.
        expect(eq(result.floating.size(), 2_u));
        expect(result.floating[0].id == widget_id { "low" });
        expect(result.floating[1].id == widget_id { "high" });
    };

    "duplicate explicit ids fail before layout"_test = []
    {
        auto tree = box()[box() | id("same"), box() | id("same")];
        expect(!tree.finalize());
    };
};


auto main() -> int {}
