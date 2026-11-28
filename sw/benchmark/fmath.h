// Small single-precision math library for the Whetstone benchmark.
//
// Written in plain C so that it is compiled for the same -march as the
// benchmark: with Zfinx it runs on the FPU, without it on libgcc soft-float.
// (The toolchain's libm is soft-float only, so linking it would hide the FPU.)
//
// Accuracy is a few ulp over the argument ranges Whetstone uses; large
// arguments to sin/cos lose accuracy (simple Cody-Waite reduction).
// SPDX-License-Identifier: Apache-2.0

#pragma once

float f_sin(float x);
float f_cos(float x);
float f_atan(float x);
float f_exp(float x);
float f_log(float x);
float f_sqrt(float x);  // correctly rounded (fsqrt.s, or exact integer method)
