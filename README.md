# options-pricing-calculator

A simple options pricing calculator written in C++

https://pheneas123.github.io/simple-options-pricing-calculator/

## Features

- **European (Black–Scholes)**  
  - Price: closed-form  
  - Greeks: closed-form (Delta, Gamma, Vega, Theta, Rho)

- **Binary (cash-or-nothing)**  
  - Price: closed-form  
  - Greeks: closed-form

- **American options**  
  - Price: CRR binomial with early exercise  
  - Greeks: finite differences

- Handles dividend yield `q` and continuous compounding.

## Calculations

- For European vanillas, formulas are straight from Black–Scholes.  
- For binaries and Americans, Greeks are estimated with finite differences:  
  - Central differences for Delta, Vega, Rho  
  - Second-order stencil for Gamma  
  - Theta uses the market convention (negative of ∂Price/∂T)  

## Plans
- Finish maths pdf with American options
- Implement analytical Greek calculations
- Improve FD mode with confidence intervals
