#include "bs.hpp" // comes from src/, via CMake include dirs
#include <emscripten/bind.h>

using namespace emscripten;

EMSCRIPTEN_BINDINGS(bs_module) {
  enum_<bs::Type>("Type")
      .value("Call", bs::Type::Call)
      .value("Put", bs::Type::Put);

  value_object<bs::Result>("Result")
      .field("price", &bs::Result::price)
      .field("delta", &bs::Result::delta)
      .field("gamma", &bs::Result::gamma)
      .field("vega", &bs::Result::vega)
      .field("theta", &bs::Result::theta)
      .field("rho", &bs::Result::rho);

  function("black_scholes", &bs::blackScholes);
}
