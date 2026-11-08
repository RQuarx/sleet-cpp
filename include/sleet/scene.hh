#pragma once
#include <cstdint>
#include <optional>


namespace sleet
{
    struct node
    {
        struct size
        {
            enum class mode : std::uint8_t
            {
                hug,
                fill,
                fixed,
            };

            mode  mode;
            float value = 0.F;


            [[nodiscard]]
            static auto hug() noexcept -> size
            { return { .mode = mode::hug, .value = 0 }; }

            [[nodiscard]]
            static auto fill() noexcept -> size
            { return { .mode = mode::fill, .value = 0 }; }

            [[nodiscard]]
            static auto fixed(float value) noexcept -> size
            { return { .mode = mode::fixed, .value = value }; }
        };

        enum class kind : std::uint8_t
        {
            box,
            row,
            column,
            text,
        };

        enum class align : std::uint8_t
        {
            start,
            center,
            end,
        };

        enum class justify : std::uint8_t
        {
            start,
            center,
            end,
            between,
        };


        kind kind = kind::box;
        size width;
        size height;

        float padding = 0;
        float gap = 0;

        align alignment = align::start;
        justify justify = justify::start;

        std::optional<align> align_self; /* child override */
        float flex = 0.F; /* 0 = not flexible */

        
    };


    class scene
    {
    public:
    private:
    };
}
