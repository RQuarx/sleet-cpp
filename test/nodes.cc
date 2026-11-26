#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/ut.hpp>

#include "sleet/widget/modifier.hh"
#include "sleet/widget/node.hh"
#include "sleet/widgets.hh"


const boost::ut::suite node_tests = []
{
    using namespace boost::ut;
    using namespace sleet::widget;
    using namespace std::string_literals;

    "copy and move preserve every layout field"_test = []
    {
        auto original = box()[column()[label("hello") | margin(2.F)] | gap(3.F)] | gap(7.F)
                      | padding(5.F) | margin(1.F) | flex(2.F);

        const auto data = std::move(original).release();
        expect(eq(data.gap, 7.F));
        expect(eq(data.padding.top, 5.F));
        expect(eq(data.margin.left, 1.F));
        expect(eq(data.flex, 2.F));

        auto copy = data;
        expect(eq(copy.gap, 7.F));
        expect(eq(copy.padding.top, 5.F));
        expect(copy.children[0].parent == &copy);
        expect(eq(copy.children[0].gap, 3.F));

        auto moved = std::move(copy);
        expect(eq(moved.gap, 7.F));
        expect(moved.children[0].parent == &moved);
    };

    "deep copies keep every parent pointer inside the copy"_test = []
    {
        auto original = to_data(box()[box()[box()[label("deep")]]]);
        auto copy     = original;

        const auto *first  = &copy.children[0];
        const auto *second = &first->children[0];
        const auto *third  = &second->children[0];

        expect(first->parent == &copy);
        expect(second->parent == first);
        expect(third->parent == second);
        expect(first->parent != &original.children[0]);
    };

    "default and empty widget ids hash consistently"_test = []
    {
        const auto empty          = widget_id {};
        const auto explicit_empty = widget_id { std::string {} };

        expect(empty == explicit_empty);
        expect(eq(std::hash<widget_id> {}(empty), std::hash<widget_id> {}(explicit_empty)));

        std::unordered_set<widget_id> ids;
        ids.insert(empty);
        expect(ids.contains(explicit_empty));
    };

    "finalize assigns stable unique ids"_test = []
    {
        auto tree = column()[box() | id("first"), box()[label("nested")]];
        expect(static_cast<bool>(tree.finalize()));

        const auto &data = tree.data_view();
        expect(!data.id.empty());
        expect(data.find("first") != nullptr);
        expect(data["first"].is_box());
        expect(data.find("missing") == nullptr);

        expect(eq(data.children_amount(), 3_u));
    };

    "generated ids stay stable when unrelated siblings are inserted"_test = []
    {
        const auto build = [](bool insert)
        {
            auto loop = []
            {
                std::vector<node_data> kids;
                for (int i = 0; i < 2; i++) kids.push_back(to_data(box()));
                return kids;
            };

            auto tree = column() | id("root");
            if (insert) tree = tree[box() | id("extra")];
            tree = tree[loop];

            auto data = std::move(tree).release();
            if (const auto result = data.finalize(); !result)
                throw std::runtime_error { result.error().what() };
            return data;
        };

        const auto before = build(false);
        const auto after  = build(true);

        expect(eq(before.children.size(), 2_u));
        expect(eq(after.children.size(), 3_u));
        expect(after.children[0].id == widget_id { "extra" });
        expect(before.children[0].id == after.children[1].id);
        expect(before.children[1].id == after.children[2].id);
    };

    "combo options can use explicit ids"_test = []
    {
        auto tree = to_data(combo_box(
            "", { "A", "B" }, { widget_id { "option-a" }, widget_id { "option-b" } }, "choose"));
        expect(static_cast<bool>(tree.finalize()));
        expect(tree.find("option-a") != nullptr);
        expect(tree.find("option-b") != nullptr);
        expect(tree.find("option-a")->control == control_kind::combo_option);
    };

    "duplicate explicit ids are rejected"_test = []
    {
        auto       tree   = box()[box() | id("same"), box() | id("same")];
        const auto result = tree.finalize();
        expect(!result);
        expect(std::string { result.error().what() }.find("Duplicate") != std::string::npos);
    };

    "parent pointers survive copies and moves"_test = []
    {
        auto tree = to_data(column()[box()[label("deep")]]);
        auto copy = tree;

        // Mutating the copy must not affect the original: children are owned.
        copy.gap = 12.F;
        expect(eq(copy.gap, 12.F));
        expect(eq(tree.gap, 0.F));

        const auto &root = copy;
        expect(root.children[0].parent == &root);
        expect(root.children[0].children[0].parent == &root.children[0]);
    };

    "assigning a subtree keeps its parent link"_test = []
    {
        auto tree        = to_data(box()[box() | id("slot")]);
        auto replacement = to_data(column()[label("new")]);

        auto *slot = tree.find("slot");
        expect(slot != nullptr);
        if (slot == nullptr) return;

        *slot = std::move(replacement);

        expect(slot->parent == &tree);
        expect(slot->is_column());
        expect(tree.find("slot") == nullptr);
        expect(slot->children[0].parent == slot);
    };

    "modifiers change the expected fields"_test = []
    {
        auto widget = box() | width(sizing::fixed(10.F)) | height(20.F) | padding(1.F, 2.F)
                    | margin(3.F) | background(sleet::color::from_rgb(0x112233))
                    | border(sleet::color::from_rgb(0x445566), 2.F) | on_click("go");

        const auto &data = widget.data_view();
        expect(data.size.x.is_fixed());
        expect(eq(data.size.x.get_fixed_size(), 10.F));
        expect(eq(data.size.y.get_fixed_size(), 20.F));
        expect(eq(data.padding.top, 1.F));
        expect(eq(data.padding.left, 2.F));
        expect(eq(data.margin.top, 3.F));
        expect(data.visual.background.has_value());
        expect(eq(data.visual.border_width, 2.F));
        expect(eq(data.action, "go"s));
    };

    "text nodes can be modified and queried"_test = []
    {
        auto        widget = label("hello") | text_size(18.F) | text_weight(font_weight::bold)
                           | text_decoration(text_attribute::underline);
        const auto &data   = widget.data_view();
        expect(data.is_text());
        expect(eq(data.text, "hello"s));
        expect(eq(*data.font.size, 18.F));
        expect(*data.font.weight == font_weight::bold);
        expect(data.font.attribute.has(text_attribute::underline));
    };

    "type-erased nodes can be used as children directly"_test = []
    {
        auto tree = to_data(column()[to_data(box() | id("first")), to_data(label("second"))]);
        expect(eq(tree.children.size(), 2_u));
        expect(tree.children[0].id == widget_id { "first" });
        expect(tree.children[1].is_text());
    };

    "factories can return ranges of type-erased nodes"_test = []
    {
        auto tree = to_data(column()[(
            []
            {
                std::vector<node_data> children;
                children.push_back(to_data(label("a")));
                children.push_back(to_data(label("b")));
                return children;
            })]);

        expect(eq(tree.children.size(), 2_u));
        expect(tree.children[0].is_text());
        expect(tree.children[1].is_text());
    };

    "factories can return const ranges of nodes"_test = []
    {
        const auto children = std::vector<node_data> { to_data(label("a")), to_data(label("b")) };

        const auto factory = [&children]() -> const auto & { return children; };
        auto       tree    = to_data(column()[factory]);

        expect(eq(tree.children.size(), 2_u));
        expect(eq(tree.children[0].text, "a"s));
        expect(eq(tree.children[1].text, "b"s));
        expect(eq(children.size(), 2_u));
    };
};


auto main() -> int {}
