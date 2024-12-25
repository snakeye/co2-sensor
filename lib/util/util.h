/**
 * Utility classes and functions
 */
#pragma once

#define TIME_TO_MS(hours, minutes, seconds) \
    (((hours) * 3600 + (minutes) * 60 + (seconds)) * 1000)

/**
 * Define value range
 */
template <typename T>
class range
{
public:
    range(T value_min, T value_max) : min(value_min), max(value_max) {}

    T clamp(T value)
    {
        if (value < min)
            return min;
        if (value > max)
            return max;
        return value;
    }

public:
    T min;
    T max;
};

/**
 * Execute action if the value has changed
 */
template <typename T, typename Func>
inline void setIfChanged(T &prev, T value, Func action)
{
    if (prev == value)
        return;
    action(value);
    prev = value;
}

/**
 * Perform linear transformation
 */
template <typename T>
inline T transform(T input, range<T> input_range, range<T> output_range)
{
    input = input_range.clamp(input);

    T input_range_diff = input_range.max - input_range.min;
    T output_range_diff = output_range.max - output_range.min;

    return output_range.min + (input - input_range.min) * (output_range_diff) / (input_range_diff);
}

/**
 *
 */
unsigned long rgb(int r, int g, int b)
{
    return (static_cast<unsigned long>(r) << 16) | (static_cast<unsigned long>(g) << 8) | static_cast<unsigned long>(b);
}
