/*******************************************************************************
 *
 * SOM: Stochastic Optimization Method for Analytic Continuation
 *
 * Copyright (C) 2016-2021 Igor Krivenko <igor.s.krivenko@gmail.com>
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

#include <boost/preprocessor/seq/for_each.hpp>

#include <som/kernels/all.hpp>

#include "som_core.hpp"
#include "common.hxx"

namespace som {

using namespace triqs::gfs;

///////////////////////////////////////
// triqs_gf_view_assign_delegation() //
///////////////////////////////////////

void fill_data(gf_view<imtime> g_tau, int i, vector<double> const& data) {
  g_tau.data()(range(), i, i) = data;
}

void fill_data(gf_view<imfreq> g_iw, int i, vector<dcomplex> const& data) {
  auto g_positive_freq = positive_freq_view(g_iw);
  g_positive_freq.data()(range(), i, i) = data;
  g_iw = make_gf_from_real_gf(make_const_view(g_positive_freq));
}

void fill_data(gf_view<legendre> g_l, int i, vector<double> const& data) {
  g_l.data()(range(), i, i) = data;
}

template <typename MeshType>
void triqs_gf_view_assign_delegation(gf_view<MeshType> g,
                                     som_core const& cont) {
  auto gf_dim = cont.data.size();
  check_gf_dim(g, gf_dim);
  if(is_stat_relevant(cont.kind))
    check_gf_stat(g, observable_statistics(cont.kind));

  g() = 0;
#define FILL_DATA_CASE(r, d, ok)                                               \
  case ok: {                                                                   \
    kernel<ok, MeshType> kern(g.mesh());                                       \
    for(int i : range(gf_dim)) {                                               \
      fill_data(g, i, kern(cont.data[i].final_solution));                      \
    }                                                                          \
    return;                                                                    \
  }
  switch(cont.kind) {
    BOOST_PP_SEQ_FOR_EACH(FILL_DATA_CASE, _, ALL_OBSERVABLES)
    default: fatal_error("unknown observable kind " + to_string(cont.kind));
  }
#undef FILL_DATA_CASE
}

template <>
void triqs_gf_view_assign_delegation<refreq>(gf_view<refreq> g_w,
                                             som_core const& cont) {
  auto gf_dim = cont.data.size();
  check_gf_dim(g_w, gf_dim);
  for(int i : range(gf_dim)) {
    back_transform(cont.kind,
                   cont.data[i].final_solution,
                   const_cast<som_core&>(cont).ci,
                   slice_target_to_scalar(g_w, i, i),
                   const_cast<som_core&>(cont).comm
                   );
  }
}

template void triqs_gf_view_assign_delegation<imtime>(gf_view<imtime>,
                                                      som_core const&);
template void triqs_gf_view_assign_delegation<imfreq>(gf_view<imfreq>,
                                                      som_core const&);
template void triqs_gf_view_assign_delegation<legendre>(gf_view<legendre>,
                                                        som_core const&);

//////////////////////////////
// som_core::compute_tail() //
//////////////////////////////

array<dcomplex, 3> som_core::compute_tail(int max_order) const {
  auto gf_dim = data.size();
  array<dcomplex, 3> tail(max_order + 1, gf_dim, gf_dim);
  for(int i : range(gf_dim)) {
    tail(range(), i, i) =
        som::compute_tail(kind, data[i].final_solution,
                          const_cast<som_core*>(this)->ci,
                          const_cast<som_core*>(this)->comm,
                          max_order);
  }
  return tail;
}

} // namespace som
