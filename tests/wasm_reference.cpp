#include "bs.hpp"
#include <iomanip>
#include <iostream>

int main() {
  std::cout << std::setprecision(17);
  for (double spot : {80.0, 100.0, 120.0}) {
    for (auto type : {bs::Type::Call, bs::Type::Put}) {
      for (int model = 0; model < 3; ++model) {
        const auto result =
            model == 0 ? bs::blackScholes(type, spot, 100, 0.05, 0.02, 0.2, 1)
            : model == 1
                ? bs::binaryCashOrNothing(type, spot, 100, 0.05, 0.02, 0.2, 1,
                                          1)
                : bs::americanOption(type, spot, 100, 0.05, 0.02, 0.2, 1, 300);
        std::cout << "{\"model\":" << model
                  << ",\"call\":" << (type == bs::Type::Call ? "true" : "false")
                  << ",\"spot\":" << spot << ",\"values\":[" << result.price
                  << ',' << result.delta << ',' << result.gamma << ','
                  << result.vega << ',' << result.theta << ',' << result.rho
                  << "]}\n";
      }
    }
  }
}
