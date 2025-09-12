#include "bs.hpp"
#include <cmath>

namespace bs {

double normPdf(double x) {
  static constexpr double INV_SQRT_2PI{0.39894228040143267794};
  return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

double normCdf(double x) {
  static constexpr double SQRT_2{1.41421356237309504880};
  return 0.5 * std::erfc(-x / SQRT_2);
}

Result blackScholes(Type type, double S, double K, double r, double q,
                    double sigma, double T) {
  if (!(S > 0.0) || !(K > 0.0) || !(sigma > 0.0) || !(T > 0.0)) {
    return {NAN, NAN, NAN, NAN, NAN, NAN};
  }

  const double sqrtT = std::sqrt(T);
  const double sigmaSqrtT = sigma * sqrtT;
  const double d1 =
      (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / sigmaSqrtT;
  const double d2 = d1 - sigmaSqrtT;
  const double Nd1 = normCdf(d1);
  const double Nd2 = normCdf(d2);
  const double Nmd1 = normCdf(-d1);
  const double Nmd2 = normCdf(-d2);
  const double phi1 = normPdf(d1);
  const double discountedR = std::exp(-r * T);
  const double discountedQ = std::exp(-q * T);
  const bool isCall = (type == Type::Call);
  const double cp = isCall ? 1.0 : -1.0;

  // Price calculation
  const double price = isCall ? S * discountedQ * Nd1 - K * discountedR * Nd2
                              : K * discountedR * Nmd2 - S * discountedQ * Nmd1;

  // Greeks
  const double delta = isCall ? discountedQ * Nd1 : discountedQ * (Nd1 - 1.0);
  const double gamma = discountedQ * phi1 / (S * sigmaSqrtT);
  const double vega = S * discountedQ * phi1 * sqrtT;

  // Theta
  const double thetaCommon = -(S * discountedQ * phi1 * sigma) / (2.0 * sqrtT);
  const double theta = isCall ? thetaCommon - r * K * discountedR * Nd2 +
                                    q * S * discountedQ * Nd1
                              : thetaCommon + r * K * discountedR * Nmd2 -
                                    q * S * discountedQ * Nmd1;

  // Rho
  const double rho =
      isCall ? K * T * discountedR * Nd2 : -K * T * discountedR * Nmd2;

  return {price, delta, gamma, vega, theta, rho};
}
} // namespace bs
