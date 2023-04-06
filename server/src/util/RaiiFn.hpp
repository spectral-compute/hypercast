#pragma once

#include <functional>

namespace Util
{

/**
 * This class runs a function when it goes out of scope.
 */
class RaiiFn final
{
public:
    /**
     * Call the function given to the constructor.
     */
    ~RaiiFn()
    {
        fn();
    }

    /**
     * Run the given function when this object goes out of scope.
     */
    RaiiFn(std::move_only_function<void ()> fn) : fn(std::move(fn)) {}

private:
    std::move_only_function<void ()> fn;
};

} // namespace Util
