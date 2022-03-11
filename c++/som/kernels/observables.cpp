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

#include <memory>

#include <nda/nda.hpp>
#include <nda/mpi.hpp>

#include <triqs/utility/exceptions.hpp>

#include "observables.hpp"

namespace som {

using namespace triqs::gfs;

bool is_stat_relevant(observable_kind kind) { return kind != ZeroTemp; }

statistic_enum observable_statistics(observable_kind kind) {
  switch(kind) {
    case FermionGf: return Fermion;
    case BosonCorr: return Boson;
    case BosonAutoCorr: return Boson;
    default:
      TRIQS_RUNTIME_ERROR << "observable_statistics: Statistics is undefined "
                             "for this observable kind";
  }
}

std::string observable_name(observable_kind kind) {
  switch(kind) {
    case FermionGf: return "Green's functions";
    case BosonCorr: return "bosonic correlator";
    case BosonAutoCorr: return "bosonic autocorrelator";
    case ZeroTemp: return "correlator at zero temperature";
    default: TRIQS_RUNTIME_ERROR << "observable_name: Invalid observable kind";
  }
}

std::pair<double, double> max_energy_window(observable_kind kind) {
  switch(kind) {
    case FermionGf: return std::make_pair(-HUGE_VAL, HUGE_VAL);
    case BosonCorr: return std::make_pair(-HUGE_VAL, HUGE_VAL);
    case BosonAutoCorr: return std::make_pair(0, HUGE_VAL);
    case ZeroTemp: return std::make_pair(0, HUGE_VAL);
    default:
      TRIQS_RUNTIME_ERROR << "max_energy_window: Invalid observable kind";
  }
}

// Construct a real-frequency GF from a configuration
void back_transform(observable_kind kind, configuration const& conf,
                    gf_view<refreq, scalar_valued> g_w,
                    bool with_binning,
                    mpi::communicator const& comm) {
  bool bosoncorr = kind == BosonCorr || kind == BosonAutoCorr;

  auto const& e_mesh = g_w.mesh();
  auto & data = g_w.data();
  data() = 0;

  auto add_to_data = [&](rectangle const& rect) {
    for(auto e : e_mesh)
      data(e.index()) += rect.hilbert_transform(double(e), bosoncorr);
  };

  auto add_to_data_with_binning = [&](rectangle const& rect) {
    double de = e_mesh.delta();
    for(auto e : e_mesh) {
      double ea = e.index() == 0 ?
                  double(e) : (double(e) - de / 2);
      double eb = (e.index() == e_mesh.size() - 1) ?
                  double(e) : (double(e) + de / 2);
      data(e.index()) += rect.averaged_hilbert_transform(ea, eb, bosoncorr);
    }
  };

  std::unique_ptr<rectangle> reflected_rect;
  if(kind == BosonAutoCorr)
    reflected_rect = std::make_unique<rectangle>(0, 0, 0);

  long rect_index = 0;
  for(auto const& rect : conf) {
    if((rect_index % comm.size()) != comm.rank()) {
      ++rect_index;
      continue;
    }

    if(with_binning)
      add_to_data_with_binning(rect);
    else
      add_to_data(rect);

    // Add the reflected rectangle as needed
    if(reflected_rect) {
      reflected_rect->center = -rect.center;
      reflected_rect->width = rect.width;
      reflected_rect->height = rect.height;
      if(with_binning)
        add_to_data_with_binning(*reflected_rect);
      else
        add_to_data(*reflected_rect);
    }

    ++rect_index;
  }

  if(bosoncorr) { data *= -1.0 / M_PI; }

  // A copy is needed here because mpi::all_reduce() does not work
  // with non-contiguous views
  array<std::complex<double>, 1> d(data);
  d = mpi::all_reduce(d, comm);
  data() = d;
}

// Compute the GF tail from a configuration
nda::array<std::complex<double>, 1>
compute_tail(observable_kind kind, configuration const& conf,
             int max_order, mpi::communicator const& comm) {
  assert(max_order >= 0);

  bool bosoncorr = kind == BosonCorr || kind == BosonAutoCorr;

  array<std::complex<double>, 1>
    tail = zeros<std::complex<double>>(max_order + 1);

  long rect_index = 0;
  for(auto const& rect : conf) {
    if((rect_index % comm.size()) != comm.rank()) {
      ++rect_index;
      continue;
    }

    tail += rect.tail_coefficients(0, max_order, bosoncorr);

    if(kind == BosonAutoCorr) {
      // Add a reflected rectangle
      rectangle reflected_rect(-rect.center, rect.width, rect.height);
      tail += reflected_rect.tail_coefficients(0, max_order, true);
    }

    ++rect_index;
  }

  if(bosoncorr) tail *= -1.0 / M_PI;

  return mpi::all_reduce(tail, comm);
}

} // namespace som
