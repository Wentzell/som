/*******************************************************************************
 *
 * SOM: Stochastic Optimization Method for Analytic Continuation
 *
 * Copyright (C) 2016-2022 Igor Krivenko <igor.s.krivenko@gmail.com>
 *
 * SOM is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * SOM is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SOM. If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************************/
#include <h5/h5.hpp>
#include <nda/gtest_tools.hpp>

#include <som/kernels/fermiongf_imtime.hpp>
#include <som/solution_worker.hpp>

using namespace som;
using namespace nda;
using triqs::utility::clock_callback;

double beta;
triqs::mesh::imtime mesh;
array<double, 1> g_tau, s_tau;

struct Env : public ::testing::Environment {
  virtual void SetUp() override {
    h5::file arch("solution_worker.ref.h5", 'r');

    h5_read(arch, "beta", beta);
    h5_read(arch, "g_tau", g_tau);
    h5_read(arch, "s_tau", s_tau);

    mesh = {beta, triqs::mesh::Fermion, static_cast<int>(first_dim(g_tau))};
  }
};
::testing::Environment* const env =
    ::testing::AddGlobalTestEnvironment(new Env);

using obj_function = objective_function<kernel<FermionGf, triqs::mesh::imtime>>;

TEST(solution_worker, RandomConfig) {
  cache_index ci;
  kernel<FermionGf, triqs::mesh::imtime> kern(mesh);
  obj_function of(kern, g_tau, s_tau);

  auto params = worker_parameters_t({-3.0, 3.0});
  params.random_seed = 963162;
  params.t = 100;
  params.min_rect_width = 0.001;
  params.min_rect_weight = 0.001;

  solution_worker<kernel<FermionGf, triqs::mesh::imtime>> worker(
      of, 1.0, ci, params, clock_callback(-1), 10);

  auto solution = worker(10);

  configuration solution_ref(ci);
  h5::file arch("solution_worker.ref.h5", 'r');
  h5_read(arch, "RandomConfig_output", solution_ref);

  EXPECT_EQ(solution_ref, solution);
}

TEST(solution_worker, StartConfig) {
  cache_index ci;
  kernel<FermionGf, triqs::mesh::imtime> kern(mesh);
  obj_function of(kern, g_tau, s_tau);

  auto params = worker_parameters_t({-3.0, 3.0});
  params.random_seed = 963162;
  params.t = 100;
  params.min_rect_width = 0.001;
  params.min_rect_weight = 0.001;

  solution_worker<kernel<FermionGf, triqs::mesh::imtime>> worker(
      of, 1.0, ci, params, clock_callback(-1), 10);

  h5::file arch("solution_worker.ref.h5", 'r');
  configuration init_config(ci);
  h5_read(arch, "StartConfig_input", init_config);

  auto solution = worker(init_config);

  configuration solution_ref(ci);
  h5_read(arch, "StartConfig_output", solution_ref);

  EXPECT_EQ(solution_ref, solution);
}
