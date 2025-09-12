#pragma once
#include <cmath>

namespace bs {

enum class Type { Call = 1, Put = -1 };

struct Result {
  double price;
  double delta;
  double gamma;
  double vega;
  double theta;
  double rho;
};

Result blackScholes(Type type, double S, double K, double r, double q,
                    double sigma, double T);

double normPdf(double x);
double normCdf(double x);

} // namespace bs
