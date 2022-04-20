#ifndef _MAIN_HPP_
#define _MAIN_HPP_
#include <iostream>
#include <fstream>
#include <boost/math/distributions.hpp>
#include <boost/array.hpp>
#include <boost/numeric/odeint.hpp>
#include <random>
#include <vector>
#include <Eigen/Dense>
#include <Eigen/Core>
#include <unsupported/Eigen/MatrixFunctions>
#include <cmath>
#include <chrono>
#include <omp.h>
#include <Eigen/StdVector>
#include <boost/numeric/odeint/external/openmp/openmp.hpp>
#include <experimental/filesystem>
#include "rr/rrRoadRunner.h"
#include "rr/rrExecutableModel.h"
#include "rr/rrException.h"
#include "rr/rrUtils.h"
#include "rr/rrLogger.h"
using Eigen::MatrixXd;
using Eigen::VectorXd;
using namespace std;
using namespace boost;
using namespace boost::math;
using namespace boost::numeric::odeint;
using namespace rr;
using namespace ls;
namespace fs = std::experimental::filesystem;
#endif