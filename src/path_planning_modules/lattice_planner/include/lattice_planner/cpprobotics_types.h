#ifndef _CPPROBOTICS_TYPES_H
#define _CPPROBOTICS_TYPES_H

#include <array>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

// namespace zww {

using Vec_f = std::vector<float>;
using Poi_f = std::array<float, 2>;
using Vec_Poi = std::vector<Poi_f>;

struct FrenetInitialConditions {
  double s0;
  double c_speed;
  double c_d;
  double c_d_d;
  double c_d_dd;
  double target_speed;
  //   double *wx;
  //   double *wy;
  //   int nw;
  //   double *o_llx;
  //   double *o_lly;
  //   double *o_urx;
  //   double *o_ury;
  //   int no;
};

// };  // namespace zww

#endif
